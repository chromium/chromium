// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proto_extras/proto_matchers.h"
#include "components/proto_extras/test_proto/test_proto.pb.h"
#include "components/proto_extras/test_proto/test_proto.test.h"
#include "components/proto_extras/test_proto/test_proto_dependency.pb.h"
#include "components/proto_extras/test_proto/test_proto_dependency.test.h"
#include "components/proto_extras/test_proto2/test_proto2.pb.h"
#include "components/proto_extras/test_proto2/test_proto2.test.h"
#include "components/proto_extras/test_proto_edition/test_proto_edition.pb.h"
#include "components/proto_extras/test_proto_edition/test_proto_edition.test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proto_extras {
namespace {
using ::testing::Not;

TEST(ProtoTestExtras, NestedMessage) {
  TestMessage::NestedMessage nested_message;
  EXPECT_THAT(nested_message,
              EqualsTestMessage_NestedMessage(TestMessage_NestedMessage()));

  nested_message.set_int32_field(1);
  EXPECT_THAT(
      nested_message,
      Not(EqualsTestMessage_NestedMessage(TestMessage_NestedMessage())));

  TestMessage::NestedMessage nested_message2;
  nested_message2.set_int32_field(1);
  EXPECT_THAT(nested_message, EqualsTestMessage_NestedMessage(nested_message2));
}

TEST(ProtoTestExtras, NestedMessageDependency) {
  TestMessage::NestedMessage nested_message;
  EXPECT_THAT(nested_message,
              EqualsTestMessage_NestedMessage(TestMessage_NestedMessage()));
}

TEST(ProtoTestExtras, TestMessagePrimitiveFields) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.set_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));
  TestMessage message2;
  message2.set_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.set_string_field("abc");
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.set_string_field("abc");
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.set_bytes_field("\x01\x02\x03");
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.set_bytes_field("\x01\x02\x03");
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.set_double_field(1.0);
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.set_double_field(1.0);
  EXPECT_THAT(message, EqualsTestMessage(message2));
}

TEST(ProtoTestExtras, TestMessageRepeatedFields) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.add_repeated_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));
  TestMessage message2;
  message2.add_repeated_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.add_repeated_int32_field(2);
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.add_repeated_int32_field(2);
  EXPECT_THAT(message, EqualsTestMessage(message2));
}

TEST(ProtoTestExtras, TestMessageOneofFields) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.set_maybe_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));
  TestMessage message2;
  message2.set_maybe_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.set_maybe_string_field("abc");
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.set_maybe_string_field("abc");
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.mutable_maybe_dependency_message()->set_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.mutable_maybe_dependency_message()->set_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(message2));
}

TEST(ProtoTestExtras, TestEnumFields) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.set_enum_field(TestMessage::ENUM_A);
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));
  TestMessage message2;
  message2.set_enum_field(TestMessage::ENUM_A);
  EXPECT_THAT(message, EqualsTestMessage(message2));
}

TEST(ProtoTestExtras, TestNestedMessages) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.mutable_nested_message_field()->set_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));
  TestMessage message2;
  message2.mutable_nested_message_field()->set_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message.mutable_dependency_message()->set_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
  message2.mutable_dependency_message()->set_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(message2));
}

TEST(ProtoTestExtras, TestMapFields) {
  TestMessage message;
  TestMessage expected;

  (*message.mutable_primitive_map_field())[1] = "hello";
  EXPECT_THAT(message, Not(EqualsTestMessage(expected)));
  (*expected.mutable_primitive_map_field())[1] = "hello";
  EXPECT_THAT(message, EqualsTestMessage(expected));

  (*message.mutable_primitive_map_field())[2] = "world";
  EXPECT_THAT(message, Not(EqualsTestMessage(expected)));
  (*expected.mutable_primitive_map_field())[2] = "world1";
  EXPECT_THAT(message, Not(EqualsTestMessage(expected)));
  (*expected.mutable_primitive_map_field())[2] = "world";
  EXPECT_THAT(message, EqualsTestMessage(expected));

  (*message.mutable_message_map_field())["hello"].set_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessage(expected)));
  (*expected.mutable_message_map_field())["hello"].set_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessage(expected));
  (*message.mutable_message_map_field())["hello"].set_int32_field(2);
  EXPECT_THAT(message, Not(EqualsTestMessage(expected)));
  (*expected.mutable_message_map_field())["hello"].set_int32_field(3);
  EXPECT_THAT(message, Not(EqualsTestMessage(expected)));
  (*expected.mutable_message_map_field())["hello"].set_int32_field(2);
  EXPECT_THAT(message, EqualsTestMessage(expected));
}

