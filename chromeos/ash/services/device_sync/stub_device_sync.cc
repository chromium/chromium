// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/stub_device_sync.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/stub_multidevice_util.h"
#include "chromeos/ash/services/device_sync/device_sync_base.h"
#include "chromeos/ash/services/device_sync/device_sync_impl.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

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
          multidevice::SoftwareFeature::kEcheHost,
          multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
      };

  // Special case: when the Chrome OS device changes the value of the phone's
  // kBetterTogetherHost field, the phone updates all other host feature
  // values to match the new value. Simulate that interaction.
  for (const auto& feature : kFeatureUpdatedByPhoneWhenSuiteStateChanged)
    device.software_features[feature] = new_state;
}

// Stub Device Sync implementation for Linux CrOS build. Creates two
// fake devices, a fake phone and a fake computer.
class StubDeviceSync : public DeviceSyncBase {
 public:
  StubDeviceSync()
      : DeviceSyncBase(),
        synced_devices_{multidevice::CreateStubHostPhone(),
                        multidevice::CreateStubClientComputer()},
        local_device_metadata_(multidevice::CreateStubClientComputer()) {}

  ~StubDeviceSync() override = default;

 protected:
  // mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }

  void ForceSyncNow(ForceSyncNowCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }

  void GetBetterTogetherMetadataStatus(
      GetBetterTogetherMetadataStatusCallback callback) override {
    std::move(callback).Run(/*result=*/BetterTogetherMetadataStatus::
                                kWaitingToProcessDeviceMetadata);
  }

  void GetGroupPrivateKeyStatus(
      GetGroupPrivateKeyStatusCallback callback) override {
    std::move(callback).Run(
        /*result=*/GroupPrivateKeyStatus::kWaitingForGroupPrivateKey);
  }

  void GetLocalDeviceMetadata(
      GetLocalDeviceMetadataCallback callback) override {
    std::move(callback).Run(local_device_metadata_);
  }

  void GetSyncedDevices(GetSyncedDevicesCallback callback) override {
    std::move(callback).Run(synced_devices_);
  }

  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override {
    multidevice::RemoteDevice* device =
        GetRemoteDevice(device_public_key, /*device_instance_id=*/std::nullopt);
    if (!device) {
      std::move(callback).Run(
          /*result=*/mojom::NetworkRequestResult::kBadRequest);
      return;
    }

    // Once software feature set for the appropriate device, return success.
    SetDeviceSoftwareFeatureState(*device, software_feature, enabled);
    NotifyOnNewDevicesSynced();
    std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess);
  }

  void SetFeatureStatus(const std::string& device_instance_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change,
                        SetFeatureStatusCallback callback) override {
    multidevice::RemoteDevice* device = GetRemoteDevice(
        /*device_public_key=*/std::nullopt, device_instance_id);
    if (!device) {
      std::move(callback).Run(
          /*result=*/mojom::NetworkRequestResult::kBadRequest);
      return;
    }

    SetDeviceSoftwareFeatureState(
        *device, feature, status_change != FeatureStatusChange::kDisable);
    NotifyOnNewDevicesSynced();
    std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess);
  }

  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override {
    multidevice::RemoteDeviceList eligible_devices;
    multidevice::RemoteDeviceList ineligible_devices;

    std::move(callback).Run(
        /*result=*/mojom::NetworkRequestResult::kSuccess,
        /*response=*/mojom::FindEligibleDevicesResponse::New(
            eligible_devices, ineligible_devices));
  }

  void NotifyDevices(const std::vector<std::string>& device_instance_ids,
                     cryptauthv2::TargetService target_service,
                     multidevice::SoftwareFeature feature,
                     NotifyDevicesCallback callback) override {
    std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess);
  }

  void GetDebugInfo(GetDebugInfoCallback callback) override {
    // Arbitrary values.
    std::move(callback).Run(mojom::DebugInfo::New(
        /*last_enrollment_time=*/base::Time::Now(),
        /*time_to_next_enrollment_attempt=*/
        base::Milliseconds(10),
        /*is_recovering_from_enrollment_failure=*/false,
        /*is_enrollment_in_progress=*/false,
        /*last_sync_time=*/base::Time::Now(),
        /*time_to_next_sync_attempt=*/base::Milliseconds(10),
        /*is_recovering_from_sync_failure=*/false,
        /*is_sync_in_progress=*/false));
  }

  void GetDevicesActivityStatus(
      GetDevicesActivityStatusCallback callback) override {
    std::move(callback).Run(/*result=*/mojom::NetworkRequestResult::kSuccess,
                            /*response=*/std::nullopt);
  }

 private:
  // Returns the synced device that has a matching |device_public_key| or a
  // matching |device_instance_id|, otherwise returns nullptr.
  multidevice::RemoteDevice* GetRemoteDevice(
      const std::optional<std::string>& device_public_key,
      const std::optional<std::string>& device_instance_id) {
    auto it = base::ranges::find_if(synced_devices_, [&](const auto& device) {
      if (device_public_key.has_value())
        return device.public_key == device_public_key;
      if (device_instance_id.has_value())
        return device.instance_id == device_instance_id;
      return false;
    });
    if (it == synced_devices_.end()) {
      return nullptr;
    }
    return &(*it);
  }

  std::vector<multidevice::RemoteDevice> synced_devices_;
  std::optional<multidevice::RemoteDevice> local_device_metadata_;
};

class StubDeviceSyncImplFactory : public DeviceSyncImpl::Factory {
 public:
  StubDeviceSyncImplFactory() = default;
  ~StubDeviceSyncImplFactory() override = default;

  // device_sync::DeviceSyncImpl::Factory:
  std::unique_ptr<DeviceSyncBase> CreateInstance(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* profile_prefs,
      const GcmDeviceInfoProvider* gcm_device_info_provider,
      ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<base::OneShotTimer> timer,
      AttestationCertificatesSyncer::GetAttestationCertificatesFunction
          get_attestation_certificates_function) override {
    return std::make_unique<StubDeviceSync>();
  }
};

}  // namespace

void SetStubDeviceSyncFactory() {
  static base::NoDestructor<StubDeviceSyncImplFactory> factory;
  DeviceSyncImpl::Factory::SetCustomFactory(factory.get());
}

}  // namespace device_sync

}  // namespace ash
