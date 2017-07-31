/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Unit tests for Solidity's ABI encoder.
 */

#include <functional>
#include <string>
#include <tuple>
#include <boost/test/unit_test.hpp>
#include <libsolidity/interface/Exceptions.h>
#include <test/libsolidity/SolidityExecutionFramework.h>

using namespace std;
using namespace std::placeholders;
using namespace dev::test;

namespace dev
{
namespace solidity
{
namespace test
{

#define REQUIRE_LOG_DATA(DATA) do { \
	BOOST_REQUIRE_EQUAL(m_logs.size(), 1); \
	BOOST_CHECK_EQUAL(m_logs[0].address, m_contractAddress); \
	BOOST_CHECK_EQUAL(toHex(m_logs[0].data), toHex(DATA)); \
} while (false)

BOOST_FIXTURE_TEST_SUITE(ABIEncoderTest, SolidityExecutionFramework)

BOOST_AUTO_TEST_CASE(value_types)
{
	char const* sourceCode = R"(
		contract C {
			event E(uint a, uint16 b, uint24 c, int24 d, bytes3 x);
			function f() {
				bytes6 x = hex"1bababababa2";
				E(10, uint16(uint256(-2)), uint24(0x12121212), int24(int256(-1)), bytes3(x));
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(10, u256(65534), u256(0x121212), u256(-1), string("\x1b\xab\xab")));
}

BOOST_AUTO_TEST_CASE(memory_array_one_dim)
{
	char const* sourceCode = R"(
		contract C {
			event E(uint a, int16[] b, uint c);
			function f() {
				int16[] memory x = new int16[](3);
				assembly {
					for { let i := 0 } lt(i, 3) { i := add(i, 1) } {
						mstore(add(x, mul(add(i, 1), 0x20)), add(0xfffffffe, i))
					}
				}
				E(10, x, 11);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(10, 0x60, 11, 3, u256(-2), u256(-1), u256(0)));
}

BOOST_AUTO_TEST_CASE(memory_array_two_dim)
{
	char const* sourceCode = R"(
		contract C {
			event E(uint a, int16[][2] b, uint c);
			function f() {
				int16[][2] memory x;
				x[0] = new int16[](3);
				x[1] = new int16[](2);
				x[0][0] = 7;
				x[0][1] = int16(0x010203040506);
				x[0][2] = -1;
				x[1][0] = 4;
				x[1][1] = 5;
				E(10, x, 11);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(10, 0x60, 11, 0x40, 0xc0, 3, 7, 0x0506, u256(-1), 2, 4, 5));
}

BOOST_AUTO_TEST_CASE(memory_byte_array)
{
	char const* sourceCode = R"(
		contract C {
			event E(uint a, bytes[] b, uint c);
			function f() {
				bytes[] memory x = new bytes[](2);
				x[0] = "abcabcdefghjklmnopqrsuvwabcdefgijklmnopqrstuwabcdefgijklmnoprstuvw";
				x[1] = "abcdefghijklmnopqrtuvwabcfghijklmnopqstuvwabcdeghijklmopqrstuvw";
				E(10, x, 11);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(
		10, 0x60, 11,
		2, 0x40, 0xc0,
		66, string("abcabcdefghjklmnopqrsuvwabcdefgijklmnopqrstuwabcdefgijklmnoprstuvw"),
		63, string("abcdefghijklmnopqrtuvwabcfghijklmnopqstuvwabcdeghijklmopqrstuvw")
	));
}

BOOST_AUTO_TEST_CASE(storage_byte_array)
{
	char const* sourceCode = R"(
		contract C {
			bytes short;
			bytes long;
			event E(bytes s, bytes l);
			function f() {
				short = "123456789012345678901234567890a";
				long = "ffff123456789012345678901234567890afffffffff123456789012345678901234567890a";
				E(short, long);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(
		0x40, 0x80,
		31, string("123456789012345678901234567890a"),
		75, string("ffff123456789012345678901234567890afffffffff123456789012345678901234567890a")
	));
}

BOOST_AUTO_TEST_CASE(storage_array)
{
	char const* sourceCode = R"(
		contract C {
			address[3] addr;
			event E(address[3] a);
			function f() {
				assembly {
					sstore(0, sub(0, 1))
					sstore(1, sub(0, 2))
					sstore(2, sub(0, 3))
				}
				E(addr);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(u160(-1), u160(-2), u160(-3)));
}

BOOST_AUTO_TEST_CASE(storage_array_dyn)
{
	char const* sourceCode = R"(
		contract C {
			address[] addr;
			event E(address[] a);
			function f() {
				addr.push(1);
				addr.push(2);
				addr.push(3);
				E(addr);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(0x20, 3, u160(1), u160(2), u160(3)));
}

BOOST_AUTO_TEST_CASE(storage_array_compact)
{
	char const* sourceCode = R"(
		contract C {
			int72[] x;
			event E(int72[]);
			function f() {
				x.push(-1);
				x.push(2);
				x.push(-3);
				x.push(4);
				x.push(-5);
				x.push(6);
				x.push(-7);
				x.push(8);
				E(x);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("f()");
	REQUIRE_LOG_DATA(encodeArgs(
		0x20, 8, u256(-1), 2, u256(-3), 4, u256(-5), 6, u256(-7), 8
	));
}

BOOST_AUTO_TEST_CASE(external_function)
{
	char const* sourceCode = R"(
		contract C {
			event E(function(uint) external returns (uint), function(uint) external returns (uint));
			function(uint) external returns (uint) g;
			function f(uint) returns (uint) {
				g = this.f;
				E(this.f, g);
			}
		}
	)";
	compileAndRun(sourceCode);
	string funSig = "f(uint256)";
	callContractFunction(funSig);
	string funType = m_contractAddress.ref().toString() + (FixedHash<4>(dev::keccak256(funSig))).ref().toString();
	REQUIRE_LOG_DATA(encodeArgs(funType, funType));
}

BOOST_AUTO_TEST_SUITE_END()

}
}
} // end namespaces
