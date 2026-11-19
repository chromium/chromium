// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_channel_sender.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;

const char kSenderIdFcmToken[] = "sender_id_fcm_token";
const char kSenderIdP256dh[] = "sender_id_p256dh";
const char kSenderIdAuthSecret[] = "sender_id_auth_secret";
const int kTtlSeconds = 10;
const char kServerConfiguration[] = "test_server_configuration";
const char kServerP256dh[] = "test_server_p256_dh";
const char kServerAuthSecret[] = "test_server_auth_secret";

// Creates a valid FCM channel configuration for test messages.
components_sharing_message::FCMChannelConfiguration CreateTestFcmChannel() {
  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);
  return fcm_channel;
}

// Creates a valid server channel configuration for test messages.
components_sharing_message::ServerChannelConfiguration
CreateTestServerChannel() {
  components_sharing_message::ServerChannelConfiguration server_channel;
  server_channel.set_configuration(kServerConfiguration);
  server_channel.set_p256dh(kServerP256dh);
  server_channel.set_auth_secret(kServerAuthSecret);
  return server_channel;
}

class FakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  FakeGCMDriver() = default;

  FakeGCMDriver(const FakeGCMDriver&) = delete;
  FakeGCMDriver& operator=(const FakeGCMDriver&) = delete;

  ~FakeGCMDriver() override = default;

  void EncryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const std::string& message,
                      EncryptMessageCallback callback) override {
    app_id_ = app_id;
    authorized_entity_ = authorized_entity;
    p256dh_ = p256dh;
    auth_secret_ = auth_secret;
    if (defer_encryption_) {
      pending_encryption_callback_ =
          base::BindOnce(std::move(callback),
                         gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08, message);
      return;
    }
    std::move(callback).Run(gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08,
                            message);
  }

  void set_defer_encryption(bool defer_encryption) {
    defer_encryption_ = defer_encryption;
  }

  void CompletePendingEncryption() {
    CHECK(pending_encryption_callback_);
    std::move(pending_encryption_callback_).Run();
  }

  const std::string& app_id() { return app_id_; }
  const std::string& authorized_entity() { return authorized_entity_; }
  const std::string& p256dh() { return p256dh_; }
  const std::string& auth_secret() { return auth_secret_; }

 private:
  std::string app_id_, authorized_entity_, p256dh_, auth_secret_;
  bool defer_encryption_ = false;
  base::OnceClosure pending_encryption_callback_;
};

class FakeSharingMessageBridge : public SharingMessageBridge {
 public:
  FakeSharingMessageBridge() = default;

  FakeSharingMessageBridge(const FakeSharingMessageBridge&) = delete;
  FakeSharingMessageBridge& operator=(const FakeSharingMessageBridge&) = delete;

  ~FakeSharingMessageBridge() override = default;

  // SharingMessageBridge:
  void SendSharingMessage(
      std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics,
      CommitFinishedCallback on_commit_callback) override {
    specifics_ = std::move(*specifics);
    sync_pb::SharingMessageCommitError commit_error;
    commit_error.set_error_code(error_code_);
    std::move(on_commit_callback).Run(commit_error);
  }

  // SharingMessageBridge:
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }

  const std::optional<sync_pb::SharingMessageSpecifics>& specifics() {
    return specifics_;
  }

  void set_error_code(
      const sync_pb::SharingMessageCommitError::ErrorCode& error_code) {
    error_code_ = error_code;
  }

 private:
  std::optional<sync_pb::SharingMessageSpecifics> specifics_;
  sync_pb::SharingMessageCommitError::ErrorCode error_code_ =
      sync_pb::SharingMessageCommitError::NONE;
};

class SharingChannelSenderTest : public testing::Test {
 public:
  void OnMessageSent(SharingSendMessageResult* result_out,
                     std::optional<std::string>* message_id_out,
                     SharingChannelType* channel_type_out,
                     SharingSendMessageResult result,
                     std::optional<std::string> message_id,
                     SharingChannelType channel_type) {
    *result_out = result;
    *message_id_out = std::move(message_id);
    *channel_type_out = channel_type;
  }

