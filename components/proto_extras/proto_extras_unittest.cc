// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "components/proto_extras/test_proto/test_proto.pb.h"
#include "components/proto_extras/test_proto/test_proto.to_value.h"
#include "components/proto_extras/test_proto/test_proto_dependency.pb.h"
#include "components/proto_extras/test_proto/test_proto_dependency.to_value.h"
#include "components/proto_extras/test_proto2/test_proto2.pb.h"
#include "components/proto_extras/test_proto2/test_proto2.to_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proto_extras {
namespace {

TEST(ProtoToValueTest, BasicField) {
  TestMessage message;
  message.set_double_field(1.0);
  message.set_int32_field(2);
  message.mutable_nested_message_field()->set_int32_field(3);
  message.set_enum_field(TestMessage::ENUM_B);
  message.set_string_field("abc");
  message.set_bytes_field("\x01\x02\x03");

  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 1.0,
    "int32_field": 2,
    "nested_message_field": {
      "int32_field": 3
    },
    "enum_field": "ENUM_B",
    "string_field": "abc",
    "bytes_field": "AQID",
    "uint64_field": "0",
})!"));
}

TEST(ProtoToValueTest, Uint64Field) {
  TestMessage message;
  message.set_uint64_field(std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "18446744073709551615",
    })!"));
}

TEST(ProtoToValueTest, RepeatedField) {
  TestMessage message;

  // Default fields only if empty
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
})!"));

  message.add_repeated_int32_field(1);
  message.add_repeated_int32_field(2);
  message.add_repeated_int32_field(3);

  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "repeated_int32_field": [1, 2, 3],
    "uint64_field": "0",
})!"));
}

TEST(ProtoToValueTest, DepedentFile) {
  TestMessage message;
  message.mutable_dependency_message()->set_int32_field(4);

  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "dependency_message": {
      "int32_field": 4,
    },
    "uint64_field": "0",
})!"));
}

TEST(ProtoToValueTest, OneofField) {
  TestMessage message;

  // Test with maybe_int32_field set
  message.set_maybe_int32_field(100);
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_int32_field": 100,
    "uint64_field": "0",
})!"));

  // Test with maybe_string_field set
  message.set_maybe_string_field("hello oneof");
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_string_field": "hello oneof",
    "uint64_field": "0",
})!"));

  // Test with maybe_dependency_message set
  message.mutable_maybe_dependency_message()->set_int32_field(200);
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_dependency_message": {
      "int32_field": 200
    },
    "uint64_field": "0",
})!"));

  // Test with maybe_enum_field set
  message.set_maybe_enum_field(TestMessage::ENUM_A);
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_enum_field": "ENUM_A",
    "uint64_field": "0",
})!"));

  // Test with no oneof field set (should be same as default fields only)
  TestMessage message_oneof_unset;
  EXPECT_EQ(Serialize(message_oneof_unset), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
})!"));
}

TEST(ProtoToValueTest, UnknownFields) {
  TestMessage message;
  *message.mutable_unknown_fields() = "unknownfielddata";
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "unknown_fields": "dW5rbm93bmZpZWxkZGF0YQ==",
    "uint64_field": "0",
  })!"));
}

TEST(Proto2ToValueTest, EmbeddedMessageToValue) {
  EmbeddedMessage message;
  base::Value::Dict result = Serialize(message);
  EXPECT_EQ(0ul, result.size());
  message.set_str_field("test");
  result = Serialize(message);
  ASSERT_TRUE(result.FindString("str_field")) << result.DebugString();
  EXPECT_EQ("test", *result.FindString("str_field"));
  EXPECT_EQ(1ul, result.size());
}

TEST(Proto2ToValueTest, TestMessageToValue) {
  TestMessageProto2 message;
  const std::string expected_empty_message_str = R"({})";
  EXPECT_EQ(Serialize(message),
            base::JSONReader::Read(expected_empty_message_str));
  message.mutable_embedded_message()->set_str_field("test");
  message.add_repeated_embedded_message()->set_str_field("1");
  message.add_repeated_embedded_message()->set_str_field("2");
  message.set_int32_field(1);
  message.set_int64_field(-1ll);
  message.set_bytes_field("bytesbytes");
  message.add_repeated_int32_field(-1);
  message.add_repeated_int32_field(100);
  message.set_outer_enum(OUTER_ENUM_OPTION2);
  message.set_maybe_bool(true);
  message.set_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  message.add_repeated_inner_enum(TestMessageProto2::INNER_ENUM_UNSPECIFIED);
  message.add_repeated_inner_enum(TestMessageProto2::INNER_ENUM_OPTION2);
  message.mutable_inner_message()->set_int_field(99);
  message.set_bool_field(true);
  message.set_uint64_field(0);
  *message.mutable_unknown_fields() = "unknownfielddata";

  std::string expected_json_str = R"({
      "bool_field": true,
      "bytes_field": "Ynl0ZXNieXRlcw==",
      "embedded_message": {
          "str_field": "test"
      },
      "inner_enum": "INNER_ENUM_OPTION1",
      "inner_message": {
          "int_field": 99
      },
      "int32_field": 1,
      "int64_field": "-1",
      "maybe_bool": true,
      "outer_enum": "OUTER_ENUM_OPTION2",
      "repeated_embedded_message": [ {
          "str_field": "1"
      }, {
          "str_field": "2"
      } ],
      "repeated_inner_enum": [ "INNER_ENUM_UNSPECIFIED", "INNER_ENUM_OPTION2" ],
      "repeated_int32_field": [ -1, 100 ],
      "uint64_field": "0",
      "unknown_fields": "dW5rbm93bmZpZWxkZGF0YQ=="
    })";
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(expected_json_str));
}

TEST(Proto2ToValueTest, TestMessageProto2OneofToValue) {
  TestMessageProto2 message;
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(R"({})"));
  message.set_maybe_int(1);
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(R"({
    "maybe_int": 1
})"));
  message.set_maybe_bool(true);
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(R"({
    "maybe_bool": true
})"));
  message.mutable_maybe_message()->set_str_field("test");
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(R"({
    "maybe_message": {
      "str_field": "test"
    }
})"));
  message.set_maybe_enum(OUTER_ENUM_OPTION1);
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(R"({
    "maybe_enum": "OUTER_ENUM_OPTION1"
})"));
  message.set_maybe_bytes("test");
  EXPECT_EQ(Serialize(message), base::JSONReader::Read(R"({
    "maybe_bytes": "dGVzdA=="
})"));
}

TEST(Proto2ToValueTest, TestMessageProto2StreamOperator) {
  TestMessageProto2 message;
  message.set_maybe_int(1);
  std::ostringstream stream;
  stream << message;
  EXPECT_EQ(base::JSONReader::Read(stream.str()),
            base::JSONReader::Read(R"({"maybe_int": 1})"));
}

TEST(Proto2ToValueTest, Uint64Field) {
  TestMessageProto2 message;
  message.set_uint64_field(std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "uint64_field": "18446744073709551615"
  })!"));
}

}  // namespace
}  // namespace proto_extras
