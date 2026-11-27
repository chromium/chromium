// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_message_sender.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_channel_sender.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_name_util.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Test constants.
const char kReceiverGUID[] = "kReceiverGUID";
const char kReceiverDeviceName[] = "receiver_device";
const char kSenderSenderIdFcmToken[] = "sender_sender_id_fcm_token";
const char kSenderSenderIdP256dh[] = "sender_sender_id_p256dh";
const char kSenderSenderIdAuthSecret[] = "sender_sender_id_auth_secret";
const char kSenderMessageID[] = "sender_message_id";
constexpr base::TimeDelta kTimeToLive = base::Seconds(10);

namespace {

using testing::IsNull;
using testing::NiceMock;

class MockSharingChannelSender : public SharingChannelSender {
 public:
  MockSharingChannelSender(
      syncer::DeviceInfoTracker* device_info_tracker,
      syncer::LocalDeviceInfoProvider* local_device_info_provider,
      syncer::SyncService* sync_service)
      : SharingChannelSender(
            /*sharing_message_bridge=*/nullptr,
            /*gcm_driver=*/nullptr,
            device_info_tracker,
            local_device_info_provider,
            sync_service,
            /*start_sync_flare=*/base::DoNothing()) {}
  MockSharingChannelSender(const MockSharingChannelSender&) = delete;
  MockSharingChannelSender& operator=(const MockSharingChannelSender&) = delete;
  ~MockSharingChannelSender() override = default;

  MOCK_METHOD4(SendMessageToFcmTarget,
               void(const components_sharing_message::FCMChannelConfiguration&
                        fcm_configuration,
                    base::TimeDelta time_to_live,
                    SharingMessage message,
                    SendMessageCallback callback));
  MOCK_METHOD3(SendIosPushMessageToDevice,
               void(const SharingTargetDeviceInfo& device,
                    sync_pb::UnencryptedSharingMessage message,
                    SendMessageCallback callback));
  MOCK_METHOD3(
      SendMessageToServerTarget,
      void(const components_sharing_message::ServerChannelConfiguration&
               server_channel,
           SharingMessage message,
           SendMessageCallback callback));
  MOCK_METHOD0(FailPendingMessages, void());
};

// Creates a valid FCM channel configuration for test ack messages.
components_sharing_message::FCMChannelConfiguration CreateTestFcmChannel() {
  components_sharing_message::FCMChannelConfiguration fcm_channel;
  fcm_channel.set_sender_id_fcm_token("ack_fcm_token");
  fcm_channel.set_sender_id_p256dh("ack_p256dh");
  fcm_channel.set_sender_id_auth_secret("ack_auth_secret");
  return fcm_channel;
}

// Creates a valid server channel configuration for test messages.
components_sharing_message::ServerChannelConfiguration
CreateTestServerChannel() {
  components_sharing_message::ServerChannelConfiguration server_channel;
  server_channel.set_configuration("test_configuration");
  server_channel.set_p256dh("test_p256dh");
  server_channel.set_auth_secret("test_auth_secret");
  return server_channel;
}

syncer::DeviceInfo::SharingInfo CreateLocalSharingInfo() {
  return syncer::DeviceInfo::SharingInfo(
      {kSenderSenderIdFcmToken, kSenderSenderIdP256dh,
       kSenderSenderIdAuthSecret},
      /*chime_representative_target_id=*/std::string(),
      std::set<syncer::DeviceInfo::SharingFeature>());
}

syncer::DeviceInfo::SharingInfo CreateSharingInfo() {
  return syncer::DeviceInfo::SharingInfo(
      {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
      "chime_representative_target_id",
      std::set<syncer::DeviceInfo::SharingFeature>{
          syncer::DeviceInfo::SharingFeature::kRemoteCopy});
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
    auto mock_sharing_channel_sender =
        std::make_unique<NiceMock<MockSharingChannelSender>>(
            fake_device_info_sync_service_.GetDeviceInfoTracker(),
            fake_device_info_sync_service_.GetLocalDeviceInfoProvider(),
            &test_sync_service_);
    mock_sharing_channel_sender_ = mock_sharing_channel_sender.get();
    sharing_message_sender_ = std::make_unique<SharingMessageSender>(
        std::move(mock_sharing_channel_sender),
        fake_device_info_sync_service_.GetLocalDeviceInfoProvider(),
        &test_sync_service_, mock_sync_flare_.Get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
        ->GetMutableDeviceInfo()
        ->set_sharing_info(CreateLocalSharingInfo());
  }

  SharingMessageSenderTest(const SharingMessageSenderTest&) = delete;
  SharingMessageSenderTest& operator=(const SharingMessageSenderTest&) = delete;

  ~SharingMessageSenderTest() override = default;

  SharingTargetDeviceInfo SetupReceiverDevice() {
    fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
        syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kLinux)
            .WithGuid(kReceiverGUID)
            .WithClientName(kReceiverDeviceName)
            .WithSharingInfo(CreateSharingInfo())
            .Build());

    return CreateFakeSharingTargetDeviceInfo(kReceiverGUID,
                                             kReceiverDeviceName);
  }

