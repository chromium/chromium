// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"

#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/components/multidevice/remote_device_cache.h"

namespace chromeos {

namespace device_sync {

FakeDeviceSyncClient::FakeDeviceSyncClient() = default;

FakeDeviceSyncClient::~FakeDeviceSyncClient() = default;

void FakeDeviceSyncClient::ForceEnrollmentNow(
    mojom::DeviceSync::ForceEnrollmentNowCallback callback) {
  force_enrollment_now_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::ForceSyncNow(
    mojom::DeviceSync::ForceSyncNowCallback callback) {
  force_sync_now_callback_queue_.push(std::move(callback));
}

multidevice::RemoteDeviceRefList FakeDeviceSyncClient::GetSyncedDevices() {
  return synced_devices_;
}

base::Optional<multidevice::RemoteDeviceRef>
FakeDeviceSyncClient::GetLocalDeviceMetadata() {
  return local_device_metadata_;
}

void FakeDeviceSyncClient::SetSoftwareFeatureState(
    const std::string public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) {
  set_software_feature_state_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  find_eligible_devices_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::GetDevicesActivityStatus(
    mojom::DeviceSync::GetDevicesActivityStatusCallback callback) {
  get_devices_activity_status_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::GetDebugInfo(
    mojom::DeviceSync::GetDebugInfoCallback callback) {
  get_debug_info_callback_queue_.push(std::move(callback));
}

int FakeDeviceSyncClient::GetForceEnrollmentNowCallbackQueueSize() {
  return force_enrollment_now_callback_queue_.size();
}

int FakeDeviceSyncClient::GetForceSyncNowCallbackQueueSize() {
  return force_sync_now_callback_queue_.size();
}

int FakeDeviceSyncClient::GetSetSoftwareFeatureStateCallbackQueueSize() {
  return set_software_feature_state_callback_queue_.size();
}

int FakeDeviceSyncClient::GetFindEligibleDevicesCallbackQueueSize() {
  return find_eligible_devices_callback_queue_.size();
}

int FakeDeviceSyncClient::GetGetDebugInfoCallbackQueueSize() {
  return get_debug_info_callback_queue_.size();
}

void FakeDeviceSyncClient::InvokePendingForceEnrollmentNowCallback(
    bool success) {
  DCHECK(force_enrollment_now_callback_queue_.size() > 0);
  std::move(force_enrollment_now_callback_queue_.front()).Run(success);
  force_enrollment_now_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingForceSyncNowCallback(bool success) {
  DCHECK(force_sync_now_callback_queue_.size() > 0);
  std::move(force_sync_now_callback_queue_.front()).Run(success);
  force_sync_now_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingSetSoftwareFeatureStateCallback(
    mojom::NetworkRequestResult result_code) {
  DCHECK(set_software_feature_state_callback_queue_.size() > 0);
  std::move(set_software_feature_state_callback_queue_.front())
      .Run(result_code);
  set_software_feature_state_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingFindEligibleDevicesCallback(
    mojom::NetworkRequestResult result_code,
    multidevice::RemoteDeviceRefList eligible_devices,
    multidevice::RemoteDeviceRefList ineligible_devices) {
  DCHECK(find_eligible_devices_callback_queue_.size() > 0);
  std::move(find_eligible_devices_callback_queue_.front())
      .Run(result_code, eligible_devices, ineligible_devices);
  find_eligible_devices_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingGetDevicesActivityStatusCallback(
    mojom::NetworkRequestResult result_code,
    base::Optional<std::vector<mojom::DeviceActivityStatusPtr>>
        device_activity_status) {
  DCHECK(get_devices_activity_status_callback_queue_.size() > 0);
  std::move(get_devices_activity_status_callback_queue_.front())
      .Run(result_code, std::move(device_activity_status));
  get_devices_activity_status_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingGetDebugInfoCallback(
    mojom::DebugInfoPtr debug_info_ptr) {
  DCHECK(get_debug_info_callback_queue_.size() > 0);
  std::move(get_debug_info_callback_queue_.front())
      .Run(std::move(debug_info_ptr));
  get_debug_info_callback_queue_.pop();
}

}  // namespace device_sync

}  // namespace chromeos
