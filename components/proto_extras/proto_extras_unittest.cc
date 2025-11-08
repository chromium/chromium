// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "components/proto_extras/test_proto/test_proto.equal.h"
#include "components/proto_extras/test_proto/test_proto.ostream.h"
#include "components/proto_extras/test_proto/test_proto.pb.h"
#include "components/proto_extras/test_proto/test_proto.test.h"
#include "components/proto_extras/test_proto/test_proto.to_value.h"
#include "components/proto_extras/test_proto/test_proto_dependency.pb.h"
#include "components/proto_extras/test_proto/test_proto_dependency.to_value.h"
#include "components/proto_extras/test_proto2/test_proto2.equal.h"
#include "components/proto_extras/test_proto2/test_proto2.ostream.h"
#include "components/proto_extras/test_proto2/test_proto2.pb.h"
#include "components/proto_extras/test_proto2/test_proto2.test.h"
#include "components/proto_extras/test_proto2/test_proto2.to_value.h"
#include "components/proto_extras/test_proto_edition/test_proto_edition.pb.h"
#include "components/proto_extras/test_proto_edition/test_proto_edition.to_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proto_extras {
namespace {

TEST(ProtoExtrasToValueTest, BasicField) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
  })!"));

  message.set_double_field(1.0);
  message.set_int32_field(2);
  message.mutable_nested_message_field()->set_int32_field(3);
  message.set_enum_field(TestMessage::ENUM_B);
  message.set_string_field("abc");
  message.set_bytes_field("\x01\x02\x03");

  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
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

TEST(ProtoExtrasToValueTest, Uint64Field) {
  TestMessage message;
  message.set_uint64_field(std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "18446744073709551615",
    })!"));
}

TEST(ProtoExtrasToValueTest, RepeatedField) {
  TestMessage message;

  // Default fields only if empty
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
})!"));

  message.add_repeated_int32_field(1);
  message.add_repeated_int32_field(2);
  message.add_repeated_int32_field(3);

  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "repeated_int32_field": [1, 2, 3],
    "uint64_field": "0",
})!"));
}

TEST(ProtoExtrasToValueTest, DepedentFile) {
  TestMessage message;
  message.mutable_dependency_message()->set_int32_field(4);

  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "dependency_message": {
      "int32_field": 4,
    },
    "uint64_field": "0",
})!"));
}

TEST(ProtoExtrasToValueTest, OneofField) {
  TestMessage message;

  // Test with maybe_int32_field set
  message.set_maybe_int32_field(100);
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_int32_field": 100,
    "uint64_field": "0",
})!"));

  // Test with maybe_string_field set
  message.set_maybe_string_field("hello oneof");
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_string_field": "hello oneof",
    "uint64_field": "0",
})!"));

  // Test with maybe_dependency_message set
  message.mutable_maybe_dependency_message()->set_int32_field(200);
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
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
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "maybe_enum_field": "ENUM_A",
    "uint64_field": "0",
})!"));

  // Test with no oneof field set (should be same as default fields only)
  TestMessage message_oneof_unset;
  EXPECT_EQ(ToValue(message_oneof_unset), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
})!"));
}

TEST(ProtoExtrasToValueTest, MapField) {
  TestMessage message;
  (*message.mutable_primitive_map_field())[1] = "hello";
  DependencyMessage dependency_message;
  (*message.mutable_message_map_field())["hello"].set_int32_field(4);
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
    "primitive_map_field": {
      "1": "hello"
    },
    "message_map_field": {
      "hello": {
        "int32_field": 4
      }
    }
  })!"));
}

TEST(ProtoExtrasToValueTest, UnknownFields) {
  TestMessage message;
  *message.mutable_unknown_fields() = "unknownfielddata";
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "unknown_fields": "dW5rbm93bmZpZWxkZGF0YQ==",
    "uint64_field": "0",
  })!"));
}

