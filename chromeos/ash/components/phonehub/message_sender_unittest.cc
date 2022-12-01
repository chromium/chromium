// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/message_sender_impl.h"

#include <netinet/in.h>
#include <stdint.h>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

class MessageSenderImplTest : public testing::Test {
 protected:
  MessageSenderImplTest() = default;
  MessageSenderImplTest(const MessageSenderImplTest&) = delete;
  MessageSenderImplTest& operator=(const MessageSenderImplTest&) = delete;
  ~MessageSenderImplTest() override = default;

  void SetUp() override {
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    message_sender_ =
        std::make_unique<MessageSenderImpl>(fake_connection_manager_.get());
  }

  void VerifyMessage(proto::MessageType expected_message_type,
                     const google::protobuf::MessageLite* expected_request,
                     const std::string& actual_message) {
    // Message types are the first two bytes of |actual_message|. Retrieving
    // a uint16_t* from |actual_message.data()| lets us get the first two
    // bytes of |actual_message|.
    uint16_t* actual_message_uint16t_ptr =
        reinterpret_cast<uint16_t*>(const_cast<char*>(actual_message.data()));
    EXPECT_EQ(ntohs(static_cast<uint16_t>(expected_message_type)),
              *actual_message_uint16t_ptr);

    const std::string& expected_proto_message =
        expected_request->SerializeAsString();
    // The serialized proto message is after the first two bytes of
    // |actual_message|.
    const std::string actual_proto_message = actual_message.substr(2);

    // Expected size is the size of the serialized expected proto string +
    // 2 bytes for the proto::MessageType.
    EXPECT_EQ(expected_proto_message.size() + 2, actual_message.size());
    EXPECT_EQ(expected_proto_message, actual_proto_message);
  }

  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<MessageSenderImpl> message_sender_;
};

TEST_F(MessageSenderImplTest, SendCrosState) {
  proto::CrosState request;
  request.set_notification_setting(
      proto::NotificationSetting::NOTIFICATIONS_ON);
  request.set_camera_roll_setting(proto::CameraRollSetting::CAMERA_ROLL_OFF);
  if (features::IsPhoneHubMonochromeNotificationIconsEnabled()) {
    request.set_notification_icon_styling(
        proto::NotificationIconStyling::ICON_STYLE_MONOCHROME_SMALL_ICON);
  }

  message_sender_->SendCrosState(/*notification_enabled=*/true,
                                 /*camera_roll_enabled=*/false);
  VerifyMessage(proto::MessageType::PROVIDE_CROS_STATE, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendUpdateNotificationModeRequest) {
  proto::UpdateNotificationModeRequest request;
  request.set_notification_mode(proto::NotificationMode::DO_NOT_DISTURB_ON);

  message_sender_->SendUpdateNotificationModeRequest(
      /*do_not_disturbed_enabled=*/true);
  VerifyMessage(proto::MessageType::UPDATE_NOTIFICATION_MODE_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendUpdateBatteryModeRequest) {
  proto::UpdateBatteryModeRequest request;
  request.set_battery_mode(proto::BatteryMode::BATTERY_SAVER_ON);

  message_sender_->SendUpdateBatteryModeRequest(
      /*battery_saver_mode_enabled=*/true);
  VerifyMessage(proto::MessageType::UPDATE_BATTERY_MODE_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendDismissNotificationRequest) {
  const int expected_id = 24;

  proto::DismissNotificationRequest request;
  request.set_notification_id(expected_id);

  message_sender_->SendDismissNotificationRequest(expected_id);
  VerifyMessage(proto::MessageType::DISMISS_NOTIFICATION_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendNotificationInlineReplyRequest) {
  const int expected_id = 24;
  const std::u16string expected_reply(u"Test message");

  proto::NotificationInlineReplyRequest request;
  request.set_notification_id(expected_id);
  request.set_reply_text(base::UTF16ToUTF8(expected_reply));

  message_sender_->SendNotificationInlineReplyRequest(expected_id,
                                                      expected_reply);
  VerifyMessage(proto::MessageType::NOTIFICATION_INLINE_REPLY_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendShowNotificationAccessSetupRequest) {
  proto::ShowNotificationAccessSetupRequest request;

  message_sender_->SendShowNotificationAccessSetupRequest();
  VerifyMessage(proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_REQUEST,
                &request, fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendRingDeviceRequest) {
  proto::RingDeviceRequest request;
  request.set_ring_status(proto::FindMyDeviceRingStatus::RINGING);

  message_sender_->SendRingDeviceRequest(/*device_ringing_enabled=*/true);
  VerifyMessage(proto::MessageType::RING_DEVICE_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendFetchCameraRollItemsRequest) {
  proto::FetchCameraRollItemsRequest request;
  request.add_current_item_metadata();
  request.mutable_current_item_metadata(0)->set_key("key0");
  request.add_current_item_metadata();
  request.mutable_current_item_metadata(1)->set_key("key1");

  message_sender_->SendFetchCameraRollItemsRequest(request);

  VerifyMessage(proto::MessageType::FETCH_CAMERA_ROLL_ITEMS_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendFetchCameraRollItemDataRequest) {
  proto::FetchCameraRollItemDataRequest request;
  request.mutable_metadata()->set_key("key0");

  message_sender_->SendFetchCameraRollItemDataRequest(request);

  VerifyMessage(proto::MessageType::FETCH_CAMERA_ROLL_ITEM_DATA_REQUEST,
                &request, fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendInitiateCameraRollItemTransferRequest) {
  proto::InitiateCameraRollItemTransferRequest request;
  request.mutable_metadata()->set_key("key0");
  request.set_payload_id(1234);

  message_sender_->SendInitiateCameraRollItemTransferRequest(request);

  VerifyMessage(proto::MessageType::INITIATE_CAMERA_ROLL_ITEM_TRANSFER_REQUEST,
                &request, fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendFeatureSetupRequest) {
  proto::FeatureSetupRequest request;
  request.set_camera_roll_setup_requested(true);
  request.set_notification_setup_requested(true);

  message_sender_->SendFeatureSetupRequest(/*camera_roll=*/true,
                                           /*notifications=*/true);

  VerifyMessage(proto::MessageType::FEATURE_SETUP_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

TEST_F(MessageSenderImplTest, SendPingRequest) {
  proto::PingRequest request;
  message_sender_->SendPingRequest(request);

  VerifyMessage(proto::MessageType::PING_REQUEST, &request,
                fake_connection_manager_->sent_messages().back());
}

}  // namespace phonehub
}  // namespace ash