  // Verifies that outgoing device, server, and ack messages fail upfront with
  // `kInternalError` without triggering the Sync flare or channel sender.
  void ExpectImmediateSendFails() {
    SharingTargetDeviceInfo device_info = SetupReceiverDevice();
    EXPECT_CALL(mock_sync_flare_, Run).Times(0);
    EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);
    EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
        .Times(0);

    base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
    EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kInternalError,
                                     /*response=*/IsNull()));
    sharing_message_sender_->SendMessageToDevice(
        device_info, kTimeToLive, components_sharing_message::SharingMessage(),
        device_callback.Get());

    base::MockCallback<SharingMessageSender::ResponseCallback> server_callback;
    EXPECT_CALL(server_callback, Run(SharingSendMessageResult::kInternalError,
                                     /*response=*/IsNull()));
    sharing_message_sender_->SendMessageToServerTarget(
        CreateTestServerChannel(), kTimeToLive,
        components_sharing_message::SharingMessage(), server_callback.Get());

    base::MockCallback<SharingMessageSender::SendAckCallback> fcm_ack_callback;
    EXPECT_CALL(fcm_ack_callback,
                Run(SharingSendMessageResult::kInternalError));
    sharing_message_sender_->SendAckMessageToFcmTarget(
        CreateTestFcmChannel(), "original_message_id",
        /*response=*/nullptr, fcm_ack_callback.Get());

    base::MockCallback<SharingMessageSender::SendAckCallback>
        server_ack_callback;
    EXPECT_CALL(server_ack_callback,
                Run(SharingSendMessageResult::kInternalError));
    sharing_message_sender_->SendAckMessageToServerTarget(
        CreateTestServerChannel(), "original_message_id",
        /*response=*/nullptr, server_ack_callback.Get());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  syncer::TestSyncService test_sync_service_;
  // `NiceMock` suppresses uninteresting-call warnings in tests that queue
  // messages without asserting on the Sync flare.
  NiceMock<base::MockCallback<syncer::SyncableService::StartSyncFlare>>
      mock_sync_flare_;

  std::unique_ptr<SharingMessageSender> sharing_message_sender_;
  raw_ptr<MockSharingChannelSender> mock_sharing_channel_sender_;
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
              Run(SharingSendMessageResult::kAckTimeout, IsNull()));

  auto simulate_timeout =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingChannelSender::SendMessageCallback callback) {
        // FCM message sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID, SharingChannelType::kUnknown);
        task_environment_.FastForwardBy(kTimeToLive);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_->OnAckReceived(kSenderMessageID,
                                               /*response=*/nullptr);
      };

  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(simulate_timeout);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, SendMessageToDevice_InternalError) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(SharingSendMessageResult::kInternalError, IsNull()));

  auto simulate_internal_error =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingChannelSender::SendMessageCallback callback) {
        // FCM message not sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kInternalError,
                                std::nullopt, SharingChannelType::kUnknown);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_->OnAckReceived(kSenderMessageID,
                                               /*response=*/nullptr);
      };

  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(simulate_internal_error);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, SendIosPushMessageToDevice_InternalError) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(SharingSendMessageResult::kInternalError, IsNull()));

  auto simulate_internal_error =
      [&](const SharingTargetDeviceInfo& device,
          sync_pb::UnencryptedSharingMessage message,
          SharingChannelSender::SendMessageCallback callback) {
        // Message not sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kInternalError,
                                std::nullopt, SharingChannelType::kUnknown);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_->OnAckReceived(kSenderMessageID,
                                               /*response=*/nullptr);
      };

  EXPECT_CALL(*mock_sharing_channel_sender_, SendIosPushMessageToDevice)
      .WillOnce(simulate_internal_error);

  sharing_message_sender_->SendIosPushMessageToDevice(
      device_info, sync_pb::UnencryptedSharingMessage(), mock_callback.Get());
}

