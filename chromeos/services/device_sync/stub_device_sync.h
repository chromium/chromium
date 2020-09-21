// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_

#include <vector>

#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/services/device_sync/device_sync_base.h"

namespace chromeos {

namespace device_sync {

// Stub Device Sync implementation for Linux CrOS build. Creates two
// fake devices, a fake phone and a fake computer.
class StubDeviceSync : public DeviceSyncBase {
 public:
  StubDeviceSync();
  ~StubDeviceSync() override;

 protected:
  // mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
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
  // Returns the synced device that has a matching |device_public_key| or a
  // matching |device_instance_id|, otherwise returns nullptr.
  multidevice::RemoteDevice* GetRemoteDevice(
      const base::Optional<std::string>& device_public_key,
      const base::Optional<std::string>& device_instance_id);

  std::vector<multidevice::RemoteDevice> synced_devices_;
  base::Optional<multidevice::RemoteDevice> local_device_metadata_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_
