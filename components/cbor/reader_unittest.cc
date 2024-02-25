// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/reader.h"

#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

/* Leveraging RFC 7049 examples from
   https://github.com/cbor/test-vectors/blob/master/appendix_a.json. */
namespace cbor {

namespace {

std::vector<uint8_t> WithExtraneousData(base::span<const uint8_t> original) {
  std::vector<uint8_t> ret(original.begin(), original.end());
  // Add a valid one byte long CBOR data item, namely, an unsigned integer
  // with value "1".
  ret.push_back(0x01);
  return ret;
}

}  // namespace

TEST(CBORReaderTest, TestReadUint) {
  struct UintTestCase {
    const int64_t value;
    const std::vector<uint8_t> cbor_data;
  };

  static const UintTestCase kUintTestCases[] = {
      {0, {0x00}},
      {1, {0x01}},
      {23, {0x17}},
      {24, {0x18, 0x18}},
      {std::numeric_limits<uint8_t>::max(), {0x18, 0xff}},
      {1LL << 8, {0x19, 0x01, 0x00}},
      {std::numeric_limits<uint16_t>::max(), {0x19, 0xff, 0xff}},
      {1LL << 16, {0x1a, 0x00, 0x01, 0x00, 0x00}},
      {std::numeric_limits<uint32_t>::max(), {0x1a, 0xff, 0xff, 0xff, 0xff}},
      {1LL << 32, {0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}},
      {std::numeric_limits<int64_t>::max(),
       {0x1b, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
  };

  for (const UintTestCase& test_case : kUintTestCases) {
    SCOPED_TRACE(testing::Message() << "testing uint: " << test_case.value);

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::UNSIGNED);
    EXPECT_EQ(cbor.value().GetInteger(), test_case.value);

    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);
    Reader::DecoderError error_code;
    cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    size_t num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed,
                        &error_code);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::UNSIGNED);
    EXPECT_EQ(cbor.value().GetInteger(), test_case.value);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestUintEncodedWithNonMinimumByteLength) {
  static const std::vector<uint8_t> non_minimal_uint_encodings[] = {
      // Uint 23 encoded with 1 byte.
      {0x18, 0x17},
      // Uint 255 encoded with 2 bytes.
      {0x19, 0x00, 0xff},
      // Uint 65535 encoded with 4 byte.
      {0x1a, 0x00, 0x00, 0xff, 0xff},
      // Uint 4294967295 encoded with 8 byte.
      {0x1b, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff},

      // When decoding byte has more than one syntax error, the first syntax
      // error encountered during deserialization is returned as the error code.
      {
          0xa2,        // map with non-minimally encoded key
          0x17,        // key 24
          0x61, 0x42,  // value :"B"

          0x18, 0x17,  // key 23 encoded with extra byte
          0x61, 0x45   // value "E"
      },
      {
          0xa2,        // map with out of order and non-minimally encoded key
          0x18, 0x17,  // key 23 encoded with extra byte
          0x61, 0x45,  // value "E"
          0x17,        // key 23
          0x61, 0x42   // value :"B"
      },
      {
          0xa2,        // map with duplicate non-minimally encoded key
          0x18, 0x17,  // key 23 encoded with extra byte
          0x61, 0x45,  // value "E"
          0x18, 0x17,  // key 23 encoded with extra byte
          0x61, 0x42   // value :"B"
      },
  };

  int test_case_index = 0;
  Reader::DecoderError error_code;
  for (const auto& non_minimal_uint : non_minimal_uint_encodings) {
    SCOPED_TRACE(testing::Message()
                 << "testing element at index : " << test_case_index++);

    std::optional<Value> cbor = Reader::Read(non_minimal_uint, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::NON_MINIMAL_CBOR_ENCODING);
  }
}