// Tests that `SendMessageToDevice()` populates sender info with the local
// device's GUID and display name and invokes the callback when an ACK is
// received.
TEST_F(SharingMessageSenderTest, MessageSent_AckReceived) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_remote_copy_message()->set_text("999999");

  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(SharingSendMessageResult::kSuccessful,
                                 ProtoEquals(expected_response_message)));

  auto simulate_expected_ack_message_received =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingChannelSender::SendMessageCallback callback) {
        // FCM message sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID, SharingChannelType::kUnknown);

        // Check sender info details.
        const syncer::DeviceInfo* local_device =
            fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
                ->GetLocalDeviceInfo();
        ASSERT_EQ(local_device->guid(), message.sender_guid());
        ASSERT_EQ(syncer::GetDeviceDisplayName(local_device),
                  message.sender_device_name());

        ASSERT_TRUE(local_device->sharing_info().has_value());
        auto& fcm_ack_configuration = message.fcm_channel_configuration();
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

        sharing_message_sender_->OnAckReceived(kSenderMessageID,
                                               std::move(response_message));
      };

  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(simulate_expected_ack_message_received);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, std::move(sent_message), mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, MessageSent_AckReceivedBeforeMessageId) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_remote_copy_message()->set_text("999999");

  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(SharingSendMessageResult::kSuccessful,
                                 ProtoEquals(expected_response_message)));

  auto simulate_expected_ack_message_received =
      [&](const components_sharing_message::FCMChannelConfiguration&
              fcm_configuration,
          base::TimeDelta time_to_live,
          components_sharing_message::SharingMessage message,
          SharingChannelSender::SendMessageCallback callback) {
        // Simulate ack message received.
        std::unique_ptr<components_sharing_message::ResponseMessage>
            response_message =
                std::make_unique<components_sharing_message::ResponseMessage>();
        response_message->CopyFrom(expected_response_message);

        sharing_message_sender_->OnAckReceived(kSenderMessageID,
                                               std::move(response_message));

        // Call FCM send success after receiving the ACK.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID,
                                SharingChannelType::kFcmSenderId);
      };

  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(simulate_expected_ack_message_received);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, std::move(sent_message), mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, RequestCancelled) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_sms_fetch_request()->add_origins("https://a.com");

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(SharingSendMessageResult::kCancelled, IsNull()));

  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget);

  base::OnceClosure cancel_callback =
      sharing_message_sender_->SendMessageToDevice(device_info, kTimeToLive,
                                                   std::move(sent_message),
                                                   mock_callback.Get());

  ASSERT_FALSE(cancel_callback.is_null());
  std::move(cancel_callback).Run();
}