TEST(ProtoExtrasProto2ToValueTest, EmbeddedMessageToValue) {
  EmbeddedMessage message;
  base::Value result = ToValue(message);
  EXPECT_TRUE(result.is_dict());
  EXPECT_EQ(0ul, result.GetDict().size());
  message.set_str_field("test");
  result = ToValue(message);
  EXPECT_TRUE(result.is_dict());
  ASSERT_TRUE(result.GetDict().FindString("str_field")) << result.DebugString();
  EXPECT_EQ("test", *result.GetDict().FindString("str_field"));
  EXPECT_EQ(1ul, result.GetDict().size());
}

TEST(ProtoExtrasToValueTest, EmptyEmbeddedMessage) {
  TestMessage message;
  message.mutable_empty_embedded_message();
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "empty_embedded_message": {},
    "uint64_field": "0",
  })!"));
}

TEST(ProtoExtrasToValueTest, CordBytesField) {
  TestMessage message;
  message.set_cord_bytes_field("123");
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "cord_bytes_field": "MTIz",
    "double_field": 0.0,
    "enum_field": "UNKNOWN",
    "int32_field": 0,
    "uint64_field": "0",
  })!"));
}

TEST(ProtoExtrasToValueTest, OptionalField) {
  TestMessage message;

  // By default, the optional field should not be present.
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
  })!"));

  message.set_optional_int_field(0);
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
    "optional_int_field": 0
  })!"));

  message.set_optional_int_field(123);
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
    "optional_int_field": 123
  })!"));
}

TEST(ProtoExtrasToValueTest, OptionalEmptyMessageField) {
  TestMessage message;

  // By default, the optional field should not be present.
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
  })!"));

  message.mutable_optional_empty_embedded_message_field();
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "uint64_field": "0",
    "optional_empty_embedded_message_field": {}
  })!"));
}

TEST(ProtoExtrasProto2ToValueTest, Basic) {
  TestMessageProto2 message;
  const std::string expected_empty_message_str = R"({})";
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(expected_empty_message_str,
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
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
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(expected_json_str,
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
}

TEST(ProtoExtrasProto2ToValueTest, OneofField) {
  TestMessageProto2 message;
  EXPECT_EQ(
      ToValue(message),
      base::JSONReader::Read(R"({})", base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  message.set_maybe_int(1);
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(R"({
    "maybe_int": 1
})",
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  message.set_maybe_bool(true);
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(R"({
    "maybe_bool": true
})",
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  message.mutable_maybe_message()->set_str_field("test");
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(R"({
    "maybe_message": {
      "str_field": "test"
    }
})",
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  message.set_maybe_enum(OUTER_ENUM_OPTION1);
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(R"({
    "maybe_enum": "OUTER_ENUM_OPTION1"
})",
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  message.set_maybe_bytes("test");
  EXPECT_EQ(ToValue(message),
            base::JSONReader::Read(R"({
    "maybe_bytes": "dGVzdA=="
})",
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
}

TEST(ProtoExtrasProto2ToValueTest, Uint64Field) {
  TestMessageProto2 message;
  message.set_uint64_field(std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "uint64_field": "18446744073709551615"
  })!"));
}

TEST(ProtoExtrasProto2ToValueTest, MapField) {
  TestMessageProto2 message;
  message.mutable_primitive_map_field()->insert({1, "hello"});
  (*message.mutable_message_map_field())["hello"].set_str_field("world");
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "primitive_map_field": {
      "1": "hello"
    },
    "message_map_field": {
      "hello": {
        "str_field": "world"
      }
    }
  })!"));
}

TEST(ProtoExtrasProto2StreamTest, Basic) {
  TestMessageProto2 message;
  message.set_maybe_int(1);
  std::ostringstream stream;
  stream << message;
  EXPECT_EQ(base::JSONReader::Read(stream.str(),
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS),
            base::JSONReader::Read(R"({"maybe_int": 1})",
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS));
}

