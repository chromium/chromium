// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_fcm_sender.h"

#include <memory>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sharing_message/vapid_key_manager.h"
#include "components/sharing_message/web_push/web_push_sender.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kMessageId[] = "message_id";
const char kVapidFcmToken[] = "vapid_fcm_token";
const char kVapidP256dh[] = "vapid_p256dh";
const char kVapidAuthSecret[] = "vapid_id_auth_secret";
const char kSenderIdFcmToken[] = "sender_id_fcm_token";
const char kSenderIdP256dh[] = "sender_id_p256dh";
const char kSenderIdAuthSecret[] = "sender_id_auth_secret";
const char kAuthorizedEntity[] = "authorized_entity";
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

class FakeWebPushSender : public WebPushSender {
 public:
  FakeWebPushSender() : WebPushSender(/*url_loader_factory=*/nullptr) {}

  FakeWebPushSender(const FakeWebPushSender&) = delete;
  FakeWebPushSender& operator=(const FakeWebPushSender&) = delete;

  ~FakeWebPushSender() override = default;

  void SendMessage(const std::string& fcm_token,
                   crypto::ECPrivateKey* vapid_key,
                   WebPushMessage message,
                   WebPushCallback callback) override {
    fcm_token_ = fcm_token;
    vapid_key_ = vapid_key;
    message_ = std::move(message);
    std::move(callback).Run(result_,
                            std::make_optional<std::string>(kMessageId));
  }

  const std::string& fcm_token() { return fcm_token_; }
  crypto::ECPrivateKey* vapid_key() { return vapid_key_; }
  const std::optional<WebPushMessage>& message() { return message_; }

  void set_result(SendWebPushMessageResult result) { result_ = result; }

 private:
  std::string fcm_token_;
  raw_ptr<crypto::ECPrivateKey, DanglingUntriaged> vapid_key_;
  std::optional<WebPushMessage> message_;
  SendWebPushMessageResult result_;
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
  sync_pb::SharingMessageCommitError::ErrorCode error_code_;
};

class MockVapidKeyManager : public VapidKeyManager {
 public:
  MockVapidKeyManager()
      : VapidKeyManager(/*sharing_sync_preference=*/nullptr,
                        /*sync_service=*/nullptr) {}
  ~MockVapidKeyManager() override = default;

  MOCK_METHOD0(GetOrCreateKey, crypto::ECPrivateKey*());
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
      : fake_web_push_sender_(new FakeWebPushSender()),
        sync_prefs_(&prefs_, &fake_device_info_sync_service_),
        sharing_fcm_sender_(
            base::WrapUnique(fake_web_push_sender_.get()),
            &fake_sharing_message_bridge_,
            &sync_prefs_,
            &vapid_key_manager_,
            &fake_gcm_driver_,
            fake_device_info_sync_service_.GetDeviceInfoTracker(),
            &fake_local_device_info_provider_,
            &test_sync_service_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  raw_ptr<FakeWebPushSender, DanglingUntriaged> fake_web_push_sender_;
  FakeSharingMessageBridge fake_sharing_message_bridge_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sync_prefs_;
  testing::NiceMock<MockVapidKeyManager> vapid_key_manager_;
  FakeGCMDriver fake_gcm_driver_;
  syncer::FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  syncer::TestSyncService test_sync_service_;

  SharingFCMSender sharing_fcm_sender_;
};  // namespace

}  // namespace

TEST_F(SharingFCMSenderTest, NoFcmRegistration) {
  // Make sync unavailable to force using vapid.
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  sync_prefs_.ClearFCMRegistration();

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(vapid_key.get()));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_vapid_fcm_token(kVapidFcmToken);
  fcm_channel.set_vapid_p256dh(kVapidP256dh);
  fcm_channel.set_vapid_auth_secret(kVapidAuthSecret);
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);

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

  EXPECT_EQ(SharingSendMessageResult::kInternalError, result);
  EXPECT_FALSE(message_id);
  EXPECT_EQ(SharingChannelType::kUnknown, channel_type);
}