TEST_F(SharingMessageSenderTest, SendMessageToServerTarget_Success) {
  components_sharing_message::ServerChannelConfiguration server_channel;
  server_channel.set_configuration("test_configuration");
  server_channel.set_p256dh("test_p256dh");
  server_channel.set_auth_secret("test_auth_secret");

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_remote_copy_message()->set_text("999999");

  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(SharingSendMessageResult::kSuccessful,
                                 ProtoEquals(expected_response_message)));

  auto simulate_expected_ack_message_received =
      [&](const components_sharing_message::ServerChannelConfiguration&
              server_channel_config,
          components_sharing_message::SharingMessage message,
          SharingChannelSender::SendMessageCallback callback) {
        // FCM message sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID, SharingChannelType::kServer);

        // Simulate ack message received.
        std::unique_ptr<components_sharing_message::ResponseMessage>
            response_message =
                std::make_unique<components_sharing_message::ResponseMessage>();
        response_message->CopyFrom(expected_response_message);

        sharing_message_sender_->OnAckReceived(kSenderMessageID,
                                               std::move(response_message));
      };

  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .WillOnce(simulate_expected_ack_message_received);

  sharing_message_sender_->SendMessageToServerTarget(
      server_channel, kTimeToLive, std::move(sent_message),
      mock_callback.Get());
}

TEST_F(SharingMessageSenderTest,
       SendMessageToServerTarget_PostponedUntilLocalDeviceInfoReady) {
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);

  components_sharing_message::ServerChannelConfiguration server_channel;
  server_channel.set_configuration("test_configuration");
  server_channel.set_p256dh("test_p256dh");
  server_channel.set_auth_secret("test_auth_secret");

  components_sharing_message::SharingMessage sent_message;
  sent_message.mutable_remote_copy_message()->set_text("999999");

  // Sending before `LocalDeviceInfoProvider` is ready should postpone the
  // message without invoking the channel sender or response callback.
  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .Times(0);

  sharing_message_sender_->SendMessageToServerTarget(
      server_channel, kTimeToLive, std::move(sent_message),
      mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  testing::Mock::VerifyAndClearExpectations(mock_sharing_channel_sender_);

  // Once `LocalDeviceInfoProvider` becomes ready, the postponed message should
  // be sent and the response callback invoked upon receiving the ACK.
  components_sharing_message::SharingMessage actual_sent_message;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .WillOnce(testing::DoAll(
          MoveArg<1>(&actual_sent_message),
          base::test::RunOnceCallback<2>(SharingSendMessageResult::kSuccessful,
                                         kSenderMessageID,
                                         SharingChannelType::kServer)));
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(true);

  EXPECT_CALL(mock_callback, Run(SharingSendMessageResult::kSuccessful,
                                 ProtoEquals(expected_response_message)));
  sharing_message_sender_->OnAckReceived(
      kSenderMessageID,
      std::make_unique<components_sharing_message::ResponseMessage>(
          expected_response_message));

  // Verify that the sent message was populated with the local device's sender
  // metadata.
  const syncer::DeviceInfo* local_device =
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();
  ASSERT_TRUE(local_device);
  EXPECT_EQ(local_device->guid(), actual_sent_message.sender_guid());
  EXPECT_EQ(syncer::GetDeviceDisplayName(local_device),
            actual_sent_message.sender_device_name());
  EXPECT_EQ("999999", actual_sent_message.remote_copy_message().text());
}

TEST_F(SharingMessageSenderTest,
       SendMessageToDevice_PostponedUntilLocalDeviceInfoReady) {
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  // Sending before `LocalDeviceInfoProvider` is ready should postpone the
  // message without invoking the channel sender or response callback.
  components_sharing_message::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  testing::Mock::VerifyAndClearExpectations(mock_sharing_channel_sender_);

  // Once `LocalDeviceInfoProvider` becomes ready, the postponed message should
  // be sent and the response callback invoked upon receiving the ACK.
  components_sharing_message::SharingMessage actual_sent_message;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(testing::DoAll(
          MoveArg<2>(&actual_sent_message),
          base::test::RunOnceCallback<3>(SharingSendMessageResult::kSuccessful,
                                         kSenderMessageID,
                                         SharingChannelType::kFcmSenderId)));
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(true);

  EXPECT_CALL(mock_callback, Run(SharingSendMessageResult::kSuccessful,
                                 ProtoEquals(expected_response_message)));
  sharing_message_sender_->OnAckReceived(
      kSenderMessageID,
      std::make_unique<components_sharing_message::ResponseMessage>(
          expected_response_message));

  // Verify that the sent message was populated with the local device's sender
  // metadata.
  const syncer::DeviceInfo* local_device =
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();
  ASSERT_TRUE(local_device);
  EXPECT_EQ(local_device->guid(), actual_sent_message.sender_guid());
  EXPECT_EQ(syncer::GetDeviceDisplayName(local_device),
            actual_sent_message.sender_device_name());
}

