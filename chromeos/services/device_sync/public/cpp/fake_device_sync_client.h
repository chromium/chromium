// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_DEVICE_SYNC_CLIENT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_DEVICE_SYNC_CLIENT_H_

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace device_sync {

// Test double implementation of DeviceSyncClient.
class FakeDeviceSyncClient : public DeviceSyncClient {
 public:
  FakeDeviceSyncClient();
  ~FakeDeviceSyncClient() override;

  int GetForceEnrollmentNowCallbackQueueSize();
  int GetForceSyncNowCallbackQueueSize();
  int GetSetSoftwareFeatureStateCallbackQueueSize();
  int GetFindEligibleDevicesCallbackQueueSize();
  int GetGetDebugInfoCallbackQueueSize();

  void InvokePendingForceEnrollmentNowCallback(bool success);
  void InvokePendingForceSyncNowCallback(bool success);
  void InvokePendingSetSoftwareFeatureStateCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingFindEligibleDevicesCallback(
      mojom::NetworkRequestResult result_code,
      multidevice::RemoteDeviceRefList eligible_devices,
      multidevice::RemoteDeviceRefList ineligible_devices);
  void InvokePendingGetDevicesActivityStatusCallback(
      mojom::NetworkRequestResult result_code,
      base::Optional<std::vector<mojom::DeviceActivityStatusPtr>>
          device_activity_status);
  void InvokePendingGetDebugInfoCallback(mojom::DebugInfoPtr debug_info_ptr);

  void set_synced_devices(multidevice::RemoteDeviceRefList synced_devices) {
    synced_devices_ = synced_devices;
  }

  void set_local_device_metadata(
      base::Optional<multidevice::RemoteDeviceRef> local_device_metadata) {
    local_device_metadata_ = local_device_metadata;
  }

  using DeviceSyncClient::NotifyEnrollmentFinished;
  using DeviceSyncClient::NotifyNewDevicesSynced;
  using DeviceSyncClient::NotifyReady;

 private:
  // DeviceSyncClient:
  void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(mojom::DeviceSync::ForceSyncNowCallback callback) override;
  multidevice::RemoteDeviceRefList GetSyncedDevices() override;
  base::Optional<multidevice::RemoteDeviceRef> GetLocalDeviceMetadata()
      override;
  void SetSoftwareFeatureState(
      const std::string public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void GetDevicesActivityStatus(
      mojom::DeviceSync::GetDevicesActivityStatusCallback callback) override;
  void GetDebugInfo(mojom::DeviceSync::GetDebugInfoCallback callback) override;

  multidevice::RemoteDeviceRefList synced_devices_;
  base::Optional<multidevice::RemoteDeviceRef> local_device_metadata_;

  std::queue<mojom::DeviceSync::ForceEnrollmentNowCallback>
      force_enrollment_now_callback_queue_;
  std::queue<mojom::DeviceSync::ForceSyncNowCallback>
      force_sync_now_callback_queue_;
  std::queue<mojom::DeviceSync::SetSoftwareFeatureStateCallback>
      set_software_feature_state_callback_queue_;
  std::queue<FindEligibleDevicesCallback> find_eligible_devices_callback_queue_;
  std::queue<mojom::DeviceSync::GetDevicesActivityStatusCallback>
      get_devices_activity_status_callback_queue_;
  std::queue<mojom::DeviceSync::GetDebugInfoCallback>
      get_debug_info_callback_queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceSyncClient);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_DEVICE_SYNC_CLIENT_H_
