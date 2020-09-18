// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/message_sender_impl.h"

#include <stdint.h>
#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/fake_connection_manager.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {

class MessageSenderImplTest : public testing::Test {
 protected:
  MessageSenderImplTest() = default;
  MessageSenderImplTest(const MessageSenderImplTest&) = delete;
  MessageSenderImplTest& operator=(const MessageSenderImplTest&) = delete;
  ~MessageSenderImplTest() override = default;

  void SetUp() override {
    fake_connection_manager_ = std::make_unique<FakeConnectionManager>();
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
    EXPECT_EQ(static_cast<uint16_t>(expected_message_type),
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

  std::unique_ptr<FakeConnectionManager> fake_connection_manager_;
  std::unique_ptr<MessageSenderImpl> message_sender_;
};

TEST_F(MessageSenderImplTest, SendCrossState) {
  proto::CrosState request;
  request.set_notification_setting(
      proto::NotificationSetting::NOTIFICATIONS_ON);

  message_sender_->SendCrosState(/*notification_enabled=*/true);
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
  const base::string16 expected_reply(base::UTF8ToUTF16("Test message"));

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

}  // namespace phonehub
}  // namespace chromeos