TEST_F(SharingMessageSenderTest,
       SendMessageToDevice_LocalDeviceInfoNotReady_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kSharingPostponeSendingUntilLocalDeviceInfoReady);

  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(SharingSendMessageResult::kInternalError, IsNull()));
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      mock_callback.Get());
}

// Tests that `FailPendingMessages()` fails outgoing messages waiting for
// `LocalDeviceInfo` and delegates to `SharingChannelSender`.
TEST_F(SharingMessageSenderTest,
       FailPendingMessages_FailsMessagesWaitingForLocalDeviceInfo) {
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              Run(SharingSendMessageResult::kInternalError, IsNull()));
  EXPECT_CALL(*mock_sharing_channel_sender_, FailPendingMessages());
  sharing_message_sender_->FailPendingMessages();
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Run).Times(0);
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(true);
}

// Tests that `SendAckMessageToFcmTarget()` builds an `AckMessage` and sends it
// immediately with `kSharingAckMessageTTL` without waiting for
// `LocalDeviceInfo` or logging `Sharing.SendMessageResult`.
TEST_F(SharingMessageSenderTest, SendAckMessageToFcmTarget) {
  base::HistogramTester histogram_tester;
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);

  components_sharing_message::SharingMessage actual_sent_message;
  EXPECT_CALL(*mock_sharing_channel_sender_,
              SendMessageToFcmTarget(testing::_, kSharingAckMessageTTL,
                                     testing::_, testing::_))
      .WillOnce(testing::DoAll(
          MoveArg<2>(&actual_sent_message),
          base::test::RunOnceCallback<3>(SharingSendMessageResult::kSuccessful,
                                         kSenderMessageID,
                                         SharingChannelType::kFcmSenderId)));

  base::test::TestFuture<SharingSendMessageResult> result_future;
  sharing_message_sender_->SendAckMessageToFcmTarget(
      CreateTestFcmChannel(), "original_message_id",
      std::make_unique<components_sharing_message::ResponseMessage>(),
      result_future.GetCallback());

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, result_future.Get());
  ASSERT_TRUE(actual_sent_message.has_ack_message());
  EXPECT_EQ("original_message_id",
            actual_sent_message.ack_message().original_message_id());
  EXPECT_TRUE(actual_sent_message.ack_message().has_response_message());
  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 0);
}

// Tests that `SendAckMessageToServerTarget()` builds an `AckMessage` and sends
// it immediately without waiting for `LocalDeviceInfo` or logging
// `Sharing.SendMessageResult`.
TEST_F(SharingMessageSenderTest, SendAckMessageToServerTarget) {
  base::HistogramTester histogram_tester;
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);

  components_sharing_message::SharingMessage actual_sent_message;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .WillOnce(testing::DoAll(
          MoveArg<1>(&actual_sent_message),
          base::test::RunOnceCallback<2>(SharingSendMessageResult::kSuccessful,
                                         kSenderMessageID,
                                         SharingChannelType::kServer)));

  base::test::TestFuture<SharingSendMessageResult> result_future;
  sharing_message_sender_->SendAckMessageToServerTarget(
      CreateTestServerChannel(), "original_message_id", /*response=*/nullptr,
      result_future.GetCallback());

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, result_future.Get());
  ASSERT_TRUE(actual_sent_message.has_ack_message());
  EXPECT_EQ("original_message_id",
            actual_sent_message.ack_message().original_message_id());
  EXPECT_FALSE(actual_sent_message.ack_message().has_response_message());
  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 0);
}

