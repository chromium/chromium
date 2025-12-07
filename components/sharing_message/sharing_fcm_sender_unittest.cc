// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_fcm_sender.h"

#include <memory>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

const char kSenderIdFcmToken[] = "sender_id_fcm_token";
const char kSenderIdP256dh[] = "sender_id_p256dh";
const char kSenderIdAuthSecret[] = "sender_id_auth_secret";
const int kTtlSeconds = 10;
const char kServerConfiguration[] = "test_server_configuration";
const char kServerP256dh[] = "test_server_p256_dh";
const char kServerAuthSecret[] = "test_server_auth_secret";

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
    std::move(callback).Run(gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08,
                            message);
  }

  const std::string& app_id() { return app_id_; }
  const std::string& authorized_entity() { return authorized_entity_; }
  const std::string& p256dh() { return p256dh_; }
  const std::string& auth_secret() { return auth_secret_; }

 private:
  std::string app_id_, authorized_entity_, p256dh_, auth_secret_;
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

class SharingFCMSenderTest : public testing::Test {
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
  SharingFCMSenderTest()
      : sync_prefs_(&prefs_, &fake_device_info_sync_service_),
        sharing_fcm_sender_(
            &fake_sharing_message_bridge_,
            &sync_prefs_,
            &fake_gcm_driver_,
            fake_device_info_sync_service_.GetDeviceInfoTracker(),
            &fake_local_device_info_provider_,
            &test_sync_service_,
            mock_sync_flare_.Get()) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeSharingMessageBridge fake_sharing_message_bridge_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sync_prefs_;
  FakeGCMDriver fake_gcm_driver_;
  syncer::FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  syncer::TestSyncService test_sync_service_;
  base::MockCallback<syncer::SyncableService::StartSyncFlare> mock_sync_flare_;

  SharingFCMSender sharing_fcm_sender_;
};

TEST_F(SharingFCMSenderTest, NoFcmRegistration) {
  sync_prefs_.ClearFCMRegistration();

  // Do not populate sender ID channel.
  components_sharing_message::FCMChannelConfiguration fcm_channel;

  SharingSendMessageResult result;
  std::optional<std::string> message_id;
  SharingChannelType channel_type;
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_fcm_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id,
                     &channel_type));

  EXPECT_EQ(SharingSendMessageResult::kDeviceNotFound, result);
  EXPECT_FALSE(message_id);
  EXPECT_EQ(SharingChannelType::kUnknown, channel_type);
}

TEST_F(SharingFCMSenderTest, NoChannelsSpecified) {
  sync_prefs_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(base::Time::Now()));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  // Don't set any channels.

  SharingSendMessageResult result;
  std::optional<std::string> message_id;
  SharingChannelType channel_type;
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_fcm_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id,
                     &channel_type));

  EXPECT_EQ(SharingSendMessageResult::kDeviceNotFound, result);
  EXPECT_FALSE(message_id);
  EXPECT_EQ(SharingChannelType::kUnknown, channel_type);
}

TEST_F(SharingFCMSenderTest, PreferSync) {
  sync_prefs_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(base::Time::Now()));

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
  sharing_fcm_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
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

class SharingFCMSenderCommitErrorCodeTest
    : public SharingFCMSenderTest,
      public testing::WithParamInterface<CommitErrorCodeTestData> {};

TEST_P(SharingFCMSenderCommitErrorCodeTest, ErrorCodeTest) {
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
  sharing_fcm_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
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
                         SharingFCMSenderCommitErrorCodeTest,
                         testing::ValuesIn(kCommitErrorCodeTestData));

TEST_F(SharingFCMSenderTest, ServerTarget) {
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
  sharing_fcm_sender_.SendMessageToServerTarget(
      server_channel, std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
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

TEST_F(SharingFCMSenderTest, ShouldPostponeSendingMessageViaSync) {
  // Make sync unavailable to simulate browser startup.
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  sync_prefs_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(base::Time::Now()));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);

  base::MockCallback<
      SharingMessageSender::SendMessageDelegate::SendMessageCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(0);

  // Sync flare should be called to speed up sync start.
  EXPECT_CALL(mock_sync_flare_, Run(syncer::SHARING_MESSAGE));
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_fcm_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      callback.Get());
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Once SHARING_MESSAGE becomes active, the message should be sent.
  EXPECT_CALL(callback, Run(SharingSendMessageResult::kSuccessful, _,
                            SharingChannelType::kFcmSenderId));
  test_sync_service_.SetFailedDataTypes({});
  test_sync_service_.FireStateChanged();
}

TEST_F(SharingFCMSenderTest, ShouldClearPendingMessages) {
  // Make sync unavailable to simulate browser startup.
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  sync_prefs_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(base::Time::Now()));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);

  base::MockCallback<
      SharingMessageSender::SendMessageDelegate::SendMessageCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(0);
  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_fcm_sender_.SendMessageToFcmTarget(
      fcm_channel, base::Seconds(kTtlSeconds), std::move(sharing_message),
      callback.Get());

  // Clear any pending messages and verify that nothing is sent once
  // SHARING_MESSAGE becomes active (the `callback` above should not be called).
  sharing_fcm_sender_.ClearPendingMessages();

  test_sync_service_.SetFailedDataTypes({});
  test_sync_service_.FireStateChanged();
}

}  // namespace
