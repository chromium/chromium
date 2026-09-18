// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/writer.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/cbor/cbor_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

/* Leveraging RFC 7049 examples from
   https://github.com/cbor/test-vectors/blob/master/appendix_a.json. */
namespace cbor {

class CBORWriterTest : public testing::TestWithParam<bool> {
 protected:
  std::optional<std::vector<uint8_t>> DoWrite(
      const Value& node,
      size_t max_nesting_level = Writer::kDefaultMaxNestingDepth) {
    Writer::Config config;
    config.max_nesting_level = base::checked_cast<int>(max_nesting_level);
    config.use_rust = GetParam();
    return Writer::Write(node, config);
  }

  std::optional<std::vector<uint8_t>> DoWrite(const Value& node,
                                              Writer::Config config) {
    config.use_rust = GetParam();
    return Writer::Write(node, config);
  }
};

TEST_P(CBORWriterTest, TestWriteUint) {
  struct UintTestCase {
    const int64_t value;
    const std::string_view cbor;
  };

  static const UintTestCase kUintTestCases[] = {
      // Reminder: must specify length when creating string pieces
      // with null bytes, else the string will truncate prematurely.
      {0, std::string_view("\x00", 1)},
      {1, std::string_view("\x01")},
      {10, std::string_view("\x0a")},
      {23, std::string_view("\x17")},
      {24, std::string_view("\x18\x18")},
      {25, std::string_view("\x18\x19")},
      {100, std::string_view("\x18\x64")},
      {1000, std::string_view("\x19\x03\xe8")},
      {1000000, std::string_view("\x1a\x00\x0f\x42\x40", 5)},
      {0xFFFFFFFF, std::string_view("\x1a\xff\xff\xff\xff")},
      {0x100000000,
       std::string_view("\x1b\x00\x00\x00\x01\x00\x00\x00\x00", 9)},
      {std::numeric_limits<int64_t>::max(),
       std::string_view("\x1b\x7f\xff\xff\xff\xff\xff\xff\xff")}};

  for (const UintTestCase& test_case : kUintTestCases) {
    auto cbor = DoWrite(Value(test_case.value));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST_P(CBORWriterTest, TestWriteNegativeInteger) {
  static const struct {
    const int64_t negative_int;
    const std::string_view cbor;
  } kNegativeIntTestCases[] = {
      {-1LL, std::string_view("\x20")},
      {-10LL, std::string_view("\x29")},
      {-23LL, std::string_view("\x36")},
      {-24LL, std::string_view("\x37")},
      {-25LL, std::string_view("\x38\x18")},
      {-100LL, std::string_view("\x38\x63")},
      {-1000LL, std::string_view("\x39\x03\xe7")},
      {-4294967296LL, std::string_view("\x3a\xff\xff\xff\xff")},
      {-4294967297LL,
       std::string_view("\x3b\x00\x00\x00\x01\x00\x00\x00\x00", 9)},
      {std::numeric_limits<int64_t>::min(),
       std::string_view("\x3b\x7f\xff\xff\xff\xff\xff\xff\xff")},
  };

  for (const auto& test_case : kNegativeIntTestCases) {
    SCOPED_TRACE(testing::Message() << "testing  negative int at index: "
                                    << test_case.negative_int);

    auto cbor = DoWrite(Value(test_case.negative_int));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST_P(CBORWriterTest, TestWriteBytes) {
  struct BytesTestCase {
    const std::vector<uint8_t> bytes;
    const std::string_view cbor;
  };

  static const BytesTestCase kBytesTestCases[] = {
      {{}, std::string_view("\x40")},
      {{0x01, 0x02, 0x03, 0x04}, std::string_view("\x44\x01\x02\x03\x04")},
  };

  for (const BytesTestCase& test_case : kBytesTestCases) {
    auto cbor = DoWrite(Value(test_case.bytes));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST_P(CBORWriterTest, TestWriteString) {
  struct StringTestCase {
    const std::string string;
    const std::string_view cbor;
  };

  static const StringTestCase kStringTestCases[] = {
      {"", std::string_view("\x60")},
      {"a", std::string_view("\x61\x61")},
      {"IETF", std::string_view("\x64\x49\x45\x54\x46")},
      {"\"\\", std::string_view("\x62\x22\x5c")},
      {"\xc3\xbc", std::string_view("\x62\xc3\xbc")},
      {"\xe6\xb0\xb4", std::string_view("\x63\xe6\xb0\xb4")},
      {"\xf0\x90\x85\x91", std::string_view("\x64\xf0\x90\x85\x91")}};

  for (const StringTestCase& test_case : kStringTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "testing encoding string : " << test_case.string);

    auto cbor = DoWrite(Value(test_case.string));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST_P(CBORWriterTest, TestWriteArray) {
  static const uint8_t kArrayTestCaseCbor[] = {
      // clang-format off
      0x98, 0x19,  // array of 25 elements
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x18, 0x18, 0x19,
      // clang-format on
  };
  std::vector<Value> array;
  for (int64_t i = 1; i <= 25; i++) {
    array.push_back(Value(i));
  }
  auto cbor = DoWrite(Value(array));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kArrayTestCaseCbor,
                                        std::size(kArrayTestCaseCbor)));
}

TEST_P(CBORWriterTest, TestWriteMap) {
  static const uint8_t kMapTestCaseCbor[] = {
      // clang-format off
      0xb8, 0x19, // map of 25 pairs:
        0x00,          // key 0
        0x61, 0x61,    // value "a"

        0x17,          // key 23
        0x61,  0x62,   // value "b"

        0x18, 0x18,    // key 24
        0x61, 0x63,  // value "c"

        0x18, 0xFF,        // key 255
        0x61,  0x64,       // value "d"

        0x19, 0x01, 0x00,  // key 256
        0x61, 0x65,        // value "e"

        0x19, 0xFF, 0xFF,  // key 65535
        0x61,  0x66,       // value "f"

        0x1A, 0x00, 0x01, 0x00, 0x00,   // key 65536
        0x61, 0x67,                     // value "g"

        0x1A, 0xFF, 0xFF, 0xFF, 0xFF,   // key 4294967295
        0x61, 0x68,                     // value "h"

        // key 4294967296
        0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x61, 0x69,  //  value "i"

        // key INT64_MAX
        0x1b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x61, 0x6a,  //  value "j"

        0x20,          // key -1
        0x61, 0x6b,    // value "k"

        0x37,          // key -24
        0x61,  0x6c,   // value "l"

        0x38, 0x18,    // key -25
        0x61, 0x6d,  // value "m"

        0x38, 0xFF,        // key -256
        0x61, 0x6e,       // value "n"

        0x39, 0x01, 0x00,  // key -257
        0x61, 0x6f,        // value "o"

        0x3A, 0x00, 0x01, 0x00, 0x00,   // key -65537
        0x61, 0x70,                     // value "p"

        0x3A, 0xFF, 0xFF, 0xFF, 0xFF,   // key -4294967296
        0x61, 0x71,                     // value "q"

        // key -4294967297
        0x3B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x61, 0x72,  //  value "r"

        // key INT64_MIN
        0x3b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x61, 0x73,  //  value "s"

        0x41, 'a', // byte string "a"
        0x02,

        0x43, 'b', 'a', 'r', // byte string "bar"
        0x03,

        0x43, 'f', 'o', 'o', // byte string "foo"
        0x04,

        0x60,        // key ""
        0x61, 0x2e,  // value "."

        0x61, 0x65,  // key "e"
        0x61, 0x45,  // value "E"

        0x62, 0x61, 0x61,  // key "aa"
        0x62, 0x41, 0x41,  // value "AA"
      // clang-format on
  };
  Value::MapValue map;
  // Shorter strings sort first in CTAP, thus the “aa” value should be
  // serialised last in the map.
  map[Value("aa")] = Value("AA");
  map[Value("e")] = Value("E");
  // The empty string is shorter than all others, so should appear first among
  // the strings.
  map[Value("")] = Value(".");
  // Map keys are sorted by major type, by byte length, and then by
  // byte-wise lexical order. So all integer type keys should appear before
  // key "" and all positive integer keys should appear before negative integer
  // keys.
  map[Value(-1)] = Value("k");
  map[Value(-24)] = Value("l");
  map[Value(-25)] = Value("m");
  map[Value(-256)] = Value("n");
  map[Value(-257)] = Value("o");
  map[Value(-65537)] = Value("p");
  map[Value(int64_t(-4294967296))] = Value("q");
  map[Value(int64_t(-4294967297))] = Value("r");
  map[Value(std::numeric_limits<int64_t>::min())] = Value("s");
  map[Value(Value::BinaryValue{'a'})] = Value(2);
  map[Value(Value::BinaryValue{'b', 'a', 'r'})] = Value(3);
  map[Value(Value::BinaryValue{'f', 'o', 'o'})] = Value(4);
  map[Value(0)] = Value("a");
  map[Value(23)] = Value("b");
  map[Value(24)] = Value("c");
  map[Value(std::numeric_limits<uint8_t>::max())] = Value("d");
  map[Value(256)] = Value("e");
  map[Value(std::numeric_limits<uint16_t>::max())] = Value("f");
  map[Value(65536)] = Value("g");
  map[Value(int64_t(std::numeric_limits<uint32_t>::max()))] = Value("h");
  map[Value(int64_t(4294967296))] = Value("i");
  map[Value(std::numeric_limits<int64_t>::max())] = Value("j");
  auto cbor = DoWrite(Value(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(), testing::ElementsAreArray(
                                kMapTestCaseCbor, std::size(kMapTestCaseCbor)));
}

TEST_P(CBORWriterTest, TestWriteMapWithArray) {
  static const uint8_t kMapArrayTestCaseCbor[] = {
      // clang-format off
      0xa2,  // map of 2 pairs
        0x61, 0x61,  // "a"
        0x01,

        0x61, 0x62,  // "b"
        0x82,        // array with 2 elements
          0x02,
          0x03,
      // clang-format on
  };
  Value::MapValue map;
  map[Value("a")] = Value(1);
  Value::ArrayValue array;
  array.push_back(Value(2));
  array.push_back(Value(3));
  map[Value("b")] = Value(array);
  auto cbor = DoWrite(Value(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kMapArrayTestCaseCbor,
                                        std::size(kMapArrayTestCaseCbor)));
}

TEST_P(CBORWriterTest, TestWriteNestedMap) {
  static const uint8_t kNestedMapTestCase[] = {
      // clang-format off
      0xa2,  // map of 2 pairs
        0x61, 0x61,  // "a"
        0x01,

        0x61, 0x62,  // "b"
        0xa2,        // map of 2 pairs
          0x61, 0x63,  // "c"
          0x02,

          0x61, 0x64,  // "d"
          0x03,
      // clang-format on
  };
  Value::MapValue map;
  map[Value("a")] = Value(1);
  Value::MapValue nested_map;
  nested_map[Value("c")] = Value(2);
  nested_map[Value("d")] = Value(3);
  map[Value("b")] = Value(nested_map);
  auto cbor = DoWrite(Value(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kNestedMapTestCase,
                                        std::size(kNestedMapTestCase)));
}

TEST_P(CBORWriterTest, TestSignedExchangeExample) {
  // Example adopted from:
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
  static const uint8_t kSignedExchangeExample[] = {
      // clang-format off
      0xa5, // map of 5 pairs
        0x0a, // 10
        0x01,

        0x18, 0x64, // 100
        0x02,

        0x20, // -1
        0x03,

        0x61, 'z', // text string "z"
        0x04,

        0x62, 'a', 'a', // text string "aa"
        0x05,

      /*
        0x81, 0x18, 0x64, // [100] (array as map key is not yet supported)
        0x06,

        0x81, 0x20,  // [-1] (array as map key is not yet supported)
        0x07,

        0xf4, // false (boolean  as map key is not yet supported)
        0x08,
      */
      // clang-format on
  };
  Value::MapValue map;
  map[Value(10)] = Value(1);
  map[Value(100)] = Value(2);
  map[Value(-1)] = Value(3);
  map[Value("z")] = Value(4);
  map[Value("aa")] = Value(5);

  auto cbor = DoWrite(Value(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kSignedExchangeExample,
                                        std::size(kSignedExchangeExample)));
}

TEST_P(CBORWriterTest, TestWriteSimpleValue) {
  static const struct {
    Value::SimpleValue simple_value;
    const std::string_view cbor;
  } kSimpleTestCase[] = {
      {Value::SimpleValue::FALSE_VALUE, std::string_view("\xf4")},
      {Value::SimpleValue::TRUE_VALUE, std::string_view("\xf5")},
      {Value::SimpleValue::NULL_VALUE, std::string_view("\xf6")},
      {Value::SimpleValue::UNDEFINED, std::string_view("\xf7")}};

  for (const auto& test_case : kSimpleTestCase) {
    auto cbor = DoWrite(Value(test_case.simple_value));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST_P(CBORWriterTest, TestWriteNoneFails) {
  EXPECT_FALSE(DoWrite(Value()).has_value());

  Value::ArrayValue array;
  array.emplace_back(1);
  array.emplace_back();
  EXPECT_FALSE(DoWrite(Value(std::move(array))).has_value());

  Value::MapValue map;
  map[Value(1)] = Value();
  EXPECT_FALSE(DoWrite(Value(std::move(map))).has_value());

  Value::MapValue map_with_none_key;
  map_with_none_key.emplace(Value(), Value(1));
  EXPECT_FALSE(DoWrite(Value(std::move(map_with_none_key))).has_value());
}

TEST_P(CBORWriterTest, TestWriteInvalidUtf8) {
  Writer::Config config;
  config.allow_invalid_utf8_for_testing = true;

  Value invalid_str = Value::InvalidUTF8StringValueForTesting("\xff\xfe");
  auto cbor = DoWrite(invalid_str, config);
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(std::string_view("\x62\xff\xfe")));

  // Verify canonical sorting of INVALID_UTF8 map keys relative to integer and
  // valid UTF-8 string keys.
  Value::MapValue map;
  map[Value("\xc3\xa9")] = Value(6);
  map[Value::InvalidUTF8StringValueForTesting("\x80\x80")] = Value(5);
  map[Value("bb")] = Value(4);
  map[Value::InvalidUTF8StringValueForTesting("\xff")] =
      Value::InvalidUTF8StringValueForTesting("\xfe");
  map[Value("a")] = Value(2);
  map[Value(1)] = Value("int_key");
  auto map_cbor = DoWrite(Value(map), config);
  ASSERT_TRUE(map_cbor.has_value());
  static const uint8_t kExpectedMapCbor[] = {
      0xa6,                                       // map of 6 pairs
      0x01,                                       // key 1: unsigned int 1
      0x67, 'i',  'n',  't', '_', 'k', 'e', 'y',  // val 1: "int_key"
      0x61, 'a',                                  // key 2: "a" (len 1, 0x61)
      0x02,                                       // val 2: 2
      0x61, 0xff,                                 // key 3: "\xff" (len 1, 0xff)
      0x61, 0xfe,                                 // val 3: "\xfe"
      0x62, 'b',  'b',                            // key 4: "bb" (len 2, 0x62)
      0x04,                                       // val 4: 4
      0x62, 0x80, 0x80,                           // key 5: "\x80\x80" (len 2)
      0x05,                                       // val 5: 5
      0x62, 0xc3, 0xa9,                           // key 6: "\xc3\xa9" (len 2)
      0x06,                                       // val 6: 6
  };
  EXPECT_THAT(
      map_cbor.value(),
      testing::ElementsAreArray(kExpectedMapCbor, std::size(kExpectedMapCbor)));
}

// For major type 0, 2, 3, empty CBOR array, and empty CBOR map, the nesting
// depth is expected to be 0 since the CBOR decoder does not need to parse
// any nested CBOR value elements.
TEST_P(CBORWriterTest, TestWriteSingleLayer) {
  const Value simple_uint = Value(1);
  const Value simple_string = Value("a");
  const std::vector<uint8_t> byte_data = {0x01, 0x02, 0x03, 0x04};
  const Value simple_bytestring = Value(byte_data);
  Value::ArrayValue empty_cbor_array;
  Value::MapValue empty_cbor_map;
  const Value empty_array_value = Value(empty_cbor_array);
  const Value empty_map_value = Value(empty_cbor_map);
  Value::ArrayValue simple_array;
  simple_array.push_back(Value(2));
  Value::MapValue simple_map;
  simple_map[Value("b")] = Value(3);
  const Value single_layer_cbor_map = Value(simple_map);
  const Value single_layer_cbor_array = Value(simple_array);

  EXPECT_TRUE(DoWrite(simple_uint, 0).has_value());
  EXPECT_TRUE(DoWrite(simple_string, 0).has_value());
  EXPECT_TRUE(DoWrite(simple_bytestring, 0).has_value());

  EXPECT_TRUE(DoWrite(empty_array_value, 0).has_value());
  EXPECT_TRUE(DoWrite(empty_map_value, 0).has_value());

  EXPECT_FALSE(DoWrite(single_layer_cbor_array, 0).has_value());
  EXPECT_TRUE(DoWrite(single_layer_cbor_array, 1).has_value());

  EXPECT_FALSE(DoWrite(single_layer_cbor_map, 0).has_value());
  EXPECT_TRUE(DoWrite(single_layer_cbor_map, 1).has_value());
}

// Major type 5 nested CBOR map value with following structure.
//     {"a": 1,
//      "b": {"c": 2,
//            "d": 3}}
TEST_P(CBORWriterTest, NestedMaps) {
  Value::MapValue cbor_map;
  cbor_map[Value("a")] = Value(1);
  Value::MapValue nested_map;
  nested_map[Value("c")] = Value(2);
  nested_map[Value("d")] = Value(3);
  cbor_map[Value("b")] = Value(nested_map);
  EXPECT_TRUE(DoWrite(Value(cbor_map), 2).has_value());
  EXPECT_FALSE(DoWrite(Value(cbor_map), 1).has_value());
}

// Testing Write() function for following CBOR structure with depth of 3.
//     [1,
//      2,
//      3,
//      {"a": 1,
//       "b": {"c": 2,
//             "d": 3}}]
TEST_P(CBORWriterTest, UnbalancedNestedContainers) {
  Value::ArrayValue cbor_array;
  Value::MapValue cbor_map;
  Value::MapValue nested_map;

  cbor_map[Value("a")] = Value(1);
  nested_map[Value("c")] = Value(2);
  nested_map[Value("d")] = Value(3);
  cbor_map[Value("b")] = Value(nested_map);
  cbor_array.push_back(Value(1));
  cbor_array.push_back(Value(2));
  cbor_array.push_back(Value(3));
  cbor_array.push_back(Value(cbor_map));

  EXPECT_TRUE(DoWrite(Value(cbor_array), 3).has_value());
  EXPECT_FALSE(DoWrite(Value(cbor_array), 2).has_value());
}

// Testing Write() function for following CBOR structure.
//     {"a": 1,
//      "b": {"c": 2,
//            "d": 3
//            "h": { "e": 4,
//                   "f": 5,
//                   "g": [6, 7, [8]]}}}
// Since above CBOR contains 5 nesting levels. Thus, Write() is expected to
// return empty optional object when maximum nesting layer size is set to 4.
TEST_P(CBORWriterTest, OverlyNestedCBOR) {
  Value::MapValue map;
  Value::MapValue nested_map;
  Value::MapValue inner_nested_map;
  Value::ArrayValue inner_array;
  Value::ArrayValue array;

  map[Value("a")] = Value(1);
  nested_map[Value("c")] = Value(2);
  nested_map[Value("d")] = Value(3);
  inner_nested_map[Value("e")] = Value(4);
  inner_nested_map[Value("f")] = Value(5);
  inner_array.push_back(Value(6));
  array.push_back(Value(6));
  array.push_back(Value(7));
  array.push_back(Value(inner_array));
  inner_nested_map[Value("g")] = Value(array);
  nested_map[Value("h")] = Value(inner_nested_map);
  map[Value("b")] = Value(nested_map);

  EXPECT_TRUE(DoWrite(Value(map), 5).has_value());
  EXPECT_FALSE(DoWrite(Value(map), 4).has_value());
}

#if BUILDFLAG(USE_CBOR_RUST)

namespace {

// `CBOR.Write.Duration` is only emitted on clients whose clock can measure it.
int ExpectedDurationCount() {
  return base::TimeTicks::IsHighResolution() ? 1 : 0;
}

}  // namespace

TEST_P(CBORWriterTest, MetricsRecordedOnSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kUseRustCborWriter, GetParam());

  base::HistogramTester histograms;

  Writer::Config config;
  std::optional<std::vector<uint8_t>> cbor = Writer::Write(Value(1), config);
  ASSERT_TRUE(cbor.has_value());

  histograms.ExpectUniqueSample("CBOR.Write.Success", true, 1);
  histograms.ExpectTotalCount("CBOR.Write.Duration", ExpectedDurationCount());
  histograms.ExpectUniqueSample("CBOR.Write.Size", cbor->size(), 1);
}

TEST_P(CBORWriterTest, MetricsRecordedOnFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kUseRustCborWriter, GetParam());

  base::HistogramTester histograms;

  Writer::Config config;
  EXPECT_FALSE(Writer::Write(Value(), config).has_value());

  histograms.ExpectUniqueSample("CBOR.Write.Success", false, 1);
  histograms.ExpectTotalCount("CBOR.Write.Duration", ExpectedDurationCount());
  histograms.ExpectTotalCount("CBOR.Write.Size", 0);
}

TEST_P(CBORWriterTest, MetricsNotRecordedWhenWriterIsSelectedExplicitly) {
  // Explicit `Config::use_rust` selection opts out of metrics even when it
  // matches the feature state.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kUseRustCborWriter, GetParam());

  base::HistogramTester histograms;

  // `DoWrite()` always sets `Config::use_rust`.
  ASSERT_TRUE(DoWrite(Value(1)).has_value());

  histograms.ExpectTotalCount("CBOR.Write.Success", 0);
  histograms.ExpectTotalCount("CBOR.Write.Duration", 0);
  histograms.ExpectTotalCount("CBOR.Write.Size", 0);
}

#else

TEST_P(CBORWriterTest, MetricsNotRecordedWithoutRustWriter) {
  // Non-Rust builds never participate in the experiment.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kUseRustCborWriter);

  base::HistogramTester histograms;

  Writer::Config config;
  ASSERT_TRUE(Writer::Write(Value(1), config).has_value());

  histograms.ExpectTotalCount("CBOR.Write.Success", 0);
  histograms.ExpectTotalCount("CBOR.Write.Duration", 0);
  histograms.ExpectTotalCount("CBOR.Write.Size", 0);
}

#endif  // BUILDFLAG(USE_CBOR_RUST)

INSTANTIATE_TEST_SUITE_P(,
                         CBORWriterTest,
#if BUILDFLAG(USE_CBOR_RUST)
                         testing::Bool(),
#else
                         testing::Values(false),
#endif
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Rust" : "Cpp";
                         });

}  // namespace cbor
