// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/stub_device_sync.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/stub_multidevice_util.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"

namespace chromeos {

namespace device_sync {

namespace {

// Helper function for SetSoftwareFeatureState and SetFeatureStatus, sets the
// |software_feature| to the correct |enabled| value for the |device| and
// handles edge cases.
void SetDeviceSoftwareFeatureState(
    multidevice::RemoteDevice& device,
    multidevice::SoftwareFeature software_feature,
    bool enabled) {
  multidevice::SoftwareFeatureState new_state =
      enabled ? multidevice::SoftwareFeatureState::kEnabled
              : multidevice::SoftwareFeatureState::kSupported;

  device.software_features[software_feature] = new_state;

  if (software_feature != multidevice::SoftwareFeature::kBetterTogetherHost) {
    return;
  }

  static const multidevice::SoftwareFeature
      kFeatureUpdatedByPhoneWhenSuiteStateChanged[] = {
          multidevice::SoftwareFeature::kSmartLockHost,
          multidevice::SoftwareFeature::kInstantTetheringHost,
          multidevice::SoftwareFeature::kMessagesForWebHost,
          multidevice::SoftwareFeature::kPhoneHubHost,
          multidevice::SoftwareFeature::kWifiSyncHost,
      };

  // Special case: when the Chrome OS device changes the value of the phone's
  // kBetterTogetherHost field, the phone updates all other host feature
  // values to match the new value. Simulate that interaction.
  for (const auto& feature : kFeatureUpdatedByPhoneWhenSuiteStateChanged)
    device.software_features[feature] = new_state;
}

}  // namespace

StubDeviceSync::StubDeviceSync()
    : DeviceSyncBase(),
      synced_devices_{multidevice::CreateStubHostPhone(),
                      multidevice::CreateStubClientComputer()},
      local_device_metadata_(multidevice::CreateStubClientComputer()) {}

StubDeviceSync::~StubDeviceSync() = default;

multidevice::RemoteDevice* StubDeviceSync::GetRemoteDevice(
    const base::Optional<std::string>& device_public_key,
    const base::Optional<std::string>& device_instance_id) {
  auto it = std::find_if(synced_devices_.begin(), synced_devices_.end(),
                         [&](const auto& device) {
                           if (device_public_key != base::nullopt)
                             return device.public_key == device_public_key;
                           if (device_instance_id != base::nullopt)
                             return device.instance_id == device_instance_id;
                           return false;
                         });
  if (it == synced_devices_.end()) {
    return nullptr;
  }
  return &(*it);
}

void StubDeviceSync::ForceEnrollmentNow(ForceEnrollmentNowCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void StubDeviceSync::ForceSyncNow(ForceSyncNowCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void StubDeviceSync::GetLocalDeviceMetadata(
    GetLocalDeviceMetadataCallback callback) {
  std::move(callback).Run(local_device_metadata_);
}

void StubDeviceSync::GetSyncedDevices(GetSyncedDevicesCallback callback) {
  std::move(callback).Run(synced_devices_);
}

void StubDeviceSync::SetSoftwareFeatureState(
    const std::string& device_public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    SetSoftwareFeatureStateCallback callback) {
  multidevice::RemoteDevice* device =
      GetRemoteDevice(device_public_key, /*device_instance_id=*/base::nullopt);
  if (device == nullptr) {
    std::move(callback).Run(
        /*result=*/mojom::NetworkRequestResult::kBadRequest);
    return;
  }

  // Once software feature set for the appropriate device, return success.
  SetDeviceSoftwareFeatureState(*device, software_feature, enabled);
  NotifyOnNewDevicesSynced();
  std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess);
}

void StubDeviceSync::SetFeatureStatus(const std::string& device_instance_id,
                                      multidevice::SoftwareFeature feature,
                                      FeatureStatusChange status_change,
                                      SetFeatureStatusCallback callback) {
  multidevice::RemoteDevice* device =
      GetRemoteDevice(/*device_public_key=*/base::nullopt, device_instance_id);
  if (device == nullptr) {
    std::move(callback).Run(
        /*result=*/mojom::NetworkRequestResult::kBadRequest);
    return;
  }

  SetDeviceSoftwareFeatureState(*device, feature,
                                status_change != FeatureStatusChange::kDisable);
  NotifyOnNewDevicesSynced();
  std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess);
}

void StubDeviceSync::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  multidevice::RemoteDeviceList eligible_devices;
  multidevice::RemoteDeviceList ineligible_devices;

  std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess,
                          /*response=*/mojom::FindEligibleDevicesResponse::New(
                              eligible_devices, ineligible_devices));
}

void StubDeviceSync::NotifyDevices(
    const std::vector<std::string>& device_instance_ids,
    cryptauthv2::TargetService target_service,
    multidevice::SoftwareFeature feature,
    NotifyDevicesCallback callback) {
  std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess);
}

void StubDeviceSync::GetDebugInfo(GetDebugInfoCallback callback) {
  // Arbitrary values.
  std::move(callback).Run(mojom::DebugInfo::New(
      /*last_enrollment_time=*/base::Time::Now(),
      /*time_to_next_enrollment_attempt=*/base::TimeDelta::FromMilliseconds(10),
      /*is_recovering_from_enrollment_failure=*/false,
      /*is_enrollment_in_progress=*/false,
      /*last_sync_time=*/base::Time::Now(),
      /*time_to_next_sync_attempt=*/base::TimeDelta::FromMilliseconds(10),
      /*is_recovering_from_sync_failure=*/false,
      /*is_sync_in_progress=*/false));
}

void StubDeviceSync::GetDevicesActivityStatus(
    GetDevicesActivityStatusCallback callback) {
  std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess,
                          /*response=*/base::nullopt);
}

}  // namespace device_sync

}  // namespace chromeos
