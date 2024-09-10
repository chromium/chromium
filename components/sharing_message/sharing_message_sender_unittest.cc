// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_message_sender.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sharing_message/fake_device_info.h"
#include "components/sharing_message/ios_push/sharing_ios_push_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Test constants.
const char kReceiverGUID[] = "kReceiverGUID";
const char kReceiverDeviceName[] = "receiver_device";
const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kFCMToken[] = "vapid_fcm_token";
const char kAuthorizedEntity[] = "authorized_entity";
const char kSenderVapidFcmToken[] = "sender_vapid_fcm_token";
const char kSenderVapidP256dh[] = "sender_vapid_p256dh";
const char kSenderVapidAuthSecret[] = "sender_vapid_auth_secret";
const char kSenderSenderIdFcmToken[] = "sender_sender_id_fcm_token";
const char kSenderSenderIdP256dh[] = "sender_sender_id_p256dh";
const char kSenderSenderIdAuthSecret[] = "sender_sender_id_auth_secret";
const char kSenderMessageID[] = "sender_message_id";
constexpr base::TimeDelta kTimeToLive = base::Seconds(10);

namespace {

class MockSharingFCMSender : public SharingFCMSender {
 public:
  MockSharingFCMSender(
      SharingSyncPreference* sync_preference,
      syncer::DeviceInfoTracker* device_info_tracker,
      syncer::LocalDeviceInfoProvider* local_device_info_provider)
      : SharingFCMSender(
            /*web_push_sender=*/nullptr,
            /*sharing_message_bridge=*/nullptr,
            sync_preference,
            /*vapid_key_manager=*/nullptr,
            /*gcm_driver=*/nullptr,
            device_info_tracker,
            local_device_info_provider,
            /*sync_service=*/nullptr) {}
  MockSharingFCMSender(const MockSharingFCMSender&) = delete;
  MockSharingFCMSender& operator=(const MockSharingFCMSender&) = delete;
  ~MockSharingFCMSender() override = default;

  MOCK_METHOD4(SendMessageToFcmTarget,
               void(const components_sharing_message::FCMChannelConfiguration&
                        fcm_configuration,
                    base::TimeDelta time_to_live,
                    SharingMessage message,
                    SendMessageCallback callback));
};

class MockSharingIOSPushSender : public sharing_message::SharingIOSPushSender {
 public:
  MockSharingIOSPushSender(
      SharingSyncPreference* sync_preference,
      syncer::DeviceInfoTracker* device_info_tracker,
      syncer::LocalDeviceInfoProvider* local_device_info_provider,
      syncer::SyncService* sync_service)
      : SharingIOSPushSender(
            /*sharing_message_bridge=*/nullptr,
            device_info_tracker,
            local_device_info_provider,
            sync_service) {}
  MockSharingIOSPushSender(const MockSharingIOSPushSender&) = delete;
  MockSharingIOSPushSender& operator=(const MockSharingIOSPushSender&) = delete;
  ~MockSharingIOSPushSender() override = default;

  MOCK_METHOD3(DoSendUnencryptedMessageToDevice,
               void(const SharingTargetDeviceInfo& device,
                    sync_pb::UnencryptedSharingMessage message,
                    SendMessageCallback callback));
  MOCK_METHOD1(CanSendSendTabPushMessage,
               bool(const syncer::DeviceInfo& target_device_info));
};

class MockSendMessageDelegate
    : public SharingMessageSender::SendMessageDelegate {
 public:
  MockSendMessageDelegate() = default;
  MockSendMessageDelegate(const MockSendMessageDelegate&) = delete;
  MockSendMessageDelegate& operator=(const MockSendMessageDelegate&) = delete;
  ~MockSendMessageDelegate() override = default;

  MOCK_METHOD4(DoSendMessageToDevice,
               void(const SharingTargetDeviceInfo& device,
                    base::TimeDelta time_to_live,
                    components_sharing_message::SharingMessage message,
                    SendMessageCallback callback));
};

syncer::DeviceInfo::SharingInfo CreateLocalSharingInfo() {
  return syncer::DeviceInfo::SharingInfo(
      {kSenderVapidFcmToken, kSenderVapidP256dh, kSenderVapidAuthSecret},
      {kSenderSenderIdFcmToken, kSenderSenderIdP256dh,
       kSenderSenderIdAuthSecret},
      /*chime_representative_target_id=*/std::string(),
      std::set<sync_pb::SharingSpecificFields::EnabledFeatures>());
}

syncer::DeviceInfo::SharingInfo CreateSharingInfo() {
  return syncer::DeviceInfo::SharingInfo(
      {kFCMToken, kP256dh, kAuthSecret},
      {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
      "chime_representative_target_id",
      std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
          sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2});
}

SharingTargetDeviceInfo CreateFakeSharingTargetDeviceInfo(
    const std::string& guid,
    const std::string& client_name) {
  return SharingTargetDeviceInfo(guid, client_name,
                                 SharingDevicePlatform::kUnknown,
                                 /*pulse_interval=*/base::TimeDelta(),
                                 syncer::DeviceInfo::FormFactor::kUnknown,
                                 /*last_updated_timestamp=*/base::Time());
}

}  // namespace