TEST(CBORReaderTest, TestReadNegativeInt) {
  struct NegativeIntTestCase {
    const int64_t negative_int;
    const std::vector<uint8_t> cbor_data;
  };

  static const NegativeIntTestCase kNegativeIntTestCases[] = {
      {-1LL, {0x20}},
      {-24LL, {0x37}},
      {-25LL, {0x38, 0x18}},
      {-256LL, {0x38, 0xff}},
      {-1000LL, {0x39, 0x03, 0xe7}},
      {-1000000LL, {0x3a, 0x00, 0x0f, 0x42, 0x3f}},
      {-4294967296LL, {0x3a, 0xff, 0xff, 0xff, 0xff}},
      {std::numeric_limits<int64_t>::min(),
       {0x3b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}};

  for (const NegativeIntTestCase& test_case : kNegativeIntTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "testing negative int : " << test_case.negative_int);

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::NEGATIVE);
    EXPECT_EQ(cbor.value().GetInteger(), test_case.negative_int);

    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);
    Reader::DecoderError error_code;
    cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    size_t num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed,
                        &error_code);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::NEGATIVE);
    EXPECT_EQ(cbor.value().GetInteger(), test_case.negative_int);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestReadBytes) {
  struct ByteTestCase {
    const std::vector<uint8_t> value;
    const std::vector<uint8_t> cbor_data;
  };

  static const ByteTestCase kByteStringTestCases[] = {
      // clang-format off
      {{}, {0x40}},
      {{0x01, 0x02, 0x03, 0x04}, {0x44, 0x01, 0x02, 0x03, 0x04}},
      // clang-format on
  };

  int element_index = 0;
  for (const ByteTestCase& test_case : kByteStringTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "testing string test case at : " << element_index++);

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::BYTE_STRING);
    EXPECT_EQ(cbor.value().GetBytestring(), test_case.value);

    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);
    Reader::DecoderError error_code;
    cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    size_t num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed,
                        &error_code);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::BYTE_STRING);
    EXPECT_EQ(cbor.value().GetBytestring(), test_case.value);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestReadString) {
  struct StringTestCase {
    const std::string value;
    const std::vector<uint8_t> cbor_data;
  };

  static const StringTestCase kStringTestCases[] = {
      {"", {0x60}},
      {"a", {0x61, 0x61}},
      {"IETF", {0x64, 0x49, 0x45, 0x54, 0x46}},
      {"\"\\", {0x62, 0x22, 0x5c}},
      {"\xc3\xbc", {0x62, 0xc3, 0xbc}},
      {"\xe6\xb0\xb4", {0x63, 0xe6, 0xb0, 0xb4}},
      {"\xf0\x90\x85\x91", {0x64, 0xf0, 0x90, 0x85, 0x91}},
  };

  for (const StringTestCase& test_case : kStringTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "testing string value : " << test_case.value);

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::STRING);
    EXPECT_EQ(cbor.value().GetString(), test_case.value);

    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);
    Reader::DecoderError error_code;
    cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    size_t num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed,
                        &error_code);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::STRING);
    EXPECT_EQ(cbor.value().GetString(), test_case.value);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestReadStringWithNUL) {
  static const struct {
    const std::string value;
    const std::vector<uint8_t> cbor_data;
  } kStringTestCases[] = {
      {std::string("string_without_nul"),
       {0x72, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x77, 0x69, 0x74, 0x68,
        0x6F, 0x75, 0x74, 0x5F, 0x6E, 0x75, 0x6C}},
      {std::string("nul_terminated_string\0", 22),
       {0x76, 0x6E, 0x75, 0x6C, 0x5F, 0x74, 0x65, 0x72, 0x6D, 0x69, 0x6E, 0x61,
        0x74, 0x65, 0x64, 0x5F, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x00}},
      {std::string("embedded\0nul", 12),
       {0x6C, 0x65, 0x6D, 0x62, 0x65, 0x64, 0x64, 0x65, 0x64, 0x00, 0x6E, 0x75,
        0x6C}},
      {std::string("trailing_nuls\0\0", 15),
       {0x6F, 0x74, 0x72, 0x61, 0x69, 0x6C, 0x69, 0x6E, 0x67, 0x5F, 0x6E, 0x75,
        0x6C, 0x73, 0x00, 0x00}},
  };

  for (const auto& test_case : kStringTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "testing string with nul bytes :" << test_case.value);

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::STRING);
    EXPECT_EQ(cbor.value().GetString(), test_case.value);

    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);
    Reader::DecoderError error_code;
    cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    size_t num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed,
                        &error_code);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::STRING);
    EXPECT_EQ(cbor.value().GetString(), test_case.value);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestReadStringWithInvalidByteSequenceAfterNUL) {
  // UTF-8 validation should not stop at the first NUL character in the string.
  // That is, a string with an invalid byte sequence should fail UTF-8
  // validation even if the invalid character is located after one or more NUL
  // characters. Here, 0xA6 is an unexpected continuation byte.
  static const std::vector<uint8_t> string_with_invalid_continuation_byte = {
      0x63, 0x00, 0x00, 0xA6};
  Reader::DecoderError error_code;
  std::optional<Value> cbor =
      Reader::Read(string_with_invalid_continuation_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::INVALID_UTF8);
}

TEST(CBORReaderTest, TestReadArray) {
  static const std::vector<uint8_t> kArrayTestCaseCbor = {
      // clang-format off
      0x98, 0x19,  // array of 25 elements
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x18, 0x18, 0x19,
      // clang-format on
  };

  std::optional<Value> cbor = Reader::Read(kArrayTestCaseCbor);
  ASSERT_TRUE(cbor.has_value());
  const Value cbor_array = std::move(cbor.value());
  ASSERT_EQ(cbor_array.type(), Value::Type::ARRAY);
  ASSERT_THAT(cbor_array.GetArray(), testing::SizeIs(25));

  std::vector<Value> array;
  for (int i = 0; i < 25; i++) {
    SCOPED_TRACE(testing::Message() << "testing array element at index " << i);

    ASSERT_EQ(cbor_array.GetArray()[i].type(), Value::Type::UNSIGNED);
    EXPECT_EQ(cbor_array.GetArray()[i].GetInteger(),
              static_cast<int64_t>(i + 1));
  }

  auto cbor_data_with_extra_byte = WithExtraneousData(kArrayTestCaseCbor);
  Reader::DecoderError error_code;
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor_array.type(), Value::Type::ARRAY);
  ASSERT_THAT(cbor_array.GetArray(), testing::SizeIs(25));
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kArrayTestCaseCbor.size());
}

TEST(CBORReaderTest, TestReadMapWithMapValue) {
  static const std::vector<uint8_t> kMapTestCaseCbor = {
      // clang-format off
      0xa4,  // map with 4 key value pairs:
        0x18, 0x18,  // 24
        0x63, 0x61, 0x62, 0x63,  // "abc"

        0x60,  // ""
        0x61, 0x2e,  // "."

        0x61, 0x62,  // "b"
        0x61, 0x42,  // "B"

        0x62, 0x61, 0x61,  // "aa"
        0x62, 0x41, 0x41,  // "AA"
      // clang-format on
  };

  std::optional<Value> cbor = Reader::Read(kMapTestCaseCbor);
  ASSERT_TRUE(cbor.has_value());
  const Value cbor_val = std::move(cbor.value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 4u);

  const Value key_uint(24);
  ASSERT_EQ(cbor_val.GetMap().count(key_uint), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_uint)->second.type(),
            Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_uint)->second.GetString(), "abc");

  const Value key_empty_string("");
  ASSERT_EQ(cbor_val.GetMap().count(key_empty_string), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_empty_string)->second.type(),
            Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_empty_string)->second.GetString(), ".");

  const Value key_b("b");
  ASSERT_EQ(cbor_val.GetMap().count(key_b), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_b)->second.type(), Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_b)->second.GetString(), "B");

  const Value key_aa("aa");
  ASSERT_EQ(cbor_val.GetMap().count(key_aa), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_aa)->second.type(), Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_aa)->second.GetString(), "AA");

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapTestCaseCbor);
  Reader::DecoderError error_code;
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 4u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kMapTestCaseCbor.size());
}