// Tests that outgoing messages and acks are queued during Sync startup
// (`START_DEFERRED` and `CONFIGURING`), trigger the Sync flare once, and replay
// once `ACTIVE`.
TEST_F(SharingMessageSenderTest,
       PostponesMessagesUntilSharingMessageSyncActive) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  base::MockCallback<SharingMessageSender::SendAckCallback> ack_callback;
  EXPECT_CALL(device_callback, Run).Times(0);
  EXPECT_CALL(ack_callback, Run).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .Times(0);

  // Sync flare should be called only once across multiple queued messages.
  EXPECT_CALL(mock_sync_flare_, Run(syncer::SHARING_MESSAGE));
  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());
  sharing_message_sender_->SendAckMessageToServerTarget(
      CreateTestServerChannel(), "original_message_id", /*response=*/nullptr,
      ack_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&device_callback);
  testing::Mock::VerifyAndClearExpectations(&ack_callback);
  testing::Mock::VerifyAndClearExpectations(mock_sharing_channel_sender_);

  // Messages remain queued while Sync is still configuring.
  EXPECT_CALL(device_callback, Run).Times(0);
  EXPECT_CALL(ack_callback, Run).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .Times(0);
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.FireStateChanged();
  testing::Mock::VerifyAndClearExpectations(&device_callback);
  testing::Mock::VerifyAndClearExpectations(&ack_callback);
  testing::Mock::VerifyAndClearExpectations(mock_sharing_channel_sender_);

  // Once Sync becomes active, both queued messages are sent.
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(base::test::RunOnceCallback<3>(
          SharingSendMessageResult::kSuccessful, kSenderMessageID,
          SharingChannelType::kFcmSenderId));
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .WillOnce(base::test::RunOnceCallback<2>(
          SharingSendMessageResult::kSuccessful, "ack_id",
          SharingChannelType::kServer));
  EXPECT_CALL(ack_callback, Run(SharingSendMessageResult::kSuccessful));

  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();

  components_sharing_message::ResponseMessage expected_response;
  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kSuccessful,
                                   ProtoEquals(expected_response)));
  sharing_message_sender_->OnAckReceived(
      kSenderMessageID,
      std::make_unique<components_sharing_message::ResponseMessage>(
          expected_response));
}

