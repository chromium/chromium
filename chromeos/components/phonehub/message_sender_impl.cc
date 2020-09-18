// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/message_sender_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/connection_manager.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {
namespace {

std::string SerializeMessage(proto::MessageType message_type,
                             const google::protobuf::MessageLite* request) {
  // Add two space characters, followed by the serialized proto.
  std::string message = base::StrCat({"  ", request->SerializeAsString()});

  // Replace the first two characters with |message_type| as a 16-bit int.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(message.data()));
  *ptr = static_cast<uint16_t>(message_type);
  return message;
}

}  // namespace

MessageSenderImpl::MessageSenderImpl(ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
  DCHECK(connection_manager_);
}

MessageSenderImpl::~MessageSenderImpl() = default;

void MessageSenderImpl::SendCrosState(bool notification_setting_enabled) {
  proto::NotificationSetting is_notification_enabled =
      notification_setting_enabled
          ? proto::NotificationSetting::NOTIFICATIONS_ON
          : proto::NotificationSetting::NOTIFICATIONS_OFF;
  proto::CrosState request;
  request.set_notification_setting(is_notification_enabled);

  connection_manager_->SendMessage(
      SerializeMessage(proto::MessageType::PROVIDE_CROS_STATE, &request));
}

void MessageSenderImpl::SendUpdateNotificationModeRequest(
    bool do_not_disturb_enabled) {
  proto::NotificationMode notification_mode =
      do_not_disturb_enabled ? proto::NotificationMode::DO_NOT_DISTURB_ON
                             : proto::NotificationMode::DO_NOT_DISTURB_OFF;
  proto::UpdateNotificationModeRequest request;
  request.set_notification_mode(notification_mode);

  connection_manager_->SendMessage(SerializeMessage(
      proto::MessageType::UPDATE_NOTIFICATION_MODE_REQUEST, &request));
}

void MessageSenderImpl::SendUpdateBatteryModeRequest(
    bool battery_saver_mode_enabled) {
  proto::BatteryMode battery_mode = battery_saver_mode_enabled
                                        ? proto::BatteryMode::BATTERY_SAVER_ON
                                        : proto::BatteryMode::BATTERY_SAVER_OFF;
  proto::UpdateBatteryModeRequest request;
  request.set_battery_mode(battery_mode);

  connection_manager_->SendMessage(SerializeMessage(
      proto::MessageType::UPDATE_BATTERY_MODE_REQUEST, &request));
}

void MessageSenderImpl::SendDismissNotificationRequest(
    int64_t notification_id) {
  proto::DismissNotificationRequest request;
  request.set_notification_id(notification_id);

  connection_manager_->SendMessage(SerializeMessage(
      proto::MessageType::DISMISS_NOTIFICATION_REQUEST, &request));
}

void MessageSenderImpl::SendNotificationInlineReplyRequest(
    int64_t notification_id,
    const base::string16& reply_text) {
  proto::NotificationInlineReplyRequest request;
  request.set_notification_id(notification_id);
  request.set_reply_text(base::UTF16ToUTF8(reply_text));

  connection_manager_->SendMessage(SerializeMessage(
      proto::MessageType::NOTIFICATION_INLINE_REPLY_REQUEST, &request));
}

void MessageSenderImpl::SendShowNotificationAccessSetupRequest() {
  proto::ShowNotificationAccessSetupRequest request;

  connection_manager_->SendMessage(SerializeMessage(
      proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_REQUEST, &request));
}

void MessageSenderImpl::SendRingDeviceRequest(bool device_ringing_enabled) {
  proto::FindMyDeviceRingStatus ringing_enabled =
      device_ringing_enabled ? proto::FindMyDeviceRingStatus::RINGING
                             : proto::FindMyDeviceRingStatus::NOT_RINGING;
  proto::RingDeviceRequest request;
  request.set_ring_status(ringing_enabled);

  connection_manager_->SendMessage(
      SerializeMessage(proto::MessageType::RING_DEVICE_REQUEST, &request));
}

}  // namespace phonehub
}  // namespace chromeos