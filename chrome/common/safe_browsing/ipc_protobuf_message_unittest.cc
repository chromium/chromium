// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/ipc_protobuf_message_test.pb.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

#define IPC_MESSAGE_IMPL
#undef CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_TEST_MESSAGES_H_
#include "chrome/common/safe_browsing/ipc_protobuf_message_test_messages.h"

// Generate ipc protobuf traits write methods.
#include "chrome/common/safe_browsing/protobuf_message_write_macros.h"
namespace IPC {
#undef CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_TEST_MESSAGES_H_
#include "chrome/common/safe_browsing/ipc_protobuf_message_test_messages.h"
}  // namespace IPC

// Generate ipc protobuf traits read methods.
#include "chrome/common/safe_browsing/protobuf_message_read_macros.h"
namespace IPC {
#undef CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_TEST_MESSAGES_H_
#include "chrome/common/safe_browsing/ipc_protobuf_message_test_messages.h"
}  // namespace IPC

// Generate ipc protobuf traits log methods.
#include "chrome/common/safe_browsing/protobuf_message_log_macros.h"
namespace IPC {
#undef CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_TEST_MESSAGES_H_
#include "chrome/common/safe_browsing/ipc_protobuf_message_test_messages.h"
}  // namespace IPC

class IPCProtobufMessageTest : public ::testing::TestWithParam<bool> {
 protected:
  IPCProtobufMessageTest() : field_is_present_(GetParam()) {}

  bool field_is_present_;
};

// Tests writing and reading a message with an optional fundamental field.
TEST_P(IPCProtobufMessageTest, FundamentalField) {
  TestMessage input;

  if (field_is_present_)
    input.set_fund_int(42);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  TestMessage output;
  base::PickleIterator iter(msg);
  ASSERT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  if (field_is_present_) {
    ASSERT_TRUE(output.has_fund_int());
    EXPECT_EQ(input.fund_int(), output.fund_int());
  } else {
    ASSERT_FALSE(output.has_fund_int());
  }
}

// Tests writing and reading a message with an optional string field.
TEST_P(IPCProtobufMessageTest, StringField) {
  TestMessage input;

  if (field_is_present_)
    input.set_op_comp_string("some string");

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  TestMessage output;
  base::PickleIterator iter(msg);
  ASSERT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  if (field_is_present_) {
    ASSERT_TRUE(output.has_op_comp_string());
    EXPECT_EQ(input.op_comp_string(), output.op_comp_string());
  } else {
    ASSERT_FALSE(output.has_op_comp_string());
  }
}

// Tests writing and reading a message with an optional bytes field.
TEST_P(IPCProtobufMessageTest, BytesField) {
  TestMessage input;

  if (field_is_present_)
    input.set_op_comp_bytes("some string");

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  TestMessage output;
  base::PickleIterator iter(msg);
  ASSERT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  if (field_is_present_) {
    ASSERT_TRUE(output.has_op_comp_bytes());
    EXPECT_EQ(input.op_comp_bytes(), output.op_comp_bytes());
  } else {
    ASSERT_FALSE(output.has_op_comp_bytes());
  }
}

// Tests writing and reading a message with an optional submessage field.
TEST_P(IPCProtobufMessageTest, OptionalSubmessage) {
  TestMessage input;

  if (field_is_present_)
    input.mutable_op_comp_sub()->set_foo(47);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  TestMessage output;
  base::PickleIterator iter(msg);
  ASSERT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  if (field_is_present_) {
    ASSERT_TRUE(output.has_op_comp_sub());
    ASSERT_TRUE(output.op_comp_sub().has_foo());
    EXPECT_EQ(input.op_comp_sub().foo(), output.op_comp_sub().foo());
  } else {
    ASSERT_FALSE(output.has_op_comp_sub());
  }
}

// Tests writing and reading a message with a repeated submessage field.
TEST_P(IPCProtobufMessageTest, RepeatedSubmessage) {
  TestMessage input;

  if (field_is_present_) {
    input.add_rep_comp_sub()->set_foo(0);
    input.add_rep_comp_sub()->set_foo(1);
    input.add_rep_comp_sub()->set_foo(2);
  }

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  TestMessage output;
  base::PickleIterator iter(msg);
  ASSERT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  if (field_is_present_) {
    ASSERT_EQ(3, output.rep_comp_sub_size());
    ASSERT_TRUE(output.rep_comp_sub(0).has_foo());
    EXPECT_EQ(input.rep_comp_sub(0).foo(), output.rep_comp_sub(0).foo());
    ASSERT_TRUE(output.rep_comp_sub(1).has_foo());
    EXPECT_EQ(input.rep_comp_sub(1).foo(), output.rep_comp_sub(1).foo());
    ASSERT_TRUE(output.rep_comp_sub(2).has_foo());
    EXPECT_EQ(input.rep_comp_sub(2).foo(), output.rep_comp_sub(2).foo());
  } else {
    ASSERT_EQ(0, output.rep_comp_sub_size());
  }
}

INSTANTIATE_TEST_SUITE_P(IPCProtobufMessage,
                         IPCProtobufMessageTest,
                         ::testing::Bool());
