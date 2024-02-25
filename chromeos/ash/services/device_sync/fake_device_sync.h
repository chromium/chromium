// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_

#include <queue>

#include "chromeos/ash/services/device_sync/device_sync_base.h"

namespace ash {

namespace device_sync {

// Test double DeviceSync implementation.
class FakeDeviceSync : public DeviceSyncBase {
 public:
  FakeDeviceSync();

  FakeDeviceSync(const FakeDeviceSync&) = delete;
  FakeDeviceSync& operator=(const FakeDeviceSync&) = delete;

  ~FakeDeviceSync() override;

  using DeviceSyncBase::NotifyOnEnrollmentFinished;
  using DeviceSyncBase::NotifyOnNewDevicesSynced;

  void set_force_enrollment_now_completed_success(
      bool force_enrollment_now_completed_success) {
    force_enrollment_now_completed_success_ =
        force_enrollment_now_completed_success;
  }

  void set_force_sync_now_completed_success(
      bool force_sync_now_completed_success) {
    force_sync_now_completed_success_ = force_sync_now_completed_success;
  }

  void InvokePendingGetGroupPrivateKeyStatusCallback(
      GroupPrivateKeyStatus status);
  void InvokePendingGetBetterTogetherMetadataStatusCallback(
      BetterTogetherMetadataStatus status);
  void InvokePendingGetLocalDeviceMetadataCallback(
      const std::optional<multidevice::RemoteDevice>& local_device_metadata);
  void InvokePendingGetSyncedDevicesCallback(
      const std::optional<std::vector<multidevice::RemoteDevice>>&
          remote_devices);
  void InvokePendingSetSoftwareFeatureStateCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingSetFeatureStatusCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingFindEligibleDevicesCallback(
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr find_eligible_devices_response_ptr);
  void InvokePendingNotifyDevicesCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingGetDevicesActivityStatusCallback(
      mojom::NetworkRequestResult result_code,
      std::optional<std::vector<mojom::DeviceActivityStatusPtr>>
          get_devices_activity_status_response);
  void InvokePendingGetDebugInfoCallback(mojom::DebugInfoPtr debug_info_ptr);

 protected:
  // device_sync::mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetGroupPrivateKeyStatus(
      GetGroupPrivateKeyStatusCallback callback) override;
  void GetBetterTogetherMetadataStatus(
      GetBetterTogetherMetadataStatusCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void SetFeatureStatus(const std::string& device_instance_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change,
                        SetFeatureStatusCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void NotifyDevices(const std::vector<std::string>& device_instance_ids,
                     cryptauthv2::TargetService target_service,
                     multidevice::SoftwareFeature feature,
                     NotifyDevicesCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void GetDevicesActivityStatus(
      GetDevicesActivityStatusCallback callback) override;

 private:
  bool force_enrollment_now_completed_success_ = true;
  bool force_sync_now_completed_success_ = true;

  std::queue<GetGroupPrivateKeyStatusCallback>
      get_group_private_key_status_callback_queue_;
  std::queue<GetBetterTogetherMetadataStatusCallback>
      get_better_together_metadata_status_callback_queue_;
  std::queue<GetLocalDeviceMetadataCallback>
      get_local_device_metadata_callback_queue_;
  std::queue<GetSyncedDevicesCallback> get_synced_devices_callback_queue_;
  std::queue<SetSoftwareFeatureStateCallback>
      set_software_feature_state_callback_queue_;
  std::queue<SetFeatureStatusCallback> set_feature_status_callback_queue_;
  std::queue<FindEligibleDevicesCallback> find_eligible_devices_callback_queue_;
  std::queue<NotifyDevicesCallback> notify_devices_callback_queue_;
  std::queue<GetDevicesActivityStatusCallback>
      get_devices_activity_status_callback_queue_;
  std::queue<GetDebugInfoCallback> get_debug_info_callback_queue_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