// Tests that a device message cancelled while queued waiting for
// `SHARING_MESSAGE` sync is not sent once Sync becomes active.
TEST_F(SharingMessageSenderTest,
       SendMessageToDevice_CancelledWhileWaitingForSync) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);

  base::OnceClosure cancel_device =
      sharing_message_sender_->SendMessageToDevice(
          device_info, kTimeToLive,
          components_sharing_message::SharingMessage(), device_callback.Get());

  EXPECT_CALL(device_callback,
              Run(SharingSendMessageResult::kCancelled, /*response=*/IsNull()));
  std::move(cancel_device).Run();
  testing::Mock::VerifyAndClearExpectations(&device_callback);

  // Activating Sync afterwards should skip the already-cancelled message.
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that a server message cancelled while queued waiting for
// `SHARING_MESSAGE` sync is not sent once Sync becomes active.
TEST_F(SharingMessageSenderTest,
       SendMessageToServerTarget_CancelledWhileWaitingForSync) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  base::MockCallback<SharingMessageSender::ResponseCallback> server_callback;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .Times(0);

  base::OnceClosure cancel_server =
      sharing_message_sender_->SendMessageToServerTarget(
          CreateTestServerChannel(), kTimeToLive,
          components_sharing_message::SharingMessage(), server_callback.Get());

  EXPECT_CALL(server_callback,
              Run(SharingSendMessageResult::kCancelled, /*response=*/IsNull()));
  std::move(cancel_server).Run();
  testing::Mock::VerifyAndClearExpectations(&server_callback);

  // Activating Sync afterwards should skip the already-cancelled message.
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that a device message that times out while queued waiting for
// `SHARING_MESSAGE` sync is not sent once Sync becomes active.
TEST_F(SharingMessageSenderTest,
       SendMessageToDevice_TimedOutWhileWaitingForSync) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());

  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kAckTimeout,
                                   /*response=*/IsNull()));
  task_environment_.FastForwardBy(kTimeToLive);
  testing::Mock::VerifyAndClearExpectations(&device_callback);

  // Activating Sync afterwards should skip the already-timed-out message.
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that a server message that times out while queued waiting for
// `SHARING_MESSAGE` sync is not sent once Sync becomes active.
TEST_F(SharingMessageSenderTest,
       SendMessageToServerTarget_TimedOutWhileWaitingForSync) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  base::MockCallback<SharingMessageSender::ResponseCallback> server_callback;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .Times(0);

  sharing_message_sender_->SendMessageToServerTarget(
      CreateTestServerChannel(), kTimeToLive,
      components_sharing_message::SharingMessage(), server_callback.Get());

  EXPECT_CALL(server_callback, Run(SharingSendMessageResult::kAckTimeout,
                                   /*response=*/IsNull()));
  task_environment_.FastForwardBy(kTimeToLive);
  testing::Mock::VerifyAndClearExpectations(&server_callback);

  // Activating Sync afterwards should skip the already-timed-out message.
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that when both `LocalDeviceInfo` and Sync are uninitialized, queued
// acks flush as soon as `SHARING_MESSAGE` becomes active while regular messages
// wait until `LocalDeviceInfo` is also initialized.
TEST_F(SharingMessageSenderTest,
       PostponesMessagesWhenBothLocalDeviceInfoAndSyncNotReady) {
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  base::MockCallback<SharingMessageSender::SendAckCallback> ack_callback;
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .Times(0);

  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());
  sharing_message_sender_->SendAckMessageToServerTarget(
      CreateTestServerChannel(), "original_message_id", /*response=*/nullptr,
      ack_callback.Get());
  testing::Mock::VerifyAndClearExpectations(mock_sharing_channel_sender_);

  // Sync becomes active first while `LocalDeviceInfo` is still uninitialized:
  // only the ack should be sent.
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToServerTarget)
      .WillOnce(base::test::RunOnceCallback<2>(
          SharingSendMessageResult::kSuccessful, "ack_id",
          SharingChannelType::kServer));
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);
  EXPECT_CALL(ack_callback, Run(SharingSendMessageResult::kSuccessful));

  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
  testing::Mock::VerifyAndClearExpectations(mock_sharing_channel_sender_);

  // Once `LocalDeviceInfo` finishes initializing, the regular message is sent.
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget)
      .WillOnce(base::test::RunOnceCallback<3>(
          SharingSendMessageResult::kSuccessful, kSenderMessageID,
          SharingChannelType::kFcmSenderId));
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(true);

  components_sharing_message::ResponseMessage expected_response;
  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kSuccessful,
                                   ProtoEquals(expected_response)));
  sharing_message_sender_->OnAckReceived(
      kSenderMessageID,
      std::make_unique<components_sharing_message::ResponseMessage>(
          expected_response));
}