 protected:
  SharingChannelSenderTest()
      : sharing_channel_sender_(
            &fake_sharing_message_bridge_,
            &fake_gcm_driver_,
            fake_device_info_sync_service_.GetDeviceInfoTracker(),
            &fake_local_device_info_provider_,
            &test_sync_service_,
            mock_sync_flare_.Get()) {}

  // Sends an ACK message to a test FCM target.
  void SendAckMessageToFcmTarget(
      SharingChannelSender::SendMessageCallback callback) {
    components_sharing_message::SharingMessage sharing_message;
    sharing_message.mutable_ack_message();
    sharing_channel_sender_.SendMessageToFcmTarget(
        CreateTestFcmChannel(), base::Seconds(kTtlSeconds),
        std::move(sharing_message), std::move(callback));
  }

  // Sends an ACK message to a test server target.
  void SendAckMessageToServerTarget(
      SharingChannelSender::SendMessageCallback callback) {
    components_sharing_message::SharingMessage sharing_message;
    sharing_message.mutable_ack_message();
    sharing_channel_sender_.SendMessageToServerTarget(
        CreateTestServerChannel(), std::move(sharing_message),
        std::move(callback));
  }

  // Puts Sync into `INITIALIZING` state and queues an outgoing FCM ACK message.
  void QueuePendingAckMessage(
      SharingChannelSender::SendMessageCallback callback) {
    test_sync_service_.SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
    SendAckMessageToFcmTarget(std::move(callback));
  }

  // Verifies that sending to both FCM and server targets fails upfront with
  // `kInternalError` without triggering the Sync flare, encryption, or commit.
  void ExpectImmediateSendFails() {
    EXPECT_CALL(mock_sync_flare_, Run).Times(0);

    base::MockCallback<SharingChannelSender::SendMessageCallback> fcm_callback;
    EXPECT_CALL(fcm_callback,
                Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                    SharingChannelType::kFcmSenderId));
    SendAckMessageToFcmTarget(fcm_callback.Get());

    base::MockCallback<SharingChannelSender::SendMessageCallback>
        server_callback;
    EXPECT_CALL(server_callback,
                Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                    SharingChannelType::kServer));
    SendAckMessageToServerTarget(server_callback.Get());

    EXPECT_TRUE(fake_gcm_driver_.app_id().empty());
    EXPECT_FALSE(fake_sharing_message_bridge_.specifics().has_value());
  }

  FakeSharingMessageBridge fake_sharing_message_bridge_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  FakeGCMDriver fake_gcm_driver_;
  syncer::FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  syncer::TestSyncService test_sync_service_;
  // `NiceMock` suppresses uninteresting-call warnings in tests that queue
  // messages without asserting on the Sync flare.
  NiceMock<base::MockCallback<syncer::SyncableService::StartSyncFlare>>
      mock_sync_flare_;

  SharingChannelSender sharing_channel_sender_;
};

TEST_F(SharingChannelSenderTest, NoChannelsSpecified) {
  components_sharing_message::FCMChannelConfiguration fcm_channel;
  // Don't set any channels.

  SharingSendMessageResult result;
  std::optional<std::string> message_id;
  SharingChannelType channel_type;
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_channel_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingChannelSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id,
                     &channel_type));

  EXPECT_EQ(SharingSendMessageResult::kDeviceNotFound, result);
  EXPECT_FALSE(message_id);
  EXPECT_EQ(SharingChannelType::kUnknown, channel_type);
}