TEST_F(SharingFCMSenderTest, NoVapidKey) {
  // Make sync unavailable to force using vapid.
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  sync_prefs_.SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(nullptr));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_vapid_fcm_token(kVapidFcmToken);
  fcm_channel.set_vapid_p256dh(kVapidP256dh);
  fcm_channel.set_vapid_auth_secret(kVapidAuthSecret);
  fcm_channel.set_sender_id_fcm_token(kSenderIdFcmToken);
  fcm_channel.set_sender_id_p256dh(kSenderIdP256dh);
  fcm_channel.set_sender_id_auth_secret(kSenderIdAuthSecret);

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

  EXPECT_EQ(SharingSendMessageResult::kInternalError, result);
  EXPECT_FALSE(message_id);
  EXPECT_EQ(SharingChannelType::kFcmVapid, channel_type);
}

TEST_F(SharingFCMSenderTest, NoChannelsSpecified) {
  sync_prefs_.SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(vapid_key.get()));

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
  sync_prefs_.SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  fake_web_push_sender_->set_result(SendWebPushMessageResult::kSuccessful);
  fake_sharing_message_bridge_.set_error_code(
      sync_pb::SharingMessageCommitError::NONE);

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(vapid_key.get()));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  // Set both VAPID and Sender ID channel.
  fcm_channel.set_vapid_fcm_token(kVapidFcmToken);
  fcm_channel.set_vapid_p256dh(kVapidP256dh);
  fcm_channel.set_vapid_auth_secret(kVapidAuthSecret);
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
  // Ensures that no message is sent through WebPushSender.
  EXPECT_FALSE(fake_web_push_sender_->message());
}

struct WebPushResultTestData {
  const SendWebPushMessageResult web_push_result;
  const SharingSendMessageResult expected_result;
} kWebPushResultTestData[] = {{SendWebPushMessageResult::kSuccessful,
                               SharingSendMessageResult::kSuccessful},
                              {SendWebPushMessageResult::kSuccessful,
                               SharingSendMessageResult::kSuccessful},
                              {SendWebPushMessageResult::kDeviceGone,
                               SharingSendMessageResult::kDeviceNotFound},
                              {SendWebPushMessageResult::kNetworkError,
                               SharingSendMessageResult::kNetworkError},
                              {SendWebPushMessageResult::kPayloadTooLarge,
                               SharingSendMessageResult::kPayloadTooLarge},
                              {SendWebPushMessageResult::kEncryptionFailed,
                               SharingSendMessageResult::kInternalError},
                              {SendWebPushMessageResult::kCreateJWTFailed,
                               SharingSendMessageResult::kInternalError},
                              {SendWebPushMessageResult::kServerError,
                               SharingSendMessageResult::kInternalError},
                              {SendWebPushMessageResult::kParseResponseFailed,
                               SharingSendMessageResult::kInternalError},
                              {SendWebPushMessageResult::kVapidKeyInvalid,
                               SharingSendMessageResult::kInternalError}};

class SharingFCMSenderWebPushResultTest
    : public SharingFCMSenderTest,
      public testing::WithParamInterface<WebPushResultTestData> {};

TEST_P(SharingFCMSenderWebPushResultTest, ResultTest) {
  sync_prefs_.SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));
  fake_web_push_sender_->set_result(GetParam().web_push_result);

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(vapid_key.get()));

  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_vapid_fcm_token(kVapidFcmToken);
  fcm_channel.set_vapid_p256dh(kVapidP256dh);
  fcm_channel.set_vapid_auth_secret(kVapidAuthSecret);

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
  EXPECT_EQ(kAuthorizedEntity, fake_gcm_driver_.authorized_entity());
  EXPECT_EQ(kVapidP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kVapidAuthSecret, fake_gcm_driver_.auth_secret());

  EXPECT_EQ(kVapidFcmToken, fake_web_push_sender_->fcm_token());
  EXPECT_EQ(vapid_key.get(), fake_web_push_sender_->vapid_key());
  EXPECT_EQ(kTtlSeconds, fake_web_push_sender_->message()->time_to_live);
  EXPECT_EQ(WebPushMessage::Urgency::kHigh,
            fake_web_push_sender_->message()->urgency);
  components_sharing_message::SharingMessage message_sent;
  ASSERT_TRUE(fake_web_push_sender_->message());
  message_sent.ParseFromString(fake_web_push_sender_->message()->payload);
  EXPECT_TRUE(message_sent.has_ping_message());

  EXPECT_EQ(GetParam().expected_result, result);
  EXPECT_EQ(kMessageId, message_id);
  EXPECT_EQ(SharingChannelType::kFcmVapid, channel_type);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharingFCMSenderWebPushResultTest,
                         testing::ValuesIn(kWebPushResultTestData));

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