TEST(CBORReaderTest, TestReadMapWithIntegerKeys) {
  static const std::vector<uint8_t> kMapWithIntegerKeyCbor = {
      // clang-format off
      0xA4,                 // map with 4 key value pairs
         0x01,              // key : 1
         0x61, 0x61,        // value : "a"

         0x09,              // key : 9
         0x61, 0x62,        // value : "b"

         0x19, 0x03, 0xE7,  // key : 999
         0x61, 0x63,        // value "c"

         0x19, 0x04, 0x57,  // key : 1111
         0x61, 0x64,        // value : "d"
      // clang-format on
  };

  std::optional<Value> cbor = Reader::Read(kMapWithIntegerKeyCbor);
  ASSERT_TRUE(cbor.has_value());
  const Value cbor_val = std::move(cbor.value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 4u);

  const Value key_1(1);
  ASSERT_EQ(cbor_val.GetMap().count(key_1), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_1)->second.type(), Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_1)->second.GetString(), "a");

  const Value key_9(9);
  ASSERT_EQ(cbor_val.GetMap().count(key_9), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_9)->second.type(), Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_9)->second.GetString(), "b");

  const Value key_999(999);
  ASSERT_EQ(cbor_val.GetMap().count(key_999), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_999)->second.type(),
            Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_999)->second.GetString(), "c");

  const Value key_1111(1111);
  ASSERT_EQ(cbor_val.GetMap().count(key_1111), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_1111)->second.type(),
            Value::Type::STRING);
  EXPECT_EQ(cbor_val.GetMap().find(key_1111)->second.GetString(), "d");

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapWithIntegerKeyCbor);
  Reader::DecoderError error_code;
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 4u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kMapWithIntegerKeyCbor.size());
}

TEST(CBORReaderTest, TestReadMapWithNegativeIntegersKeys) {
  static const std::vector<uint8_t> kMapWithIntegerKeyCbor = {
      // clang-format off
      0xA3,                 // map with 3 key value pairs
         0x20,              // key : -1
         0x01,

         0x21,              // key : -2
         0x02,

         0x38, 0x63,        // key : -100
         0x03,
      // clang-format on
  };

  std::optional<Value> cbor = Reader::Read(kMapWithIntegerKeyCbor);
  ASSERT_TRUE(cbor.has_value());
  const Value cbor_val = std::move(cbor.value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 3u);

  const Value key_1(-1);
  ASSERT_EQ(cbor_val.GetMap().count(key_1), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_1)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(cbor_val.GetMap().find(key_1)->second.GetInteger(), 1);

  const Value key_2(-2);
  ASSERT_EQ(cbor_val.GetMap().count(key_2), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_2)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(cbor_val.GetMap().find(key_2)->second.GetInteger(), 2);

  const Value key_100(-100);
  ASSERT_EQ(cbor_val.GetMap().count(key_100), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_100)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(cbor_val.GetMap().find(key_100)->second.GetInteger(), 3);

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapWithIntegerKeyCbor);
  Reader::DecoderError error_code;
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 3u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kMapWithIntegerKeyCbor.size());
}

TEST(CBORReaderTest, TestReadMapWithArray) {
  static const std::vector<uint8_t> kMapArrayTestCaseCbor = {
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

  std::optional<Value> cbor = Reader::Read(kMapArrayTestCaseCbor);
  ASSERT_TRUE(cbor.has_value());
  const Value cbor_val = std::move(cbor.value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 2u);

  const Value key_a("a");
  ASSERT_EQ(cbor_val.GetMap().count(key_a), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_a)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(cbor_val.GetMap().find(key_a)->second.GetInteger(), 1u);

  const Value key_b("b");
  ASSERT_EQ(cbor_val.GetMap().count(key_b), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_b)->second.type(), Value::Type::ARRAY);

  const Value nested_array = cbor_val.GetMap().find(key_b)->second.Clone();
  ASSERT_EQ(nested_array.GetArray().size(), 2u);
  for (int i = 0; i < 2; i++) {
    ASSERT_THAT(nested_array.GetArray()[i].type(), Value::Type::UNSIGNED);
    EXPECT_EQ(nested_array.GetArray()[i].GetInteger(),
              static_cast<int64_t>(i + 2));
  }

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapArrayTestCaseCbor);
  Reader::DecoderError error_code;
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 2u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kMapArrayTestCaseCbor.size());
}

TEST(CBORReaderTest, TestReadMapWithTextStringKeys) {
  static const std::vector<uint8_t> kMapTestCase{
      // clang-format off
      0xa2,  // map of 2 pairs
        0x61, 'k', // text string "k"
        0x61, 'v',

        0x63, 'f', 'o', 'o', // text string "foo"
        0x63, 'b', 'a', 'r',
      // clang-format on
  };

  Reader::DecoderError error_code;
  std::optional<Value> cbor = Reader::Read(kMapTestCase, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor->type(), Value::Type::MAP);
  ASSERT_EQ(cbor->GetMap().size(), 2u);

  const Value key_k("k");
  ASSERT_EQ(cbor->GetMap().count(key_k), 1u);
  ASSERT_EQ(cbor->GetMap().find(key_k)->second.type(), Value::Type::STRING);
  EXPECT_EQ(cbor->GetMap().find(key_k)->second.GetString(), "v");

  const Value key_foo("foo");
  ASSERT_EQ(cbor->GetMap().count(key_foo), 1u);
  ASSERT_EQ(cbor->GetMap().find(key_foo)->second.type(), Value::Type::STRING);
  EXPECT_EQ(cbor->GetMap().find(key_foo)->second.GetString(), "bar");

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapTestCase);
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor->type(), Value::Type::MAP);
  ASSERT_EQ(cbor->GetMap().size(), 2u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kMapTestCase.size());
}