TEST_F(SharingChannelSenderTest, PreferSync) {
  fake_sharing_message_bridge_.set_error_code(
      sync_pb::SharingMessageCommitError::NONE);

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);

  SharingSendMessageResult result;
  std::optional<std::string> message_id;
  SharingChannelType channel_type;
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ping_message();
  sharing_channel_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingChannelSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id,
                     &channel_type));

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, result);
  // Ensures that a Ping message is sent through SharingMessageBridge.
  components_sharing_message::SharingMessage message_sent;
  ASSERT_TRUE(fake_sharing_message_bridge_.specifics());
  ASSERT_TRUE(fake_sharing_message_bridge_.specifics()->has_payload());
  message_sent.ParseFromString(
      fake_sharing_message_bridge_.specifics()->payload());
  EXPECT_TRUE(message_sent.has_ping_message());
}

struct CommitErrorCodeTestData {
  const sync_pb::SharingMessageCommitError::ErrorCode commit_error_code;
  const SharingSendMessageResult expected_result;
} kCommitErrorCodeTestData[] = {
    {sync_pb::SharingMessageCommitError::NONE,
     SharingSendMessageResult::kSuccessful},
    {sync_pb::SharingMessageCommitError::NOT_FOUND,
     SharingSendMessageResult::kDeviceNotFound},
    {sync_pb::SharingMessageCommitError::INVALID_ARGUMENT,
     SharingSendMessageResult::kPayloadTooLarge},
    {sync_pb::SharingMessageCommitError::INTERNAL,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::UNAVAILABLE,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::RESOURCE_EXHAUSTED,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::UNAUTHENTICATED,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::PERMISSION_DENIED,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::SYNC_NETWORK_ERROR,
     SharingSendMessageResult::kNetworkError},
    {sync_pb::SharingMessageCommitError::SYNC_SERVER_ERROR,
     SharingSendMessageResult::kInternalError},
    {sync_pb::SharingMessageCommitError::SYNC_TIMEOUT,
     SharingSendMessageResult::kCommitTimeout}};

class SharingChannelSenderCommitErrorCodeTest
    : public SharingChannelSenderTest,
      public testing::WithParamInterface<CommitErrorCodeTestData> {};

TEST_P(SharingChannelSenderCommitErrorCodeTest, ErrorCodeTest) {
  fake_sharing_message_bridge_.set_error_code(GetParam().commit_error_code);

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);

  SharingSendMessageResult result;
  std::optional<std::string> message_id;
  SharingChannelType channel_type;
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ping_message();
  sharing_channel_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingChannelSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id,
                     &channel_type));

  EXPECT_EQ(kSharingFCMAppID, fake_gcm_driver_.app_id());
  EXPECT_EQ(kSharingSenderID, fake_gcm_driver_.authorized_entity());
  EXPECT_EQ(kSenderIdP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kSenderIdAuthSecret, fake_gcm_driver_.auth_secret());

  auto specifics = fake_sharing_message_bridge_.specifics();
  ASSERT_TRUE(specifics);
  ASSERT_TRUE(specifics->has_channel_configuration());
  ASSERT_TRUE(specifics->channel_configuration().has_fcm());
  auto fcm = specifics->channel_configuration().fcm();
  EXPECT_EQ(kSenderIdFcmToken, fcm.token());
  EXPECT_EQ(kTtlSeconds, fcm.ttl());
  EXPECT_EQ(10, fcm.priority());

  components_sharing_message::SharingMessage message_sent;
  ASSERT_TRUE(specifics->has_payload());
  message_sent.ParseFromString(specifics->payload());
  EXPECT_TRUE(message_sent.has_ping_message());

  EXPECT_EQ(GetParam().expected_result, result);
  EXPECT_TRUE(message_id);
  EXPECT_EQ(SharingChannelType::kFcmSenderId, channel_type);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharingChannelSenderCommitErrorCodeTest,
                         testing::ValuesIn(kCommitErrorCodeTestData));