TEST(ProtoExtrasEquality, Basic) {
  TestMessage msg1;
  TestMessage msg2;

  // Test default messages are equal.
  EXPECT_EQ(msg1, msg2);

  // Test setting a field makes them unequal.
  msg1.set_int32_field(1);
  EXPECT_NE(msg1, msg2);

  // Test setting the same field to the same value makes them equal.
  msg2.set_int32_field(1);
  EXPECT_EQ(msg1, msg2);

  // Test setting different values makes them unequal.
  msg2.set_int32_field(2);
  EXPECT_NE(msg1, msg2);
}

TEST(ProtoExtrasEquality, RepeatedField) {
  TestMessage msg1;
  TestMessage msg2;

  // Test repeated fields.
  msg1.add_repeated_int32_field(1);
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_int32_field(1);
  EXPECT_EQ(msg1, msg2);
  msg1.add_repeated_int32_field(2);
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_int32_field(3);
  EXPECT_NE(msg1, msg2);
  msg2.set_repeated_int32_field(1, 2);
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasEquality, OneofField) {
  TestMessage msg1;
  TestMessage msg2;

  // Test oneof fields.
  msg1.set_maybe_int32_field(100);
  EXPECT_NE(msg1, msg2);
  msg2.set_maybe_int32_field(100);
  EXPECT_EQ(msg1, msg2);
  msg2.set_maybe_string_field("test");
  EXPECT_NE(msg1, msg2);
  msg1.set_maybe_string_field("test");
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasEquality, NestedMessage) {
  TestMessage msg1;
  TestMessage msg2;

  // Test nested message
  msg1.mutable_nested_message_field()->set_int32_field(1);
  EXPECT_NE(msg1, msg2);
  msg2.mutable_nested_message_field()->set_int32_field(1);
  EXPECT_EQ(msg1, msg2);
  msg1.mutable_nested_message_field()->set_int32_field(2);
  EXPECT_NE(msg1, msg2);
}

TEST(ProtoExtrasEquality, EnumField) {
  TestMessage msg1;
  TestMessage msg2;
  // Test enum
  msg1.set_enum_field(TestMessage::ENUM_A);
  EXPECT_NE(msg1, msg2);
  msg2.set_enum_field(TestMessage::ENUM_A);
  EXPECT_EQ(msg1, msg2);
  msg2.set_enum_field(TestMessage::ENUM_B);
  EXPECT_NE(msg1, msg2);
}

TEST(ProtoExtrasEquality, OptionalField) {
  TestMessage msg1;
  TestMessage msg2;

  // Test default messages are equal.
  EXPECT_EQ(msg1, msg2);

  // Test setting an optional field makes them unequal.
  msg1.set_optional_int_field(0);
  EXPECT_NE(msg1, msg2);

  // Test setting the same optional field to the same value makes them equal.
  msg2.set_optional_int_field(0);
  EXPECT_EQ(msg1, msg2);

  // Test setting different values makes them unequal.
  msg2.set_optional_int_field(2);
  EXPECT_NE(msg1, msg2);
}

TEST(ProtoExtrasEquality, OptionalEmptyMessageField) {
  TestMessage msg1;
  TestMessage msg2;

  // Test default messages are equal.
  EXPECT_EQ(msg1, msg2);

  // Test setting an optional field makes them unequal.
  msg1.mutable_optional_empty_embedded_message_field();
  EXPECT_NE(msg1, msg2);

  // Test setting the same optional field to the same value makes them equal.
  msg2.mutable_optional_empty_embedded_message_field();
  EXPECT_EQ(msg1, msg2);
}


