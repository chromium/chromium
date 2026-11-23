// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_fcm_handler.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sharing_message/fake_sharing_handler_registry.h"
#include "components/sharing_message/mock_sharing_message_handler.h"
#include "components/sharing_message/mock_sharing_message_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Pointee;
using SharingMessage = components_sharing_message::SharingMessage;

namespace {

const char kTestAppId[] = "test_app_id";
const char kTestMessageId[] = "0:1563805165426489%0bb84dcff9fd7ecd";
const char kTestMessageIdSecondaryUser[] =
    "0:1563805165426489%20#0bb84dcff9fd7ecd";
const char kOriginalMessageId[] = "test_original_message_id";
const char kSenderGuid[] = "test_sender_guid";
const char kSenderIdFCMToken[] = "test_sender_id_fcm_token";
const char kSenderIdP256dh[] = "test_sender_id_p256_dh";
const char kSenderIdAuthSecret[] = "test_sender_id_auth_secret";
const char kServerConfiguration[] = "test_server_configuration";
const char kServerP256dh[] = "test_server_p256_dh";
const char kServerAuthSecret[] = "test_server_auth_secret";

void SetupFcmChannel(
    components_sharing_message::FCMChannelConfiguration* fcm_configuration) {
  fcm_configuration->set_sender_id_fcm_token(kSenderIdFCMToken);
  fcm_configuration->set_sender_id_p256dh(kSenderIdP256dh);
  fcm_configuration->set_sender_id_auth_secret(kSenderIdAuthSecret);
}

class SharingFCMHandlerTest : public testing::Test {
 protected:
  SharingFCMHandlerTest() {
    sharing_fcm_handler_ = std::make_unique<SharingFCMHandler>(
        &fake_gcm_driver_, &mock_sharing_message_sender_, &handler_registry_);
  }

  // Creates a gcm::IncomingMessage with SharingMessage and defaults.
  gcm::IncomingMessage CreateGCMIncomingMessage(
      const std::string& message_id,
      const SharingMessage& sharing_message) {
    gcm::IncomingMessage incoming_message;
    incoming_message.message_id = message_id;
    sharing_message.SerializeToString(&incoming_message.raw_data);
    return incoming_message;
  }

  base::test::TaskEnvironment task_environment_;

  FakeSharingHandlerRegistry handler_registry_;

  testing::NiceMock<MockSharingMessageHandler> mock_sharing_message_handler_;
  testing::NiceMock<MockSharingMessageSender> mock_sharing_message_sender_;

  gcm::FakeGCMDriver fake_gcm_driver_;
  std::unique_ptr<SharingFCMHandler> sharing_fcm_handler_;
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

MATCHER(FCMChannelMatcher, "") {
  return arg.sender_id_fcm_token() == kSenderIdFCMToken &&
         arg.sender_id_p256dh() == kSenderIdP256dh &&
         arg.sender_id_auth_secret() == kSenderIdAuthSecret;
}

MATCHER(ServerChannelMatcher, "") {
  return arg.configuration() == kServerConfiguration &&
         arg.p256dh() == kServerP256dh &&
         arg.auth_secret() == kServerAuthSecret;
}
// Tests handling of SharingMessage with AckMessage payload. This is different
// from other payloads since we need to ensure AckMessage is not sent for
// SharingMessage with AckMessage payload.
TEST_F(SharingFCMHandlerTest, AckMessageHandler) {
  SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kOriginalMessageId);
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  EXPECT_CALL(mock_sharing_message_handler_,
              OnMessage(ProtoEquals(sharing_message), _));
  EXPECT_CALL(mock_sharing_message_sender_, SendAckMessageToFcmTarget).Times(0);
  EXPECT_CALL(mock_sharing_message_sender_, SendAckMessageToServerTarget)
      .Times(0);
  handler_registry_.SetSharingHandler(SharingMessage::kAckMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Generic test for handling of SharingMessage payload other than AckMessage.
TEST_F(SharingFCMHandlerTest, PingMessageHandler) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  SetupFcmChannel(sharing_message.mutable_fcm_channel_configuration());
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  // Tests OnMessage flow in SharingFCMHandler when no handler is registered.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _)).Times(0);
  EXPECT_CALL(mock_sharing_message_sender_, SendAckMessageToFcmTarget).Times(0);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault([](const SharingMessage& message,
                        SharingMessageHandler::DoneCallback done_callback) {
        std::move(done_callback).Run(/*response=*/nullptr);
      });
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(mock_sharing_message_sender_,
              SendAckMessageToFcmTarget(FCMChannelMatcher(), Eq(kTestMessageId),
                                        IsNull(), _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after registered handler is
  // removed.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _)).Times(0);
  EXPECT_CALL(mock_sharing_message_sender_, SendAckMessageToFcmTarget).Times(0);
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage, nullptr);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

TEST_F(SharingFCMHandlerTest, PingMessageHandlerWithMessageIdInPayload) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  sharing_message.set_message_id(kTestMessageId);
  SetupFcmChannel(sharing_message.mutable_fcm_channel_configuration());
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(std::string(), sharing_message);

  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault([](const SharingMessage& message,
                        SharingMessageHandler::DoneCallback done_callback) {
        std::move(done_callback).Run(/*response=*/nullptr);
      });
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(mock_sharing_message_sender_,
              SendAckMessageToFcmTarget(FCMChannelMatcher(), Eq(kTestMessageId),
                                        IsNull(), _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

TEST_F(SharingFCMHandlerTest, PingMessageHandlerWithResponse) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  SetupFcmChannel(sharing_message.mutable_fcm_channel_configuration());
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault([](const SharingMessage& message,
                        SharingMessageHandler::DoneCallback done_callback) {
        std::move(done_callback)
            .Run(std::make_unique<
                 components_sharing_message::ResponseMessage>());
      });
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(
      mock_sharing_message_sender_,
      SendAckMessageToFcmTarget(
          FCMChannelMatcher(), Eq(kTestMessageId),
          Pointee(ProtoEquals(components_sharing_message::ResponseMessage())),
          _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Test for handling of SharingMessage payload other than AckMessage for
// secondary users in Android.
TEST_F(SharingFCMHandlerTest, PingMessageHandlerSecondaryUser) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  SetupFcmChannel(sharing_message.mutable_fcm_channel_configuration());
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageIdSecondaryUser, sharing_message);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault([](const SharingMessage& message,
                        SharingMessageHandler::DoneCallback done_callback) {
        std::move(done_callback).Run(/*response=*/nullptr);
      });
  EXPECT_CALL(mock_sharing_message_sender_,
              SendAckMessageToFcmTarget(FCMChannelMatcher(), kTestMessageId,
                                        IsNull(), _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

TEST_F(SharingFCMHandlerTest,
       PingMessageHandlerWithServerChannelConfiguration) {
  SharingMessage sharing_message;
  sharing_message.mutable_ping_message();
  components_sharing_message::ServerChannelConfiguration* server_configuration =
      sharing_message.mutable_server_channel_configuration();
  server_configuration->set_configuration(kServerConfiguration);
  server_configuration->set_p256dh(kServerP256dh);
  server_configuration->set_auth_secret(kServerAuthSecret);
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault([](const SharingMessage& message,
                        SharingMessageHandler::DoneCallback done_callback) {
        std::move(done_callback).Run(/*response=*/nullptr);
      });
  EXPECT_CALL(mock_sharing_message_sender_,
              SendAckMessageToServerTarget(ServerChannelMatcher(),
                                           kTestMessageId, IsNull(), _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}