class SharingMessageSenderTest : public testing::Test {
 public:
  SharingMessageSenderTest() {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
    auto mock_sharing_fcm_sender = std::make_unique<MockSharingFCMSender>(
        &sharing_sync_preference_,
        fake_device_info_sync_service_.GetDeviceInfoTracker(),
        fake_device_info_sync_service_.GetLocalDeviceInfoProvider());
    auto mock_sharing_ios_push_sender =
        std::make_unique<MockSharingIOSPushSender>(
            &sharing_sync_preference_,
            fake_device_info_sync_service_.GetDeviceInfoTracker(),
            fake_device_info_sync_service_.GetLocalDeviceInfoProvider(),
            &sync_service_);
    mock_sharing_fcm_sender_ = mock_sharing_fcm_sender.get();
    mock_sharing_ios_push_sender_ = mock_sharing_ios_push_sender.get();
    sharing_message_sender_.RegisterSendDelegate(
        SharingMessageSender::DelegateType::kFCM,
        std::move(mock_sharing_fcm_sender));
    sharing_message_sender_.RegisterSendDelegate(
        SharingMessageSender::DelegateType::kIOSPush,
        std::move(mock_sharing_ios_push_sender));
    sharing_sync_preference_.SetFCMRegistration(
        SharingSyncPreference::FCMRegistration(kAuthorizedEntity,
                                               base::Time::Now()));
    fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
        ->GetMutableDeviceInfo()
        ->set_sharing_info(CreateLocalSharingInfo());
  }

  SharingMessageSenderTest(const SharingMessageSenderTest&) = delete;
  SharingMessageSenderTest& operator=(const SharingMessageSenderTest&) = delete;

  ~SharingMessageSenderTest() override = default;

  SharingTargetDeviceInfo SetupReceiverDevice() {
    fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
        CreateFakeDeviceInfo(kReceiverGUID, kReceiverDeviceName,
                             CreateSharingInfo()));

    return CreateFakeSharingTargetDeviceInfo(kReceiverGUID,
                                             kReceiverDeviceName);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sharing_sync_preference_{
      &prefs_, &fake_device_info_sync_service_};
  syncer::MockSyncService sync_service_;