TEST(ProtoTestExtras, TestOptionalFields) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.set_optional_int_field(0);
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));

  TestMessage message2;
  message2.set_optional_int_field(0);
  EXPECT_THAT(message, EqualsTestMessage(message2));

  message2.set_optional_int_field(2);
  EXPECT_THAT(message, Not(EqualsTestMessage(message2)));
}

TEST(ProtoTestExtras, TestOptionalEmptyMessageFields) {
  TestMessage message;
  EXPECT_THAT(message, EqualsTestMessage(TestMessage()));

  message.mutable_optional_empty_embedded_message_field();
  EXPECT_THAT(message, Not(EqualsTestMessage(TestMessage())));

  TestMessage message2;
  message2.mutable_optional_empty_embedded_message_field();
  EXPECT_THAT(message, EqualsTestMessage(message2));
}

TEST(ProtoTestExtrasProto2, TestMessage) {
  TestMessageProto2 message;
  EXPECT_THAT(message, EqualsTestMessageProto2(TestMessageProto2()));
}

TEST(ProtoTestExtrasProto2, TestMessageProto2PrimitiveFields) {
  TestMessageProto2 message;
  EXPECT_THAT(message, EqualsTestMessageProto2(TestMessageProto2()));

  message.set_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(TestMessageProto2())));
  TestMessageProto2 message2;
  message2.set_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessageProto2(message2));

  message.set_uint64_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(message2)));
  message2.set_uint64_field(1);
  EXPECT_THAT(message, EqualsTestMessageProto2(message2));

  message.set_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(message2)));
  message2.set_inner_enum(TestMessageProto2::INNER_ENUM_OPTION1);
  EXPECT_THAT(message, EqualsTestMessageProto2(message2));
}

TEST(ProtoTestExtrasProto2, RepeatedFields) {
  TestMessageProto2 message;
  EXPECT_THAT(message, EqualsTestMessageProto2(TestMessageProto2()));

  message.add_repeated_int32_field(1);
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(TestMessageProto2())));
  TestMessageProto2 message2;
  message2.add_repeated_int32_field(1);
  EXPECT_THAT(message, EqualsTestMessageProto2(message2));

  message.add_repeated_embedded_message()->set_str_field("abc");
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(message2)));
  message2.add_repeated_embedded_message()->set_str_field("abc");
  EXPECT_THAT(message, EqualsTestMessageProto2(message2));
}

TEST(ProtoTestExtrasProto2, MapFields) {
  TestMessageProto2 message;
  TestMessageProto2 expected;

  EXPECT_THAT(message, EqualsTestMessageProto2(expected));

  (*message.mutable_primitive_map_field())[1] = "hello";
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(expected)));
  (*expected.mutable_primitive_map_field())[1] = "hello";
  EXPECT_THAT(message, EqualsTestMessageProto2(expected));

  (*message.mutable_primitive_map_field())[2] = "world";
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(expected)));
  (*expected.mutable_primitive_map_field())[2] = "world1";
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(expected)));
  (*expected.mutable_primitive_map_field())[2] = "world";
  EXPECT_THAT(message, EqualsTestMessageProto2(expected));

  (*message.mutable_message_map_field())["hello"].set_str_field("world");
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(expected)));
  (*expected.mutable_message_map_field())["hello"].set_str_field("world");
  EXPECT_THAT(message, EqualsTestMessageProto2(expected));

  (*message.mutable_message_map_field())["hello2"].set_str_field("world2");
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(expected)));
  (*expected.mutable_message_map_field())["hello2"].set_str_field("world3");
  EXPECT_THAT(message, Not(EqualsTestMessageProto2(expected)));
  (*expected.mutable_message_map_field())["hello2"].set_str_field("world2");
  EXPECT_THAT(message, EqualsTestMessageProto2(expected));
}

TEST(ProtoTestExtrasProtoEdition, TestMessage) {
  TestMessageEdition message;
  message.set_text("foo");
  TestMessageEdition message2;
  message2.set_text("bar");
  TestMessageEdition expected;
  expected.set_text("foo");

  EXPECT_THAT(message, EqualsTestMessageEdition(expected));
  EXPECT_THAT(message, Not(EqualsTestMessageEdition(message2)));
}

}  // namespace
}  // namespace proto_extras
