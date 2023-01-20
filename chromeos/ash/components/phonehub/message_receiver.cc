// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/message_receiver.h"

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

MessageReceiver::MessageReceiver() = default;
MessageReceiver::~MessageReceiver() = default;

void MessageReceiver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MessageReceiver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageReceiver::NotifyPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  for (auto& observer : observer_list_)
    observer.OnPhoneStatusSnapshotReceived(phone_status_snapshot);
}

void MessageReceiver::NotifyPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  for (auto& observer : observer_list_)
    observer.OnPhoneStatusUpdateReceived(phone_status_update);
}

void MessageReceiver::NotifyFeatureSetupResponseReceived(
    proto::FeatureSetupResponse response) {
  for (auto& observer : observer_list_)
    observer.OnFeatureSetupResponseReceived(response);
}

void MessageReceiver::NotifyFetchCameraRollItemsResponseReceived(
    const proto::FetchCameraRollItemsResponse& response) {
  for (auto& observer : observer_list_)
    observer.OnFetchCameraRollItemsResponseReceived(response);
}

void MessageReceiver::NotifyFetchCameraRollItemDataResponseReceived(
    const proto::FetchCameraRollItemDataResponse& response) {
  for (auto& observer : observer_list_)
    observer.OnFetchCameraRollItemDataResponseReceived(response);
}

void MessageReceiver::NotifyPingResponseReceived() {
  for (auto& observer : observer_list_)
    observer.OnPingResponseReceived();
}

void MessageReceiver::NotifyAppStreamUpdateReceived(
    const proto::AppStreamUpdate app_stream_update) {
  for (auto& observer : observer_list_)
    observer.OnAppStreamUpdateReceived(app_stream_update);
}

void MessageReceiver::NotifyAppListUpdateReceived(
    const proto::AppListUpdate app_list_update) {
  for (auto& observer : observer_list_)
    observer.OnAppListUpdateReceived(app_list_update);
}

void MessageReceiver::NotifyAppListIncrementalUpdateReceived(
    const proto::AppListIncrementalUpdate app_list_incremental_update) {
  for (auto& observer : observer_list_) {
    observer.OnAppListIncrementalUpdateReceived(app_list_incremental_update);
  }
}

}  // namespace phonehub
}  // namespace ash
