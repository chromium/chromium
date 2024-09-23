// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/ack_message_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/sharing_message/mock_sharing_message_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestMessageId[] = "test_message_id";

class AckMessageHandlerTest : public testing::Test {
 protected:
  AckMessageHandlerTest()
      : ack_message_handler_(&mock_response_callback_helper_) {}

  testing::NiceMock<MockSharingMessageSender> mock_response_callback_helper_;
  AckMessageHandler ack_message_handler_;
};

MATCHER_P(ProtoEquals, message, "") {
  if (!arg) {
    return false;
  }

  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg->SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

TEST_F(AckMessageHandlerTest, OnMessageNoResponse) {
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::Eq(nullptr)));

  EXPECT_CALL(mock_response_callback_helper_,
              OnAckReceived(testing::Eq(kTestMessageId), testing::Eq(nullptr)));

  ack_message_handler_.OnMessage(std::move(sharing_message),
                                 done_callback.Get());
}

TEST_F(AckMessageHandlerTest, OnMessageWithResponse) {
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_message.mutable_ack_message()->mutable_response_message();

  components_sharing_message::ResponseMessage response_message_copy =
      sharing_message.ack_message().response_message();

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::Eq(nullptr)));

  EXPECT_CALL(mock_response_callback_helper_,
              OnAckReceived(testing::Eq(kTestMessageId),
                            ProtoEquals(response_message_copy)));

  ack_message_handler_.OnMessage(std::move(sharing_message),
                                 done_callback.Get());
}