TEST_F(SharingChannelSenderTest, ServerTarget) {
  fake_sharing_message_bridge_.set_error_code(
      sync_pb::SharingMessageCommitError::NONE);

  components_sharing_message::ServerChannelConfiguration server_channel;
  server_channel.set_configuration(kServerConfiguration);
  server_channel.set_p256dh(kServerP256dh);
  server_channel.set_auth_secret(kServerAuthSecret);

  SharingSendMessageResult result;
  std::optional<std::string> message_id;
  SharingChannelType channel_type;
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ping_message();
  sharing_channel_sender_.SendMessageToServerTarget(
      server_channel, std::move(sharing_message),
      base::BindOnce(&SharingChannelSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id,
                     &channel_type));

  EXPECT_EQ(kSharingFCMAppID, fake_gcm_driver_.app_id());
  EXPECT_EQ(kSharingSenderID, fake_gcm_driver_.authorized_entity());
  EXPECT_EQ(kServerP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kServerAuthSecret, fake_gcm_driver_.auth_secret());

  auto specifics = fake_sharing_message_bridge_.specifics();
  ASSERT_TRUE(specifics);
  ASSERT_TRUE(specifics->has_channel_configuration());
  EXPECT_EQ(kServerConfiguration, specifics->channel_configuration().server());

  components_sharing_message::SharingMessage message_sent;
  ASSERT_TRUE(specifics->has_payload());
  message_sent.ParseFromString(specifics->payload());
  EXPECT_TRUE(message_sent.has_ping_message());

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, result);
  EXPECT_TRUE(message_id);
  EXPECT_EQ(SharingChannelType::kServer, channel_type);
}

// Tests that outgoing messages are queued during Sync startup (`START_DEFERRED`
// and `CONFIGURING`), trigger the Sync flare once, and replay once `ACTIVE`.
TEST_F(SharingChannelSenderTest, ShouldPostponeSendingMessageViaSync) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);

  base::MockCallback<SharingChannelSender::SendMessageCallback> fcm_callback;
  base::MockCallback<SharingChannelSender::SendMessageCallback> server_callback;
  EXPECT_CALL(fcm_callback, Run).Times(0);
  EXPECT_CALL(server_callback, Run).Times(0);

  // Sync flare should be called only once across multiple queued messages.
  EXPECT_CALL(mock_sync_flare_, Run(syncer::SHARING_MESSAGE));
  SendAckMessageToFcmTarget(fcm_callback.Get());
  SendAckMessageToServerTarget(server_callback.Get());
  Mock::VerifyAndClearExpectations(&fcm_callback);
  Mock::VerifyAndClearExpectations(&server_callback);

  // Messages stay queued while Sync is still configuring.
  EXPECT_CALL(fcm_callback, Run).Times(0);
  EXPECT_CALL(server_callback, Run).Times(0);
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.FireStateChanged();
  Mock::VerifyAndClearExpectations(&fcm_callback);
  Mock::VerifyAndClearExpectations(&server_callback);

  // Once Sync becomes active, all queued messages should be sent.
  EXPECT_CALL(fcm_callback, Run(SharingSendMessageResult::kSuccessful, _,
                                SharingChannelType::kFcmSenderId));
  EXPECT_CALL(server_callback, Run(SharingSendMessageResult::kSuccessful, _,
                                   SharingChannelType::kServer));
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that `FailPendingMessages()` fails all queued messages with
// `kInternalError` and prevents them from being sent once Sync becomes active.
TEST_F(SharingChannelSenderTest, ShouldFailPendingMessages) {
  base::MockCallback<SharingChannelSender::SendMessageCallback> fcm_callback;
  base::MockCallback<SharingChannelSender::SendMessageCallback> server_callback;
  EXPECT_CALL(fcm_callback, Run).Times(0);
  EXPECT_CALL(server_callback, Run).Times(0);
  QueuePendingAckMessage(fcm_callback.Get());
  SendAckMessageToServerTarget(server_callback.Get());
  Mock::VerifyAndClearExpectations(&fcm_callback);
  Mock::VerifyAndClearExpectations(&server_callback);

  EXPECT_CALL(fcm_callback,
              Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                  SharingChannelType::kFcmSenderId));
  EXPECT_CALL(server_callback,
              Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                  SharingChannelType::kServer));
  sharing_channel_sender_.FailPendingMessages();
  Mock::VerifyAndClearExpectations(&fcm_callback);
  Mock::VerifyAndClearExpectations(&server_callback);

  EXPECT_CALL(fcm_callback, Run).Times(0);
  EXPECT_CALL(server_callback, Run).Times(0);
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
  EXPECT_FALSE(fake_sharing_message_bridge_.specifics().has_value());
}