TEST(ProtoExtrasEquality, MapField) {
  TestMessage msg1;
  TestMessage msg2;

  (*msg1.mutable_primitive_map_field())[1] = "hello";
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_primitive_map_field())[1] = "hello";
  EXPECT_EQ(msg1, msg2);
  (*msg1.mutable_primitive_map_field())[2] = "world";
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_primitive_map_field())[2] = "world1";
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_primitive_map_field())[2] = "world";
  EXPECT_EQ(msg1, msg2);

  (*msg1.mutable_message_map_field())["hello"].set_int32_field(1);
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_message_map_field())["hello"].set_int32_field(1);
  EXPECT_EQ(msg1, msg2);
  (*msg1.mutable_message_map_field())["hello2"].set_int32_field(2);
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_message_map_field())["hello2"].set_int32_field(1);
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_message_map_field())["hello2"].set_int32_field(2);
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasProtoEqualityProto2, Basic) {
  TestMessageProto2 msg1;
  TestMessageProto2 msg2;

  // Test default messages are equal.
  EXPECT_EQ(msg1, msg2);

  // Test setting a field makes them unequal.
  msg1.set_int32_field(1);
  EXPECT_NE(msg1, msg2);

  // Test setting the same field to the same value makes them equal.
  msg2.set_int32_field(1);
  EXPECT_EQ(msg1, msg2);

  // Test setting different values makes them unequal.
  msg2.set_int32_field(2);
  EXPECT_NE(msg1, msg2);

  msg2.set_int32_field(1);
  EXPECT_EQ(msg1, msg2);

  // Test other basic types
  msg1.set_int64_field(100);
  msg2.set_int64_field(200);
  EXPECT_NE(msg1, msg2);
  msg2.set_int64_field(100);
  EXPECT_EQ(msg1, msg2);

  msg1.set_bytes_field("abc");
  msg2.set_bytes_field("def");
  EXPECT_NE(msg1, msg2);
  msg2.set_bytes_field("abc");
  EXPECT_EQ(msg1, msg2);

  msg1.set_bool_field(true);
  msg2.set_bool_field(false);
  EXPECT_NE(msg1, msg2);
  msg2.set_bool_field(true);
  EXPECT_EQ(msg1, msg2);

  msg1.set_uint64_field(std::numeric_limits<uint64_t>::max());
  msg2.set_uint64_field(0);
  EXPECT_NE(msg1, msg2);
  msg2.set_uint64_field(std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasProtoEqualityProto2, RepeatedField) {
  TestMessageProto2 msg1;
  TestMessageProto2 msg2;

  // Test repeated int32
  msg1.add_repeated_int32_field(1);
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_int32_field(1);
  EXPECT_EQ(msg1, msg2);
  msg1.add_repeated_int32_field(2);
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_int32_field(3);
  EXPECT_NE(msg1, msg2);
  msg2.set_repeated_int32_field(1, 2);
  EXPECT_EQ(msg1, msg2);

  // Test repeated embedded message
  msg1.add_repeated_embedded_message()->set_str_field("a");
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_embedded_message()->set_str_field("a");
  EXPECT_EQ(msg1, msg2);
  msg1.add_repeated_embedded_message()->set_str_field("b");
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_embedded_message()->set_str_field("c");
  EXPECT_NE(msg1, msg2);
  msg2.mutable_repeated_embedded_message(1)->set_str_field("b");
  EXPECT_EQ(msg1, msg2);

  // Test repeated inner enum
  msg1.add_repeated_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  EXPECT_EQ(msg1, msg2);
  msg1.add_repeated_inner_enum(TestMessageProto2::INNER_ENUM_OPTION2);
  EXPECT_NE(msg1, msg2);
  msg2.add_repeated_inner_enum(TestMessageProto2::INNER_ENUM_UNSPECIFIED);
  EXPECT_NE(msg1, msg2);
  msg2.set_repeated_inner_enum(1, TestMessageProto2::INNER_ENUM_OPTION2);
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasProtoEqualityProto2, OneofField) {
  TestMessageProto2 msg1;
  TestMessageProto2 msg2;

  // Test oneof fields.
  msg1.set_maybe_int(100);
  EXPECT_NE(msg1, msg2);
  msg2.set_maybe_int(100);
  EXPECT_EQ(msg1, msg2);

  msg2.set_maybe_bool(true);
  EXPECT_NE(msg1, msg2);
  msg1.set_maybe_bool(true);
  EXPECT_EQ(msg1, msg2);

  msg2.mutable_maybe_message()->set_str_field("test");
  EXPECT_NE(msg1, msg2);
  msg1.mutable_maybe_message()->set_str_field("test");
  EXPECT_EQ(msg1, msg2);

  msg2.set_maybe_enum(OUTER_ENUM_OPTION1);
  EXPECT_NE(msg1, msg2);
  msg1.set_maybe_enum(OUTER_ENUM_OPTION1);
  EXPECT_EQ(msg1, msg2);

  msg2.set_maybe_bytes("bytes");
  EXPECT_NE(msg1, msg2);
  msg1.set_maybe_bytes("bytes");
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasProtoEqualityProto2, NestedMessage) {
  TestMessageProto2 msg1;
  TestMessageProto2 msg2;

  // Test embedded message
  msg1.mutable_embedded_message()->set_str_field("a");
  EXPECT_NE(msg1, msg2);
  msg2.mutable_embedded_message()->set_str_field("a");
  EXPECT_EQ(msg1, msg2);
  msg1.mutable_embedded_message()->set_str_field("b");
  EXPECT_NE(msg1, msg2);
  msg2.mutable_embedded_message()->set_str_field("b");
  EXPECT_EQ(msg1, msg2);

  // Test inner message
  msg1.mutable_inner_message()->set_int_field(1);
  EXPECT_NE(msg1, msg2);
  msg2.mutable_inner_message()->set_int_field(1);
  EXPECT_EQ(msg1, msg2);
  msg1.mutable_inner_message()->set_int_field(2);
  EXPECT_NE(msg1, msg2);
}

TEST(ProtoExtrasProtoEqualityProto2, EnumField) {
  TestMessageProto2 msg1;
  TestMessageProto2 msg2;

  // Test outer enum
  msg1.set_outer_enum(OUTER_ENUM_OPTION1);
  EXPECT_NE(msg1, msg2);
  msg2.set_outer_enum(OUTER_ENUM_OPTION1);
  EXPECT_EQ(msg1, msg2);
  msg2.set_outer_enum(OUTER_ENUM_OPTION2);
  EXPECT_NE(msg1, msg2);

  msg2.set_outer_enum(OUTER_ENUM_OPTION1);

  // Test inner enum
  msg1.set_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  EXPECT_NE(msg1, msg2);
  msg2.set_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  EXPECT_EQ(msg1, msg2);
  msg2.set_inner_enum(TestMessageProto2::INNER_ENUM_OPTION2);
  EXPECT_NE(msg1, msg2);
}

TEST(ProtoExtrasProtoEqualityProto2, MapField) {
  TestMessageProto2 msg1;
  TestMessageProto2 msg2;

  (*msg1.mutable_primitive_map_field())[1] = "hello";
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_primitive_map_field())[1] = "hello";
  EXPECT_EQ(msg1, msg2);
  (*msg1.mutable_primitive_map_field())[2] = "world";
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_primitive_map_field())[2] = "world1";
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_primitive_map_field())[2] = "world";
  EXPECT_EQ(msg1, msg2);

  (*msg1.mutable_message_map_field())["hello"].set_str_field("world");
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_message_map_field())["hello"].set_str_field("world");
  EXPECT_EQ(msg1, msg2);
  (*msg1.mutable_message_map_field())["hello2"].set_str_field("world2");
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_message_map_field())["hello2"].set_str_field("world1");
  EXPECT_NE(msg1, msg2);
  (*msg2.mutable_message_map_field())["hello2"].set_str_field("world2");
  EXPECT_EQ(msg1, msg2);
}

TEST(ProtoExtrasProto2ToValueTest, EmptyEmbeddedMessageToValue) {
  TestMessageProto2 message;
  message.mutable_empty_embedded_message();
  base::Value result = ToValue(message);
  ASSERT_TRUE(result.is_dict());
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "empty_embedded_message": {}
  })!"));
}

TEST(ProtoExtrasProto2ToValueTest, TestEditionMessage) {
  TestMessageEdition message;
  message.set_text("test");
  base::Value result = ToValue(message);
  ASSERT_TRUE(result.is_dict());
  EXPECT_EQ(ToValue(message), base::test::ParseJson(R"!({
    "text": "test"
  })!"));
}

}  // namespace
}  // namespace proto_extras
