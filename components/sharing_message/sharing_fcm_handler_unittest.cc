// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_fcm_handler.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sharing_message/fake_device_info.h"
#include "components/sharing_message/fake_sharing_handler_registry.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/mock_sharing_message_handler.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using SharingMessage = components_sharing_message::SharingMessage;

namespace {

const char kTestAppId[] = "test_app_id";
const char kTestMessageId[] = "0:1563805165426489%0bb84dcff9fd7ecd";
const char kTestMessageIdSecondaryUser[] =
    "0:1563805165426489%20#0bb84dcff9fd7ecd";
const char kOriginalMessageId[] = "test_original_message_id";
const char kSenderGuid[] = "test_sender_guid";
const char kSenderName[] = "test_sender_name";
const char kVapidFCMToken[] = "test_vapid_fcm_token";
const char kVapidP256dh[] = "test_vapid_p256_dh";
const char kVapidAuthSecret[] = "test_vapid_auth_secret";
const char kSenderIdFCMToken[] = "test_sender_id_fcm_token";
const char kSenderIdP256dh[] = "test_sender_id_p256_dh";
const char kSenderIdAuthSecret[] = "test_sender_id_auth_secret";
const char kServerConfiguration[] = "test_server_configuration";
const char kServerP256dh[] = "test_server_p256_dh";
const char kServerAuthSecret[] = "test_server_auth_secret";

void SetupFcmChannel(
    components_sharing_message::FCMChannelConfiguration* fcm_configuration) {
  fcm_configuration->set_vapid_fcm_token(kVapidFCMToken);
  fcm_configuration->set_vapid_p256dh(kVapidP256dh);
  fcm_configuration->set_vapid_auth_secret(kVapidAuthSecret);
  fcm_configuration->set_sender_id_fcm_token(kSenderIdFCMToken);
  fcm_configuration->set_sender_id_p256dh(kSenderIdP256dh);
  fcm_configuration->set_sender_id_auth_secret(kSenderIdAuthSecret);
}

class MockSharingFCMSender : public SharingFCMSender {
 public:
  MockSharingFCMSender()
      : SharingFCMSender(
            /*web_push_sender=*/nullptr,
            /*sharing_message_bridge=*/nullptr,
            /*sync_preference=*/nullptr,
            /*vapid_key_manager=*/nullptr,
            /*gcm_driver=*/nullptr,
            /*device_info_tracker=*/nullptr,
            /*local_device_info_provider=*/nullptr,
            /*sync_service=*/nullptr) {}
  ~MockSharingFCMSender() override = default;

  MOCK_METHOD4(SendMessageToFcmTarget,
               void(const components_sharing_message::FCMChannelConfiguration&
                        fcm_configuration,
                    base::TimeDelta time_to_live,
                    SharingMessage message,
                    SendMessageCallback callback));

  MOCK_METHOD3(
      SendMessageToServerTarget,
      void(const components_sharing_message::ServerChannelConfiguration&
               server_channel,
           SharingMessage message,
           SendMessageCallback callback));
};

class SharingFCMHandlerTest : public testing::Test {
 protected:
  SharingFCMHandlerTest() {
    sharing_fcm_handler_ = std::make_unique<SharingFCMHandler>(
        &fake_gcm_driver_,
        fake_device_info_sync_service_.GetDeviceInfoTracker(),
        &mock_sharing_fcm_sender_, &handler_registry_);
    fake_device_info_ = CreateFakeDeviceInfo(
        kSenderGuid, kSenderName,
        syncer::DeviceInfo::SharingInfo(
            {kVapidFCMToken, kVapidP256dh, kVapidAuthSecret},
            {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
            /*chime_representative_target_id=*/std::string(),
            std::set<sync_pb::SharingSpecificFields::EnabledFeatures>()));
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
  testing::NiceMock<MockSharingFCMSender> mock_sharing_fcm_sender_;

  gcm::FakeGCMDriver fake_gcm_driver_;
  std::unique_ptr<SharingFCMHandler> sharing_fcm_handler_;

  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;

  std::unique_ptr<syncer::DeviceInfo> fake_device_info_;
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

MATCHER(FCMChannelMatcher, "") {
  return arg.vapid_fcm_token() == kVapidFCMToken &&
         arg.vapid_p256dh() == kVapidP256dh &&
         arg.vapid_auth_secret() == kVapidAuthSecret &&
         arg.sender_id_fcm_token() == kSenderIdFCMToken &&
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
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToFcmTarget(_, _, _, _))
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

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  // Tests OnMessage flow in SharingFCMHandler when no handler is registered.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _)).Times(0);
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToFcmTarget(_, _, _, _))
      .Times(0);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(
      mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(FCMChannelMatcher(), Eq(kSharingAckMessageTTL),
                             ProtoEquals(sharing_ack_message), _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after registered handler is
  // removed.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _)).Times(0);
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToFcmTarget(_, _, _, _))
      .Times(0);
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

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(
      mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(FCMChannelMatcher(), Eq(kSharingAckMessageTTL),
                             ProtoEquals(sharing_ack_message), _));
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

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_ack_message.mutable_ack_message()->mutable_response_message();

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback)
                .Run(std::make_unique<
                     components_sharing_message::ResponseMessage>());
          }));
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(
      mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(FCMChannelMatcher(), Eq(kSharingAckMessageTTL),
                             ProtoEquals(sharing_ack_message), _));
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

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(
      mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(FCMChannelMatcher(), Eq(kSharingAckMessageTTL),
                             ProtoEquals(sharing_ack_message), _));
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

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToServerTarget(ServerChannelMatcher(),
                                        ProtoEquals(sharing_ack_message), _));
  handler_registry_.SetSharingHandler(SharingMessage::kPingMessage,
                                      &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}