  SharingMessageSender sharing_message_sender_{
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider(),
      base::SingleThreadTaskRunner::GetCurrentDefault()};
  raw_ptr<MockSharingFCMSender> mock_sharing_fcm_sender_;
  raw_ptr<MockSharingIOSPushSender> mock_sharing_ios_push_sender_;
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

TEST_F(SharingMessageSenderTest, MessageSent_AckTimedout) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kAckTimeout),
                  testing::Eq(nullptr)));

  auto simulate_timeout =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingFCMSender::SendMessageCallback callback) {
        // FCM message sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID, SharingChannelType::kUnknown);
        task_environment_.FastForwardBy(kTimeToLive);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_.OnAckReceived(kSenderMessageID,
                                              /*response=*/nullptr);
      };

  EXPECT_CALL(
      *mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(simulate_timeout));

  sharing_message_sender_.SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      SharingMessageSender::DelegateType::kFCM, mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, SendMessageToDevice_InternalError) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kInternalError),
                  testing::Eq(nullptr)));

  auto simulate_internal_error =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingFCMSender::SendMessageCallback callback) {
        // FCM message not sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kInternalError,
                                std::nullopt, SharingChannelType::kUnknown);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_.OnAckReceived(kSenderMessageID,
                                              /*response=*/nullptr);
      };

  EXPECT_CALL(
      *mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(simulate_internal_error));

  sharing_message_sender_.SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      SharingMessageSender::DelegateType::kFCM, mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, SendUnencryptedMessageToDevice_InternalError) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kInternalError),
                  testing::Eq(nullptr)));

  auto simulate_internal_error =
      [&](const SharingTargetDeviceInfo& device,
          sync_pb::UnencryptedSharingMessage message,
          sharing_message::SharingIOSPushSender::SendMessageCallback callback) {
        // Message not sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kInternalError,
                                std::nullopt, SharingChannelType::kUnknown);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_.OnAckReceived(kSenderMessageID,
                                              /*response=*/nullptr);
      };

  EXPECT_CALL(
      *mock_sharing_ios_push_sender_,
      DoSendUnencryptedMessageToDevice(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(simulate_internal_error));

  sharing_message_sender_.SendUnencryptedMessageToDevice(
      device_info, sync_pb::UnencryptedSharingMessage(),
      SharingMessageSender::DelegateType::kIOSPush, mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, MessageSent_AckReceived) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_click_to_call_message()->set_phone_number("999999");

  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kSuccessful),
                  ProtoEquals(expected_response_message)));

  auto simulate_expected_ack_message_received =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingFCMSender::SendMessageCallback callback) {
        // FCM message sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID, SharingChannelType::kUnknown);

        // Check sender info details.
        const syncer::DeviceInfo* local_device =
            fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
                ->GetLocalDeviceInfo();
        ASSERT_EQ(local_device->guid(), message.sender_guid());
        ASSERT_EQ(
            send_tab_to_self::GetSharingDeviceNames(local_device).full_name,
            message.sender_device_name());
        ASSERT_TRUE(local_device->sharing_info().has_value());
        auto& fcm_ack_configuration = message.fcm_channel_configuration();
        ASSERT_EQ(kSenderVapidFcmToken,
                  fcm_ack_configuration.vapid_fcm_token());
        ASSERT_EQ(kSenderVapidP256dh, fcm_ack_configuration.vapid_p256dh());
        ASSERT_EQ(kSenderVapidAuthSecret,
                  fcm_ack_configuration.vapid_auth_secret());
        ASSERT_EQ(kSenderSenderIdFcmToken,
                  fcm_ack_configuration.sender_id_fcm_token());
        ASSERT_EQ(kSenderSenderIdP256dh,
                  fcm_ack_configuration.sender_id_p256dh());
        ASSERT_EQ(kSenderSenderIdAuthSecret,
                  fcm_ack_configuration.sender_id_auth_secret());

        // Simulate ack message received.
        std::unique_ptr<components_sharing_message::ResponseMessage>
            response_message =
                std::make_unique<components_sharing_message::ResponseMessage>();
        response_message->CopyFrom(expected_response_message);

        sharing_message_sender_.OnAckReceived(kSenderMessageID,
                                              std::move(response_message));
      };

  EXPECT_CALL(
      *mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(simulate_expected_ack_message_received));

  sharing_message_sender_.SendMessageToDevice(
      device_info, kTimeToLive, std::move(sent_message),
      SharingMessageSender::DelegateType::kFCM, mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, MessageSent_AckReceivedBeforeMessageId) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_click_to_call_message()->set_phone_number("999999");

  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kSuccessful),
                  ProtoEquals(expected_response_message)));

  auto simulate_expected_ack_message_received =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingFCMSender::SendMessageCallback callback) {
        // Simulate ack message received.
        std::unique_ptr<components_sharing_message::ResponseMessage>
            response_message =
                std::make_unique<components_sharing_message::ResponseMessage>();
        response_message->CopyFrom(expected_response_message);

        sharing_message_sender_.OnAckReceived(kSenderMessageID,
                                              std::move(response_message));

        // Call FCM send success after receiving the ACK.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID,
                                SharingChannelType::kFcmVapid);
      };

  EXPECT_CALL(
      *mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(simulate_expected_ack_message_received));

  sharing_message_sender_.SendMessageToDevice(
      device_info, kTimeToLive, std::move(sent_message),
      SharingMessageSender::DelegateType::kFCM, mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, NonExistingDelegate) {
  SharingMessageSender sharing_message_sender{
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider(),
      base::SingleThreadTaskRunner::GetCurrentDefault()};

  SharingTargetDeviceInfo device_info =
      CreateFakeSharingTargetDeviceInfo(kReceiverGUID, kReceiverDeviceName);

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kInternalError),
                  testing::Eq(nullptr)));

  sharing_message_sender.SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      SharingMessageSender::DelegateType::kFCM, mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, RequestCancelled) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_sms_fetch_request()->add_origins("https://a.com");

  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kCancelled),
                  testing::Eq(nullptr)));

  EXPECT_CALL(
      *mock_sharing_fcm_sender_,
      SendMessageToFcmTarget(testing::_, testing::_, testing::_, testing::_));

  base::OnceClosure cancel_callback =
      sharing_message_sender_.SendMessageToDevice(
          device_info, kTimeToLive, std::move(sent_message),
          SharingMessageSender::DelegateType::kFCM, mock_callback.Get());

  ASSERT_FALSE(cancel_callback.is_null());
  std::move(cancel_callback).Run();
}