// Tests that `FailPendingMessages()` fails queued device and ack messages
// waiting on Sync and prevents them from sending once Sync becomes active.
TEST_F(SharingMessageSenderTest,
       FailPendingMessages_FailsQueuedDeviceAndAckMessages) {
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  base::MockCallback<SharingMessageSender::SendAckCallback> ack_callback;
  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());
  sharing_message_sender_->SendAckMessageToFcmTarget(
      CreateTestFcmChannel(), "original_message_id", /*response=*/nullptr,
      ack_callback.Get());

  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kInternalError,
                                   /*response=*/IsNull()));
  EXPECT_CALL(ack_callback, Run(SharingSendMessageResult::kInternalError));
  sharing_message_sender_->FailPendingMessages();
  testing::Mock::VerifyAndClearExpectations(&device_callback);
  testing::Mock::VerifyAndClearExpectations(&ack_callback);

  // Activating Sync afterwards should not send anything.
  EXPECT_CALL(*mock_sharing_channel_sender_, SendMessageToFcmTarget).Times(0);
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that queued messages (including those waiting on `LocalDeviceInfo`)
// fail immediately when Sync transitions to `DISABLED`.
TEST_F(SharingMessageSenderTest, FailsPendingMessagesWhenSyncDisabled) {
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()->SetReady(false);
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  base::MockCallback<SharingMessageSender::SendAckCallback> ack_callback;
  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());
  sharing_message_sender_->SendAckMessageToFcmTarget(
      CreateTestFcmChannel(), "original_message_id", /*response=*/nullptr,
      ack_callback.Get());

  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kInternalError,
                                   /*response=*/IsNull()));
  EXPECT_CALL(ack_callback, Run(SharingSendMessageResult::kInternalError));
  test_sync_service_.SetSignedOut();
  test_sync_service_.FireStateChanged();
}

// Tests that queued messages fail when Sync transitions to `PAUSED`.
TEST_F(SharingMessageSenderTest, FailsPendingMessagesWhenSyncPaused) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());

  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kInternalError,
                                   /*response=*/IsNull()));
  test_sync_service_.SetPersistentAuthError();
  test_sync_service_.FireStateChanged();
}

// Tests that queued messages fail when Sync reaches `ACTIVE` with
// `SHARING_MESSAGE` failed.
TEST_F(SharingMessageSenderTest,
       FailsPendingMessagesWhenSharingMessageDataTypeFails) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());

  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kInternalError,
                                   /*response=*/IsNull()));
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.FireStateChanged();
}

// Tests that sending fails upfront when Sync is `DISABLED`.
TEST_F(SharingMessageSenderTest, FailsImmediatelyWhenSyncDisabled) {
  test_sync_service_.SetSignedOut();
  ExpectImmediateSendFails();
}

// Tests that sending fails upfront when Sync is `PAUSED`.
TEST_F(SharingMessageSenderTest, FailsImmediatelyWhenSyncPaused) {
  test_sync_service_.SetPersistentAuthError();
  ExpectImmediateSendFails();
}

// Tests that sending fails upfront when Sync is `ACTIVE` with
// `SHARING_MESSAGE` failed.
TEST_F(SharingMessageSenderTest,
       FailsImmediatelyWhenSharingMessageDataTypeFailed) {
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});
  ExpectImmediateSendFails();
}

// Tests that `OnSyncShutdown()` fails queued messages and causes subsequent
// send attempts to fail immediately without dereferencing `sync_service_`.
TEST_F(SharingMessageSenderTest,
       OnSyncShutdown_FailsPendingMessagesAndSubsequentSends) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  SharingTargetDeviceInfo device_info = SetupReceiverDevice();

  base::MockCallback<SharingMessageSender::ResponseCallback> device_callback;
  base::MockCallback<SharingMessageSender::SendAckCallback> ack_callback;
  sharing_message_sender_->SendMessageToDevice(
      device_info, kTimeToLive, components_sharing_message::SharingMessage(),
      device_callback.Get());
  sharing_message_sender_->SendAckMessageToFcmTarget(
      CreateTestFcmChannel(), "original_message_id", /*response=*/nullptr,
      ack_callback.Get());

  EXPECT_CALL(device_callback, Run(SharingSendMessageResult::kInternalError,
                                   /*response=*/IsNull()));
  EXPECT_CALL(ack_callback, Run(SharingSendMessageResult::kInternalError));
  test_sync_service_.Shutdown();
  testing::Mock::VerifyAndClearExpectations(&device_callback);
  testing::Mock::VerifyAndClearExpectations(&ack_callback);

  ExpectImmediateSendFails();
}
