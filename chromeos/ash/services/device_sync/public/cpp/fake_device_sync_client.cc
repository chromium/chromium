// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"

#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"

namespace ash {

namespace device_sync {
FakeDeviceSyncClient::SetSoftwareFeatureStateInputs::
    SetSoftwareFeatureStateInputs(
        const std::string& public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        bool is_exclusive,
        mojom::DeviceSync::SetSoftwareFeatureStateCallback callback)
    : public_key(public_key),
      software_feature(software_feature),
      enabled(enabled),
      is_exclusive(is_exclusive),
      callback(std::move(callback)) {}

FakeDeviceSyncClient::SetSoftwareFeatureStateInputs::
    SetSoftwareFeatureStateInputs(SetSoftwareFeatureStateInputs&&) = default;

FakeDeviceSyncClient::SetSoftwareFeatureStateInputs::
    ~SetSoftwareFeatureStateInputs() = default;

FakeDeviceSyncClient::SetFeatureStatusInputs::SetFeatureStatusInputs(
    const std::string& device_instance_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    mojom::DeviceSync::SetFeatureStatusCallback callback)
    : device_instance_id(device_instance_id),
      feature(feature),
      status_change(status_change),
      callback(std::move(callback)) {}

FakeDeviceSyncClient::SetFeatureStatusInputs::SetFeatureStatusInputs(
    SetFeatureStatusInputs&&) = default;

FakeDeviceSyncClient::SetFeatureStatusInputs::~SetFeatureStatusInputs() =
    default;

FakeDeviceSyncClient::FindEligibleDevicesInputs::FindEligibleDevicesInputs(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback)
    : software_feature(software_feature), callback(std::move(callback)) {}

FakeDeviceSyncClient::FindEligibleDevicesInputs::FindEligibleDevicesInputs(
    FindEligibleDevicesInputs&&) = default;

FakeDeviceSyncClient::FindEligibleDevicesInputs::~FindEligibleDevicesInputs() =
    default;

FakeDeviceSyncClient::NotifyDevicesInputs::NotifyDevicesInputs(
    const std::vector<std::string>& device_instance_ids,
    cryptauthv2::TargetService target_service,
    multidevice::SoftwareFeature feature,
    mojom::DeviceSync::NotifyDevicesCallback callback)
    : device_instance_ids(device_instance_ids),
      target_service(target_service),
      feature(feature),
      callback(std::move(callback)) {}

FakeDeviceSyncClient::NotifyDevicesInputs::NotifyDevicesInputs(
    NotifyDevicesInputs&&) = default;

FakeDeviceSyncClient::NotifyDevicesInputs::~NotifyDevicesInputs() = default;

FakeDeviceSyncClient::FakeDeviceSyncClient() = default;

FakeDeviceSyncClient::~FakeDeviceSyncClient() = default;

void FakeDeviceSyncClient::ForceEnrollmentNow(
    mojom::DeviceSync::ForceEnrollmentNowCallback callback) {
  force_enrollment_now_callback_queue_.push_back(std::move(callback));
}

void FakeDeviceSyncClient::ForceSyncNow(
    mojom::DeviceSync::ForceSyncNowCallback callback) {
  force_sync_now_callback_queue_.push_back(std::move(callback));
}

void FakeDeviceSyncClient::GetBetterTogetherMetadataStatus(
    mojom::DeviceSync::GetBetterTogetherMetadataStatusCallback callback) {
  get_better_together_metadata_status_callback_queue_.push_back(
      std::move(callback));
}

void FakeDeviceSyncClient::GetGroupPrivateKeyStatus(
    mojom::DeviceSync::GetGroupPrivateKeyStatusCallback callback) {
  get_group_private_key_status_callback_queue_.push_back(std::move(callback));
}

multidevice::RemoteDeviceRefList FakeDeviceSyncClient::GetSyncedDevices() {
  return synced_devices_;
}

std::optional<multidevice::RemoteDeviceRef>
FakeDeviceSyncClient::GetLocalDeviceMetadata() {
  return local_device_metadata_;
}

void FakeDeviceSyncClient::SetSoftwareFeatureState(
    const std::string public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) {
  set_software_feature_state_inputs_queue_.emplace_back(
      public_key, software_feature, enabled, is_exclusive, std::move(callback));
}

void FakeDeviceSyncClient::SetFeatureStatus(
    const std::string& device_instance_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    mojom::DeviceSync::SetFeatureStatusCallback callback) {
  set_feature_status_inputs_queue_.emplace_back(
      device_instance_id, feature, status_change, std::move(callback));
}

void FakeDeviceSyncClient::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  find_eligible_devices_inputs_queue_.emplace_back(software_feature,
                                                   std::move(callback));
}

void FakeDeviceSyncClient::NotifyDevices(
    const std::vector<std::string>& device_instance_ids,
    cryptauthv2::TargetService target_service,
    multidevice::SoftwareFeature feature,
    mojom::DeviceSync::NotifyDevicesCallback callback) {
  notify_devices_inputs_queue_.emplace_back(device_instance_ids, target_service,
                                            feature, std::move(callback));
}

void FakeDeviceSyncClient::GetDevicesActivityStatus(
    mojom::DeviceSync::GetDevicesActivityStatusCallback callback) {
  get_devices_activity_status_callback_queue_.push_back(std::move(callback));
}

void FakeDeviceSyncClient::GetDebugInfo(
    mojom::DeviceSync::GetDebugInfoCallback callback) {
  get_debug_info_callback_queue_.push_back(std::move(callback));
}

int FakeDeviceSyncClient::GetForceEnrollmentNowCallbackQueueSize() const {
  return force_enrollment_now_callback_queue_.size();
}

int FakeDeviceSyncClient::GetForceSyncNowCallbackQueueSize() const {
  return force_sync_now_callback_queue_.size();
}

int FakeDeviceSyncClient::GetBetterTogetherMetadataStatusCallbackQueueSize()
    const {
  return get_better_together_metadata_status_callback_queue_.size();
}

int FakeDeviceSyncClient::GetGroupPrivateKeyStatusCallbackQueueSize() const {
  return get_group_private_key_status_callback_queue_.size();
}

int FakeDeviceSyncClient::GetSetSoftwareFeatureStateInputsQueueSize() const {
  return set_software_feature_state_inputs_queue_.size();
}

int FakeDeviceSyncClient::GetSetFeatureStatusInputsQueueSize() const {
  return set_feature_status_inputs_queue_.size();
}

int FakeDeviceSyncClient::GetFindEligibleDevicesInputsQueueSize() const {
  return find_eligible_devices_inputs_queue_.size();
}

int FakeDeviceSyncClient::GetNotifyDevicesInputsQueueSize() const {
  return notify_devices_inputs_queue_.size();
}

int FakeDeviceSyncClient::GetGetDebugInfoCallbackQueueSize() const {
  return get_debug_info_callback_queue_.size();
}

void FakeDeviceSyncClient::InvokePendingForceEnrollmentNowCallback(
    bool success) {
  DCHECK(force_enrollment_now_callback_queue_.size() > 0);
  std::move(force_enrollment_now_callback_queue_.front()).Run(success);
  force_enrollment_now_callback_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingForceSyncNowCallback(bool success) {
  DCHECK(force_sync_now_callback_queue_.size() > 0);
  std::move(force_sync_now_callback_queue_.front()).Run(success);
  force_sync_now_callback_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingGetBetterTogetherMetadataStatusCallback(
    BetterTogetherMetadataStatus status) {
  DCHECK(get_better_together_metadata_status_callback_queue_.size() > 0);
  std::move(get_better_together_metadata_status_callback_queue_.front())
      .Run(status);
  get_better_together_metadata_status_callback_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingGetGroupPrivateKeyStatusCallback(
    GroupPrivateKeyStatus status) {
  DCHECK(get_group_private_key_status_callback_queue_.size() > 0);
  std::move(get_group_private_key_status_callback_queue_.front()).Run(status);
  get_group_private_key_status_callback_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingSetSoftwareFeatureStateCallback(
    mojom::NetworkRequestResult result_code) {
  DCHECK(set_software_feature_state_inputs_queue_.size() > 0);
  std::move(set_software_feature_state_inputs_queue_.front().callback)
      .Run(result_code);
  set_software_feature_state_inputs_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingSetFeatureStatusCallback(
    mojom::NetworkRequestResult result_code) {
  DCHECK(set_feature_status_inputs_queue_.size() > 0);
  std::move(set_feature_status_inputs_queue_.front().callback).Run(result_code);
  set_feature_status_inputs_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingFindEligibleDevicesCallback(
    mojom::NetworkRequestResult result_code,
    multidevice::RemoteDeviceRefList eligible_devices,
    multidevice::RemoteDeviceRefList ineligible_devices) {
  DCHECK(find_eligible_devices_inputs_queue_.size() > 0);
  std::move(find_eligible_devices_inputs_queue_.front().callback)
      .Run(result_code, eligible_devices, ineligible_devices);
  find_eligible_devices_inputs_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingNotifyDevicesCallback(
    mojom::NetworkRequestResult result_code) {
  DCHECK(notify_devices_inputs_queue_.size() > 0);
  std::move(notify_devices_inputs_queue_.front().callback).Run(result_code);
  notify_devices_inputs_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingGetDevicesActivityStatusCallback(
    mojom::NetworkRequestResult result_code,
    std::optional<std::vector<mojom::DeviceActivityStatusPtr>>
        device_activity_status) {
  DCHECK(get_devices_activity_status_callback_queue_.size() > 0);
  std::move(get_devices_activity_status_callback_queue_.front())
      .Run(result_code, std::move(device_activity_status));
  get_devices_activity_status_callback_queue_.pop_front();
}

void FakeDeviceSyncClient::InvokePendingGetDebugInfoCallback(
    mojom::DebugInfoPtr debug_info_ptr) {
  DCHECK(get_debug_info_callback_queue_.size() > 0);
  std::move(get_debug_info_callback_queue_.front())
      .Run(std::move(debug_info_ptr));
  get_debug_info_callback_queue_.pop_front();
}

}  // namespace device_sync

}  // namespace ash