TEST(CBORReaderTest, TestReadMapWithByteStringKeys) {
  static const std::vector<uint8_t> kMapTestCase{
      // clang-format off
      0xa2,  // map of 2 pairs
        0x41, 'k', // byte string "k"
        0x41, 'v',

        0x43, 'f', 'o', 'o', // byte string "foo"
        0x43, 'b', 'a', 'r',
      // clang-format on
  };

  Reader::DecoderError error_code;
  std::optional<Value> cbor = Reader::Read(kMapTestCase, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor->type(), Value::Type::MAP);
  ASSERT_EQ(cbor->GetMap().size(), 2u);

  const Value key_k(std::vector<uint8_t>{'k'});
  ASSERT_EQ(cbor->GetMap().count(key_k), 1u);
  ASSERT_EQ(cbor->GetMap().find(key_k)->second.type(),
            Value::Type::BYTE_STRING);
  EXPECT_EQ(cbor->GetMap().find(key_k)->second.GetBytestring(),
            std::vector<uint8_t>{'v'});

  const Value key_foo(std::vector<uint8_t>{'f', 'o', 'o'});
  ASSERT_EQ(cbor->GetMap().count(key_foo), 1u);
  ASSERT_EQ(cbor->GetMap().find(key_foo)->second.type(),
            Value::Type::BYTE_STRING);
  static const std::vector<uint8_t> kBarBytes{'b', 'a', 'r'};
  EXPECT_EQ(cbor->GetMap().find(key_foo)->second.GetBytestring(), kBarBytes);

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapTestCase);
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor->type(), Value::Type::MAP);
  ASSERT_EQ(cbor->GetMap().size(), 2u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, kMapTestCase.size());
}

TEST(CBORReaderTest, TestReadMapWithMixedKeys) {
  // Example adopted from:
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
  static const uint8_t kMapTestCase[] = {
      // clang-format off
      0xa6, // map of 6 pairs
        0x0a, // 10
        0x00,

        0x18, 0x64, // 100
        0x01,

        0x20, // -1
        0x02,

        // This entry is not in the example, but added to test byte string key
        0x42, 'x', 'y', // byte string "xy"
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

  Reader::DecoderError error_code;
  std::optional<Value> cbor = Reader::Read(kMapTestCase, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor->type(), Value::Type::MAP);
  ASSERT_EQ(cbor->GetMap().size(), 6u);

  std::vector<Value> keys;
  keys.emplace_back(10);
  keys.emplace_back(100);
  keys.emplace_back(-1);
  keys.emplace_back(Value::BinaryValue{'x', 'y'});
  keys.emplace_back("z");
  keys.emplace_back("aa");
  for (size_t i = 0; i < keys.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "testing key at index: " << i);
    ASSERT_EQ(cbor->GetMap().count(keys[i]), 1u);
    ASSERT_EQ(cbor->GetMap().find(keys[i])->second.type(),
              Value::Type::UNSIGNED);
    EXPECT_EQ(cbor->GetMap().find(keys[i])->second.GetInteger(),
              static_cast<int>(i));
  }

  auto cbor_data_with_extra_byte = WithExtraneousData(kMapTestCase);
  cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

  size_t num_bytes_consumed;
  cbor =
      Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed, &error_code);
  ASSERT_TRUE(cbor.has_value());
  ASSERT_EQ(cbor->type(), Value::Type::MAP);
  ASSERT_EQ(cbor->GetMap().size(), 6u);
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  EXPECT_EQ(num_bytes_consumed, std::size(kMapTestCase));
}

