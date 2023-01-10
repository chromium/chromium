// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_message_sender.h"

namespace ash {
namespace phonehub {

FakeMessageSender::FakeMessageSender() = default;
FakeMessageSender::~FakeMessageSender() = default;

void FakeMessageSender::SendCrosState(bool notification_enabled,
                                      bool camera_roll_enabled,
                                      const std::vector<std::string>* certs) {
  auto new_message =
      std::make_tuple(notification_enabled, camera_roll_enabled, certs);
  cros_states_.push_back(new_message);
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
    const std::u16string& reply_text) {
  notification_inline_reply_requests_.push_back(
      std::make_pair(notification_id, reply_text));
}

void FakeMessageSender::SendShowNotificationAccessSetupRequest() {
  show_notification_access_setup_count_++;
}

void FakeMessageSender::SendRingDeviceRequest(bool device_ringing_enabled) {
  ring_device_requests_.push_back(device_ringing_enabled);
}

void FakeMessageSender::SendFetchCameraRollItemsRequest(
    const proto::FetchCameraRollItemsRequest& request) {
  fetch_camera_roll_items_requests_.push_back(request);
}

void FakeMessageSender::SendFetchCameraRollItemDataRequest(
    const proto::FetchCameraRollItemDataRequest& request) {
  fetch_camera_roll_item_data_requests_.push_back(request);
}

void FakeMessageSender::SendInitiateCameraRollItemTransferRequest(
    const proto::InitiateCameraRollItemTransferRequest& request) {
  initiate_camera_roll_item_transfer_requests_.push_back(request);
}

void FakeMessageSender::SendPingRequest(const proto::PingRequest& request) {
  send_ping_requests_.push_back(request);
}

void FakeMessageSender::SendFeatureSetupRequest(bool camera_roll,
                                                bool notifications) {
  feature_setup_requests_.push_back(std::make_pair(camera_roll, notifications));
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

size_t FakeMessageSender::GetFetchCameraRollItemsRequestCallCount() const {
  return fetch_camera_roll_items_requests_.size();
}

size_t FakeMessageSender::GetFetchCameraRollItemDataRequestCallCount() const {
  return fetch_camera_roll_item_data_requests_.size();
}

size_t FakeMessageSender::GetInitiateCameraRollItemTransferRequestCallCount()
    const {
  return initiate_camera_roll_item_transfer_requests_.size();
}

size_t FakeMessageSender::GetFeatureSetupRequestCallCount() const {
  return feature_setup_requests_.size();
}

size_t FakeMessageSender::GetPingRequestCallCount() const {
  return send_ping_requests_.size();
}

std::tuple<bool, bool, const std::vector<std::string>*>
FakeMessageSender::GetRecentCrosState() const {
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

const std::pair<int64_t, std::u16string>
FakeMessageSender::GetRecentNotificationInlineReplyRequest() const {
  return notification_inline_reply_requests_.back();
}

bool FakeMessageSender::GetRecentRingDeviceRequest() const {
  return ring_device_requests_.back();
}

const proto::FetchCameraRollItemsRequest&
FakeMessageSender::GetRecentFetchCameraRollItemsRequest() const {
  return fetch_camera_roll_items_requests_.back();
}

const proto::FetchCameraRollItemDataRequest&
FakeMessageSender::GetRecentFetchCameraRollItemDataRequest() const {
  return fetch_camera_roll_item_data_requests_.back();
}

const proto::InitiateCameraRollItemTransferRequest&
FakeMessageSender::GetRecentInitiateCameraRollItemTransferRequest() const {
  return initiate_camera_roll_item_transfer_requests_.back();
}

const proto::PingRequest& FakeMessageSender::GetRecentPingRequest() const {
  return send_ping_requests_.back();
}

std::pair<bool, bool> FakeMessageSender::GetRecentFeatureSetupRequest() const {
  return feature_setup_requests_.back();
}

}  // namespace phonehub
}  // namespace ash