// Tests that queued messages fail when Sync transitions to `DISABLED`.
TEST_F(SharingChannelSenderTest, ShouldFailPendingMessagesWhenSyncDisabled) {
  base::MockCallback<SharingChannelSender::SendMessageCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  QueuePendingAckMessage(callback.Get());
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback,
              Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                  SharingChannelType::kFcmSenderId));
  test_sync_service_.SetSignedOut();
  test_sync_service_.FireStateChanged();
  EXPECT_FALSE(fake_sharing_message_bridge_.specifics().has_value());
}

// Tests that queued messages fail when Sync transitions to `PAUSED`.
TEST_F(SharingChannelSenderTest, ShouldFailPendingMessagesWhenSyncPaused) {
  base::MockCallback<SharingChannelSender::SendMessageCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  QueuePendingAckMessage(callback.Get());
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback,
              Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                  SharingChannelType::kFcmSenderId));
  test_sync_service_.SetPersistentAuthError();
  test_sync_service_.FireStateChanged();
  EXPECT_FALSE(fake_sharing_message_bridge_.specifics().has_value());
}

// Tests that queued messages fail when Sync reaches `ACTIVE` with
// `SHARING_MESSAGE` failed.
TEST_F(SharingChannelSenderTest,
       ShouldFailPendingMessagesWhenSharingMessageDataTypeFails) {
  base::MockCallback<SharingChannelSender::SendMessageCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  QueuePendingAckMessage(callback.Get());
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback,
              Run(SharingSendMessageResult::kInternalError, Eq(std::nullopt),
                  SharingChannelType::kFcmSenderId));
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
  EXPECT_FALSE(fake_sharing_message_bridge_.specifics().has_value());
}

// Tests that sending fails upfront before encryption when Sync is `DISABLED`.
TEST_F(SharingChannelSenderTest, ShouldFailImmediatelyWhenSyncDisabled) {
  test_sync_service_.SetSignedOut();
  ExpectImmediateSendFails();
}

// Tests that sending fails upfront before encryption when Sync is `PAUSED`.
TEST_F(SharingChannelSenderTest, ShouldFailImmediatelyWhenSyncPaused) {
  test_sync_service_.SetPersistentAuthError();
  ExpectImmediateSendFails();
}

// Tests that sending fails upfront before encryption when Sync is `ACTIVE` with
// `SHARING_MESSAGE` failed.
TEST_F(SharingChannelSenderTest,
       ShouldFailImmediatelyWhenSharingMessageDataTypeFailed) {
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  ExpectImmediateSendFails();
}

// Tests that in-flight encryption completing after Sync becomes disabled fails
// in `SendMessageViaSync()` without queueing or committing.
TEST_F(SharingChannelSenderTest, ShouldFailWhenSyncDisabledDuringEncryption) {
  fake_gcm_driver_.set_defer_encryption(true);

  base::MockCallback<SharingChannelSender::SendMessageCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  SendAckMessageToFcmTarget(callback.Get());
  Mock::VerifyAndClearExpectations(&callback);

  // Sync becomes disabled while `EncryptMessage()` is still in flight.
  test_sync_service_.SetSignedOut();

  EXPECT_CALL(mock_sync_flare_, Run).Times(0);
  EXPECT_CALL(callback, Run(SharingSendMessageResult::kInternalError,
                            Eq(std::nullopt), SharingChannelType::kFcmSenderId));
  fake_gcm_driver_.CompletePendingEncryption();
  EXPECT_FALSE(fake_sharing_message_bridge_.specifics().has_value());
}

}  // namespace
