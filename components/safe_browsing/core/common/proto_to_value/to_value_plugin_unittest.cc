// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "components/safe_browsing/core/common/proto_to_value/test_proto/test_proto.pb.h"
#include "components/safe_browsing/core/common/proto_to_value/test_proto/test_proto.to_value.h"
#include "components/safe_browsing/core/common/proto_to_value/test_proto/test_proto_dependency.pb.h"
#include "components/safe_browsing/core/common/proto_to_value/test_proto/test_proto_dependency.to_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proto_to_value {

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
})!"));
}

TEST(ProtoToValueTest, RepeatedField) {
  TestMessage message;

  // Default fields only if empty
  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
})!"));

  message.add_repeated_int32_field(1);
  message.add_repeated_int32_field(2);
  message.add_repeated_int32_field(3);

  EXPECT_EQ(Serialize(message), base::test::ParseJson(R"!({
    "double_field": 0.0,
    "int32_field": 0,
    "enum_field": "UNKNOWN",
    "repeated_int32_field": [1, 2, 3],
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
})!"));
}

}  // namespace proto_to_value
