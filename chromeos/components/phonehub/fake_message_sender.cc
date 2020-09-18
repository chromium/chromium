// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_message_sender.h"

namespace chromeos {
namespace phonehub {

FakeMessageSender::FakeMessageSender() = default;
FakeMessageSender::~FakeMessageSender() = default;

void FakeMessageSender::SendCrosState(bool notification_enabled) {
  cros_states_.push_back(notification_enabled);
}

void FakeMessageSender::SendUpdateNotificationModeRequest(
    bool do_not_disturb_enabled) {
  update_notification_mode_requests_.push_back(do_not_disturb_enabled);
}

void FakeMessageSender::SendUpdateBatteryModeRequest(
    bool battery_saver_mode_enabled) {
  update_battery_mode_requests_.push_back(battery_saver_mode_enabled);
}

void FakeMessageSender::SendDismissNotificationRequest(
    int64_t notification_id) {
  dismiss_notification_requests_.push_back(notification_id);
}

void FakeMessageSender::SendNotificationInlineReplyRequest(
    int64_t notification_id,
    const base::string16& reply_text) {
  notification_inline_reply_requests_.push_back(
      std::make_pair(notification_id, reply_text));
}

void FakeMessageSender::SendShowNotificationAccessSetupRequest() {
  show_notification_access_setup_count_++;
}

void FakeMessageSender::SendRingDeviceRequest(bool device_ringing_enabled) {
  ring_device_requests_.push_back(device_ringing_enabled);
}

size_t FakeMessageSender::GetCrosStateCallCount() const {
  return cros_states_.size();
}

size_t FakeMessageSender::GetUpdateNotificationModeRequestCallCount() const {
  return update_notification_mode_requests_.size();
}

size_t FakeMessageSender::GetUpdateBatteryModeRequestCallCount() const {
  return update_battery_mode_requests_.size();
}

size_t FakeMessageSender::GetDismissNotificationRequestCallCount() const {
  return dismiss_notification_requests_.size();
}

size_t FakeMessageSender::GetNotificationInlineReplyRequestCallCount() const {
  return notification_inline_reply_requests_.size();
}

size_t FakeMessageSender::GetRingDeviceRequestCallCount() const {
  return ring_device_requests_.size();
}

bool FakeMessageSender::GetRecentCrosState() const {
  return cros_states_.back();
}

bool FakeMessageSender::GetRecentUpdateNotificationModeRequest() const {
  return update_notification_mode_requests_.back();
}

bool FakeMessageSender::GetRecentUpdateBatteryModeRequest() const {
  return update_battery_mode_requests_.back();
}

int64_t FakeMessageSender::GetRecentDismissNotificationRequest() const {
  return dismiss_notification_requests_.back();
}

const std::pair<int64_t, base::string16>
FakeMessageSender::GetRecentNotificationInlineReplyRequest() const {
  return notification_inline_reply_requests_.back();
}

bool FakeMessageSender::GetRecentRingDeviceRequest() const {
  return ring_device_requests_.back();
}

}  // namespace phonehub
}  // namespace chromeos