TEST(CBORReaderTest, TestReadNestedMap) {
  static const std::vector<uint8_t> kNestedMapTestCase = {
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

  std::optional<Value> cbor = Reader::Read(kNestedMapTestCase);
  ASSERT_TRUE(cbor.has_value());
  const Value cbor_val = std::move(cbor.value());
  ASSERT_EQ(cbor_val.type(), Value::Type::MAP);
  ASSERT_EQ(cbor_val.GetMap().size(), 2u);

  const Value key_a("a");
  ASSERT_EQ(cbor_val.GetMap().count(key_a), 1u);
  ASSERT_EQ(cbor_val.GetMap().find(key_a)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(cbor_val.GetMap().find(key_a)->second.GetInteger(), 1u);

  const Value key_b("b");
  ASSERT_EQ(cbor_val.GetMap().count(key_b), 1u);
  const Value nested_map = cbor_val.GetMap().find(key_b)->second.Clone();
  ASSERT_EQ(nested_map.type(), Value::Type::MAP);
  ASSERT_EQ(nested_map.GetMap().size(), 2u);

  const Value key_c("c");
  ASSERT_EQ(nested_map.GetMap().count(key_c), 1u);
  ASSERT_EQ(nested_map.GetMap().find(key_c)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(nested_map.GetMap().find(key_c)->second.GetInteger(), 2u);

  const Value key_d("d");
  ASSERT_EQ(nested_map.GetMap().count(key_d), 1u);
  ASSERT_EQ(nested_map.GetMap().find(key_d)->second.type(),
            Value::Type::UNSIGNED);
  EXPECT_EQ(nested_map.GetMap().find(key_d)->second.GetInteger(), 3u);
}

TEST(CBORReaderTest, TestIntegerRange) {
  static const std::vector<uint8_t> kMaxPositiveInt = {
      0x1b, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  static const std::vector<uint8_t> kMinNegativeInt = {
      0x3b, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  std::optional<Value> max_positive_int = Reader::Read(kMaxPositiveInt);
  ASSERT_TRUE(max_positive_int.has_value());
  EXPECT_EQ(max_positive_int.value().GetInteger(), INT64_MAX);

  std::optional<Value> min_negative_int = Reader::Read(kMinNegativeInt);
  ASSERT_TRUE(min_negative_int.has_value());
  EXPECT_EQ(min_negative_int.value().GetInteger(), INT64_MIN);
}

TEST(CBORReaderTest, TestIntegerOutOfRangeError) {
  static const std::vector<uint8_t> kOutOfRangePositiveInt = {
      0x1b, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  static const std::vector<uint8_t> kOutOfRangeNegativeInt = {
      0x3b, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  Reader::DecoderError error_code;
  std::optional<Value> positive_int_out_of_range_cbor =
      Reader::Read(kOutOfRangePositiveInt, &error_code);
  EXPECT_FALSE(positive_int_out_of_range_cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::OUT_OF_RANGE_INTEGER_VALUE);

  std::optional<Value> negative_int_out_of_range_cbor =
      Reader::Read(kOutOfRangeNegativeInt, &error_code);
  EXPECT_FALSE(negative_int_out_of_range_cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::OUT_OF_RANGE_INTEGER_VALUE);
}

TEST(CBORReaderTest, TestReadSimpleValue) {
  static const struct {
    const Value::SimpleValue value;
    const std::vector<uint8_t> cbor_data;
  } kSimpleValueTestCases[] = {
      {Value::SimpleValue::FALSE_VALUE, {0xf4}},
      {Value::SimpleValue::TRUE_VALUE, {0xf5}},
      {Value::SimpleValue::NULL_VALUE, {0xf6}},
      {Value::SimpleValue::UNDEFINED, {0xf7}},
  };

  int test_element_index = 0;
  for (const auto& test_case : kSimpleValueTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "testing simple value at index : " << test_element_index++);

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::SIMPLE_VALUE);
    EXPECT_EQ(cbor.value().GetSimpleValue(), test_case.value);

    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);
    Reader::DecoderError error_code;
    cbor = Reader::Read(cbor_data_with_extra_byte, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    size_t num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, &num_bytes_consumed,
                        &error_code);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::SIMPLE_VALUE);
    EXPECT_EQ(cbor.value().GetSimpleValue(), test_case.value);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestReadFloatingPointNumbers) {
  static const struct {
    const double value;
    const std::vector<uint8_t> cbor_data;
  } kFloatingPointTestCases[] = {
      // 16 bit floating point values.
      {0.5, {0xf9, 0x38, 0x00}},
      {3.140625, {0xf9, 0x42, 0x48}},
      {0.0, {0xf9, 0x00, 0x00}},
      {-0.0, {0xf9, 0x80, 0x00}},
      {std::numeric_limits<double>::infinity(), {0xf9, 0x7c, 0x00}},
      {-std::numeric_limits<double>::infinity(), {0xf9, 0xfc, 0x00}},
      {std::scalbn(1023.0, -24), {0xf9, 0x03, 0xFF}},
      {65504, {0xf9, 0x7b, 0xff}},
      // 32 bit floating point value.
      {3.1415927410125732, {0xfa, 0x40, 0x49, 0x0f, 0xdb}},
      {2049.0, {0xfa, 0x45, 0x00, 0x10, 0x00}},
      // 64 bit floating point value.
      {3.141592653589793,
       {0xfb, 0x40, 0x09, 0x21, 0xfb, 0x54, 0x44, 0x2d, 0x18}},
      {268435455.0, {0xfb, 0x41, 0xaf, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00}},
  };

  for (const auto& test_case : kFloatingPointTestCases) {
    SCOPED_TRACE(testing::Message() << "testing float: " << test_case.value);

    Reader::Config config;
    size_t num_bytes_consumed;
    Reader::DecoderError error_code;
    config.allow_floating_point = true;

    std::optional<Value> cbor = Reader::Read(test_case.cbor_data, config);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::FLOAT_VALUE);
    EXPECT_EQ(cbor.value().GetDouble(), test_case.value);

    config.error_code_out = &error_code;
    auto cbor_data_with_extra_byte = WithExtraneousData(test_case.cbor_data);

    cbor = Reader::Read(cbor_data_with_extra_byte, config);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);

    config.num_bytes_consumed = &num_bytes_consumed;
    cbor = Reader::Read(cbor_data_with_extra_byte, config);
    ASSERT_TRUE(cbor.has_value());
    ASSERT_EQ(cbor.value().type(), Value::Type::FLOAT_VALUE);
    EXPECT_EQ(cbor.value().GetDouble(), test_case.value);
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    EXPECT_EQ(num_bytes_consumed, test_case.cbor_data.size());
  }
}

TEST(CBORReaderTest, TestReadNonMinimalFloatingPointNumbers) {
  static const std::vector<uint8_t> test_case_inputs[] = {
      {0xfa, 0x00, 0x00, 0x00, 0x00},  // 0 as 32 bit float.
      {0xfa, 0x7f, 0x80, 0x00, 0x00},  // infinity as 32 bit float.
      {0xfa, 0xff, 0x80, 0x00, 0x00},  // -infinity as 32 bit float.
      {0xfa, 0x7f, 0xC0, 0x00, 0x00},  // -NaN as 32 bit float.
      {0xfa, 0xff, 0xc0, 0x00, 0x00},  // -NaN as 32 bit float.
      // 3.1415927410125732 as 64 bit double (fits in 32 bits).
      {0xfb, 0x40, 0x09, 0x21, 0xfb, 0x60, 0x00, 0x00, 0x00},
  };
  for (const auto& input : test_case_inputs) {
    SCOPED_TRACE(testing::Message() << "Testing non-minimal floating point : "
                                    << testing::PrintToString(input));
    Reader::Config config;
    Reader::DecoderError error_code;
    config.error_code_out = &error_code;
    config.allow_floating_point = true;

    std::optional<Value> cbor = Reader::Read(input, config);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::NON_MINIMAL_CBOR_ENCODING);
  }
}

TEST(CBORReaderTest, TestReadUnsupportedFloatingPointNumbers) {
  static const std::vector<uint8_t> floating_point_cbors[] = {
      // 16 bit floating point value.
      {0xf9, 0x10, 0x00},
      // 32 bit floating point value.
      {0xfa, 0x10, 0x00, 0x00, 0x00},
      // 64 bit floating point value.
      {0xfb, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  for (const auto& unsupported_floating_point : floating_point_cbors) {
    SCOPED_TRACE(testing::Message()
                 << "testing unsupported floating point : "
                 << testing::PrintToString(unsupported_floating_point));
    Reader::DecoderError error_code;
    std::optional<Value> cbor =
        Reader::Read(unsupported_floating_point, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code,
              Reader::DecoderError::UNSUPPORTED_FLOATING_POINT_VALUE);
  }
}

TEST(CBORReaderTest, TestIncompleteCBORDataError) {
  static const std::vector<uint8_t> incomplete_cbor_list[] = {
      // Additional info byte corresponds to unsigned int that corresponds
      // to 2 additional bytes. But actual data encoded  in one byte.
      {0x19, 0x03},
      // CBOR bytestring of length 3 encoded with additional info of length 4.
      {0x44, 0x01, 0x02, 0x03},
      // CBOR string data "IETF" of length 4 encoded with additional info of
      // length 5.
      {0x65, 0x49, 0x45, 0x54, 0x46},
      // CBOR array of length 1 encoded with additional info of length 2.
      {0x82, 0x02},
      // CBOR map with single key value pair encoded with additional info of
      // length 2.
      {0xa2, 0x61, 0x61, 0x01},
      {0x18},  // unsigned with pending 1 byte of numeric value.
      {0x99},  // array with pending 2 byte of numeric value (length).
      {0xba},  // map with pending 4 byte of numeric value (length).
      {0x5b},  // byte string with pending 4 byte of numeric value (length).
      {0x3b},  // negative integer with pending 8 byte of numeric value.
      {0x99, 0x01},  // array with pending 2 byte of numeric value (length),
                     // with only 1 byte of additional data.
      {0xba, 0x01, 0x02, 0x03},  // map with pending 4 byte of numeric value
                                 // (length), with only 3 bytes of additional
                                 // data.
      {0x3b, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
       0x07},  // negative integer with pending 8 byte of
               // numeric value, with only 7 bytes of
               // additional data.
  };

  int test_element_index = 0;
  for (const auto& incomplete_data : incomplete_cbor_list) {
    SCOPED_TRACE(testing::Message() << "testing incomplete data at index : "
                                    << test_element_index++);

    Reader::DecoderError error_code;
    std::optional<Value> cbor = Reader::Read(incomplete_data, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::INCOMPLETE_CBOR_DATA);
  }
}

// While RFC 7049 allows CBOR map keys with all types, current decoder only
// supports unsigned integer and string keys.
TEST(CBORReaderTest, TestUnsupportedMapKeyFormatError) {
  static const std::vector<uint8_t> kMapWithUintKey = {
      // clang-format off
      0xa2,        // map of 2 pairs

        0x82, 0x01, 0x02,  // invalid key : [1, 2]
        0x02,              // value : 2

        0x61, 0x64,  // key : "d"
        0x03,        // value : 3
      // clang-format on
  };

  Reader::DecoderError error_code;
  std::optional<Value> cbor = Reader::Read(kMapWithUintKey, &error_code);
  EXPECT_FALSE(cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::INCORRECT_MAP_KEY_TYPE);
}

TEST(CBORReaderTest, TestUnknownAdditionalInfoError) {
  static const std::vector<uint8_t> kUnknownAdditionalInfoList[] = {
      // "IETF" encoded with major type 3 and additional info of 28.
      {0x7C, 0x49, 0x45, 0x54, 0x46},
      // "\"\\" encoded with major type 3 and additional info of 29.
      {0x7D, 0x22, 0x5c},
      // "\xc3\xbc" encoded with major type 3 and additional info of 30.
      {0x7E, 0xc3, 0xbc},
      // "\xe6\xb0\xb4" encoded with major type 3 and additional info of 31.
      {0x7F, 0xe6, 0xb0, 0xb4},
      // Major type 7, additional information 28: unassigned.
      {0xFC},
      // Major type 7, additional information 29: unassigned.
      {0xFD},
      // Major type 7, additional information 30: unassigned.
      {0xFE},
      // Major type 7, additional information 31: "break" stop code for
      // indefinite-length items.
      {0xFF},
  };

  int test_element_index = 0;
  for (const auto& incorrect_cbor : kUnknownAdditionalInfoList) {
    SCOPED_TRACE(testing::Message()
                 << "testing data at index : " << test_element_index++);

    Reader::DecoderError error_code;
    std::optional<Value> cbor = Reader::Read(incorrect_cbor, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::UNKNOWN_ADDITIONAL_INFO);
  }
}

TEST(CBORReaderTest, TestTooMuchNestingError) {
  static const std::vector<uint8_t> kZeroDepthCBORList[] = {
      // Unsigned int with value 100.
      {0x18, 0x64},
      // CBOR bytestring of length 4.
      {0x44, 0x01, 0x02, 0x03, 0x04},
      // CBOR string of corresponding to "IETF.
      {0x64, 0x49, 0x45, 0x54, 0x46},
      // Empty CBOR array.
      {0x80},
      // Empty CBOR Map
      {0xa0},
  };

  int test_element_index = 0;
  for (const auto& zero_depth_data : kZeroDepthCBORList) {
    SCOPED_TRACE(testing::Message()
                 << "testing zero nested data : " << test_element_index++);
    Reader::DecoderError error_code;
    std::optional<Value> cbor = Reader::Read(zero_depth_data, &error_code, 0);
    EXPECT_TRUE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  }

  // Corresponds to a CBOR structure with a nesting depth of 2:
  //      {"a": 1,
  //       "b": [2, 3]}
  static const std::vector<uint8_t> kNestedCBORData = {
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

  Reader::DecoderError error_code;
  std::optional<Value> cbor_single_layer_max =
      Reader::Read(kNestedCBORData, &error_code, 1);
  EXPECT_FALSE(cbor_single_layer_max.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::TOO_MUCH_NESTING);

  std::optional<Value> cbor_double_layer_max =
      Reader::Read(kNestedCBORData, &error_code, 2);
  EXPECT_TRUE(cbor_double_layer_max.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
}

TEST(CBORReaderTest, TestOutOfOrderKeyError) {
  static const std::vector<uint8_t> kMapsWithUnsortedKeys[] = {
      // clang-format off
      {0xa2,  // map with 2 keys with same major type and length
         0x61, 0x62,  // key "b"
         0x61, 0x42,  // value "B"

         0x61, 0x61,  // key "a" (out of order byte-wise lexically)
         0x61, 0x45   // value "E"
      },
      {0xa2,  // map with 2 keys with different major type
         0x61, 0x62,        // key "b"
         0x02,              // value 2

         // key 1000 (out of order since lower major type sorts first)
         0x19, 0x03, 0xe8,
         0x61, 0x61,        // value a
      },
      {0xa2,  // map with 2 keys with same major type
         0x19, 0x03, 0xe8,  // key 1000  (out of order due to longer length)
         0x61, 0x61,        //value "a"

         0x0a,              // key 10
         0x61, 0x62},       // value "b"
      {0xa2,  // map with 2 text string keys
         0x62, 'a', 'a', // key text string "aa"
                         // (out of order due to longer length)
         0x02,

         0x61, 'b',   // key "b"
         0x01,
      },
      {0xa2,  // map with 2 byte string keys
         0x42, 'x', 'x', // key byte string "xx"
                         // (out of order due to longer length)
         0x02,

         0x41, 'y',  // key byte string "y"
         0x01,
      },
      //clang-format on
  };

  int test_element_index = 0;
  for (const auto& unsorted_map : kMapsWithUnsortedKeys) {
    testing::Message scope_message;
    scope_message << "testing unsorted map : " << test_element_index++;
    SCOPED_TRACE(scope_message);

    // Expect `OUT_OF_ORDER_KEY`.
    {
      Reader::DecoderError error_code;
      std::optional<Value> cbor =
          Reader::Read(unsorted_map, &error_code);
      EXPECT_FALSE(cbor.has_value());
      EXPECT_EQ(error_code, Reader::DecoderError::OUT_OF_ORDER_KEY);
    }

    // When `allow_and_canonicalize_out_of_order_keys` flag is set, expect
    // `CBOR_NO_ERROR`.
    {
      Reader::DecoderError error_code;
      Reader::Config config;
      config.error_code_out = &error_code;
      config.allow_and_canonicalize_out_of_order_keys = true;

      std::optional<Value> cbor =
          Reader::Read(unsorted_map, config);
      EXPECT_TRUE(cbor);
      EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
    }
  }
}

TEST(CBORReaderTest, TestOutOfOrderKeyErrorWithDuplicateKeys) {
  static const std::vector<uint8_t> kMapsWithUnsortedKeys[] = {
      // clang-format off
      {0xa3,  // map with 3 keys with same major type and length
         0x61, 0x62,  // key "b"
         0x61, 0x42,  // value "B"

         0x61, 0x61,  // key "a" (out of order byte-wise lexically)
         0x61, 0x45,   // value "E"

         0x61, 0x62,  // key "b" (duplicate)
         0x61, 0x42,  // value "B"
      },
      {0xa3,  // map with 3 byte string keys
         0x42, 'x', 'x', // key byte string "xx"
                         // (out of order due to longer length)
         0x02,

         0x41, 'y',  // key byte string "y"
         0x01,

         0x41, 'y',  // key byte string "y" (duplicate)
         0x02,
      },
      //clang-format on
  };

  int test_element_index = 0;
  for (const auto& unsorted_map : kMapsWithUnsortedKeys) {
    testing::Message scope_message;
    scope_message << "testing unsorted map : " << test_element_index++;
    SCOPED_TRACE(scope_message);

    // Expect `OUT_OF_ORDER_KEY`.
    {
      Reader::DecoderError error_code;
      std::optional<Value> cbor =
          Reader::Read(unsorted_map, &error_code);
      EXPECT_FALSE(cbor.has_value());
      EXPECT_EQ(error_code, Reader::DecoderError::OUT_OF_ORDER_KEY);
    }

    // When `allow_and_canonicalize_out_of_order_keys` flag is set, expect
    // `DUPLICATE_KEY`.
    {
      Reader::DecoderError error_code;
      Reader::Config config;
      config.error_code_out = &error_code;
      config.allow_and_canonicalize_out_of_order_keys = true;

      std::optional<Value> cbor =
          Reader::Read(unsorted_map, config);
      EXPECT_FALSE(cbor);
      EXPECT_EQ(error_code, Reader::DecoderError::DUPLICATE_KEY);
    }
  }
}

TEST(CBORReaderTest, TestDuplicateKeyError) {
  static const std::vector<uint8_t> kMapWithDuplicateKey = {
      // clang-format off
      0xa6,  // map of 6 pairs:
        0x60,  // ""
        0x61, 0x2e,  // "."

        0x61, 0x62,  // "b"
        0x61, 0x42,  // "B"

        0x61, 0x62,  // "b" (Duplicate key)
        0x61, 0x43,  // "C"

        0x61, 0x64,  // "d"
        0x61, 0x44,  // "D"

        0x61, 0x65,  // "e"
        0x61, 0x44,  // "D"

        0x62, 0x61, 0x61,  // "aa"
        0x62, 0x41, 0x41,  // "AA"
      // clang-format on
  };

  {
    Reader::DecoderError error_code;
    std::optional<Value> cbor = Reader::Read(kMapWithDuplicateKey, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::DUPLICATE_KEY);
  }

  {
    Reader::DecoderError error_code;
    Reader::Config config;
    config.error_code_out = &error_code;
    config.allow_and_canonicalize_out_of_order_keys = true;

    std::optional<Value> cbor = Reader::Read(kMapWithDuplicateKey, config);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::DUPLICATE_KEY);
  }
}

// Leveraging Markus Kuhn’s UTF-8 decoder stress test. See
// http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt for details.
TEST(CBORReaderTest, TestIncorrectStringEncodingError) {
  static const std::vector<uint8_t> utf8_character_encodings[] = {
      // Corresponds to utf8 encoding of "퟿" (section 2.3.1 of stress test).
      {0x63, 0xED, 0x9F, 0xBF},
      // Corresponds to utf8 encoding of "" (section 2.3.2 of stress test).
      {0x63, 0xEE, 0x80, 0x80},
      // Corresponds to utf8 encoding of "�"  (section 2.3.3 of stress test).
      {0x63, 0xEF, 0xBF, 0xBD},
  };

  int test_element_index = 0;
  Reader::DecoderError error_code;
  for (const auto& cbor_byte : utf8_character_encodings) {
    SCOPED_TRACE(testing::Message() << "testing cbor data utf8 encoding : "
                                    << test_element_index++);

    std::optional<Value> correctly_encoded_cbor =
        Reader::Read(cbor_byte, &error_code);
    EXPECT_TRUE(correctly_encoded_cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::CBOR_NO_ERROR);
  }

  // Incorrect UTF8 encoding referenced by section 3.5.3 of the stress test.
  std::vector<uint8_t> impossible_utf_byte{0x64, 0xfe, 0xfe, 0xff, 0xff};
  std::optional<Value> incorrectly_encoded_cbor =
      Reader::Read(impossible_utf_byte, &error_code);
  EXPECT_FALSE(incorrectly_encoded_cbor.has_value());
  EXPECT_EQ(error_code, Reader::DecoderError::INVALID_UTF8);
}

TEST(CBORReaderTest, TestExtraneousCBORDataError) {
  static const std::vector<uint8_t> zero_padded_cbor_list[] = {
      // 1 extra byte after a 2-byte unsigned int.
      {0x19, 0x03, 0x05, 0x00},
      // 1 extra byte after a 4-byte cbor byte array.
      {0x44, 0x01, 0x02, 0x03, 0x04, 0x00},
      // 1 extra byte after a 4-byte string.
      {0x64, 0x49, 0x45, 0x54, 0x46, 0x00},
      // 1 extra byte after CBOR array of length 2.
      {0x82, 0x01, 0x02, 0x00},
      // 1 extra key value pair after CBOR map of size 2.
      {0xa1, 0x61, 0x63, 0x02, 0x61, 0x64, 0x03},
  };

  int test_element_index = 0;
  for (const auto& extraneous_cbor_data : zero_padded_cbor_list) {
    SCOPED_TRACE(testing::Message()
                 << "testing cbor extraneous data : " << test_element_index++);

    Reader::DecoderError error_code;
    std::optional<Value> cbor = Reader::Read(extraneous_cbor_data, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::EXTRANEOUS_DATA);
  }
}

TEST(CBORReaderTest, TestUnsupportedSimpleValue) {
  static const std::vector<uint8_t> unsupported_simple_values[] = {
      // Simple value (0, unassigned)
      {0xE0},
      // Simple value (19, unassigned)
      {0xF3},
      // Simple value (24, reserved)
      {0xF8, 0x18},
      // Simple value (28, reserved)
      {0xF8, 0x1C},
      // Simple value (29, reserved)
      {0xF8, 0x1D},
      // Simple value (30, reserved)
      {0xF8, 0x1E},
      // Simple value (31, reserved)
      {0xF8, 0x1F},
      // Simple value (32, unassigned)
      {0xF8, 0x20},
      // Simple value (255, unassigned)
      {0xF8, 0xFF},
  };

  for (const auto& unsupported_simple_val : unsupported_simple_values) {
    SCOPED_TRACE(testing::Message()
                 << "testing unsupported cbor simple value  : "
                 << ::testing::PrintToString(unsupported_simple_val));

    Reader::DecoderError error_code;
    std::optional<Value> cbor =
        Reader::Read(unsupported_simple_val, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::UNSUPPORTED_SIMPLE_VALUE);
  }
}

TEST(CBORReaderTest, TestSuperLongContentDontCrash) {
  static const std::vector<uint8_t> kTestCases[] = {
      // CBOR array of 0xffffffff length.
      {0x9b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
      // CBOR map of 0xffffffff pairs.
      {0xbb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
  };
  for (const auto& test_case : kTestCases) {
    Reader::DecoderError error_code;
    std::optional<Value> cbor = Reader::Read(test_case, &error_code);
    EXPECT_FALSE(cbor.has_value());
    EXPECT_EQ(error_code, Reader::DecoderError::INCOMPLETE_CBOR_DATA);
  }
}

TEST(CBORReaderTest, AllowInvalidUTF8) {
  static const uint8_t kInvalidUTF8[] = {
      // clang-format off
      0xa1,                    // map of length 1
        0x61, 'x',             // "x"
        0x81,                  // array of length 1
          0xa2,                // map of length 2
            0x61, 'y',         // "y"
            0x62, 0xe2, 0x80,  // invalid UTF-8 value
            0x61, 'z',         // "z"
            0x61, '.',         // "."
      // clang-format on
  };

  Reader::DecoderError error;
  Reader::Config config;
  config.error_code_out = &error;

  std::optional<Value> cbor = Reader::Read(kInvalidUTF8, config);
  EXPECT_FALSE(cbor);
  EXPECT_EQ(Reader::DecoderError::INVALID_UTF8, error);

  cbor = Reader::Read(kInvalidUTF8, config);
  EXPECT_FALSE(cbor);
  EXPECT_EQ(Reader::DecoderError::INVALID_UTF8, error);

  config.allow_invalid_utf8 = true;

  cbor = Reader::Read(kInvalidUTF8, config);
  EXPECT_TRUE(cbor);
  EXPECT_EQ(Reader::DecoderError::CBOR_NO_ERROR, error);
  const cbor::Value& invalid_value = cbor->GetMap()
                                         .find(Value("x"))
                                         ->second.GetArray()[0]
                                         .GetMap()
                                         .find(Value("y"))
                                         ->second;
  ASSERT_TRUE(invalid_value.is_invalid_utf8());
  EXPECT_EQ(std::vector<uint8_t>({0xe2, 0x80}), invalid_value.GetInvalidUTF8());

  static const uint8_t kInvalidUTF8InMapKey[] = {
      // clang-format off
      0xa1,                    // map of length 1
        0x62, 0xe2, 0x80,      // invalid UTF-8 map key
        0x61, '.',             // "."
      // clang-format on
  };

  EXPECT_TRUE(config.allow_invalid_utf8);
  cbor = Reader::Read(kInvalidUTF8InMapKey, config);
  EXPECT_FALSE(cbor);
  EXPECT_EQ(Reader::DecoderError::INVALID_UTF8, error);
}

}  // namespace cbor
