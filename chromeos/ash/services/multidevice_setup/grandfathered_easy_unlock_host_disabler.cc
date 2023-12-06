// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace multidevice_setup {

namespace {

// Name of the prefs that stores the legacy device ID and Instance ID of the
// device which still potentially needs to have kSmartLockHost disabled on it.
const char kEasyUnlockHostIdToDisablePrefName[] =
    "multidevice_setup.easy_unlock_host_id_to_disable";
const char kEasyUnlockHostInstanceIdToDisablePrefName[] =
    "multidevice_setup.easy_unlock_host_instance_id_to_disable";

const char kNoDevice[] = "";

// The number of minutes to wait before retrying a failed attempt.
const int kNumMinutesBetweenRetries = 5;

bool IsEasyUnlockHost(const multidevice::RemoteDeviceRef& device) {
  return device.GetSoftwareFeatureState(
             multidevice::SoftwareFeature::kSmartLockHost) ==
         multidevice::SoftwareFeatureState::kEnabled;
}

}  // namespace

// static
GrandfatheredEasyUnlockHostDisabler::Factory*
    GrandfatheredEasyUnlockHostDisabler::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<GrandfatheredEasyUnlockHostDisabler>
GrandfatheredEasyUnlockHostDisabler::Factory::Create(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(host_backend_delegate,
                                         device_sync_client, pref_service,
                                         std::move(timer));
  }

  return base::WrapUnique(new GrandfatheredEasyUnlockHostDisabler(
      host_backend_delegate, device_sync_client, pref_service,
      std::move(timer)));
}

// static
void GrandfatheredEasyUnlockHostDisabler::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

GrandfatheredEasyUnlockHostDisabler::Factory::~Factory() = default;

// static
void GrandfatheredEasyUnlockHostDisabler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kEasyUnlockHostIdToDisablePrefName, kNoDevice);
  registry->RegisterStringPref(kEasyUnlockHostInstanceIdToDisablePrefName,
                               kNoDevice);
}

GrandfatheredEasyUnlockHostDisabler::~GrandfatheredEasyUnlockHostDisabler() {
  host_backend_delegate_->RemoveObserver(this);
}

GrandfatheredEasyUnlockHostDisabler::GrandfatheredEasyUnlockHostDisabler(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer)
    : host_backend_delegate_(host_backend_delegate),
      device_sync_client_(device_sync_client),
      pref_service_(pref_service),
      timer_(std::move(timer)),
      current_better_together_host_(
          host_backend_delegate_->GetMultiDeviceHostFromBackend()) {
  host_backend_delegate_->AddObserver(this);

  // There might be a device stored in the pref waiting for kSmartLockHost to
  // be disabled.
  DisableEasyUnlockHostIfNecessary();
}

void GrandfatheredEasyUnlockHostDisabler::OnHostChangedOnBackend() {
  // kSmartLockHost possibly needs to be disabled on the previous
  // BetterTogether host.
  SetPotentialEasyUnlockHostToDisable(current_better_together_host_);

  // Retrieve the new BetterTogether host.
  current_better_together_host_ =
      host_backend_delegate_->GetMultiDeviceHostFromBackend();

  DisableEasyUnlockHostIfNecessary();
}

void GrandfatheredEasyUnlockHostDisabler::DisableEasyUnlockHostIfNecessary() {
  timer_->Stop();

  std::optional<multidevice::RemoteDeviceRef> host_to_disable =
      GetEasyUnlockHostToDisable();

  if (!host_to_disable)
    return;

  PA_LOG(VERBOSE) << "Attempting to disable kSmartLockHost on device "
                  << host_to_disable->GetInstanceIdDeviceIdForLogs();
  if (features::ShouldUseV1DeviceSync()) {
    // Even if the host has a non-trivial Instance ID, we still invoke the v1
    // DeviceSync RPC to set the feature state. This ensures that GmsCore will
    // be notified of the change regardless of what version of DeviceSync it is
    // running. The v1 and v2 RPCs to change feature states ultimately update
    // the same backend database entry. Note: The RemoteDeviceProvider
    // guarantees that every device will have a public key while v1 DeviceSync
    // is enabled.
    DCHECK(!host_to_disable->public_key().empty());
    device_sync_client_->SetSoftwareFeatureState(
        host_to_disable->public_key(),
        multidevice::SoftwareFeature::kSmartLockHost, false /* enabled */,
        false /* is_exclusive */,
        base::BindOnce(
            &GrandfatheredEasyUnlockHostDisabler::OnDisableEasyUnlockHostResult,
            weak_ptr_factory_.GetWeakPtr(), *host_to_disable));
  } else {
    DCHECK(!host_to_disable->instance_id().empty());
    device_sync_client_->SetFeatureStatus(
        host_to_disable->instance_id(),
        multidevice::SoftwareFeature::kSmartLockHost,
        device_sync::FeatureStatusChange::kDisable,
        base::BindOnce(
            &GrandfatheredEasyUnlockHostDisabler::OnDisableEasyUnlockHostResult,
            weak_ptr_factory_.GetWeakPtr(), *host_to_disable));
  }
}

void GrandfatheredEasyUnlockHostDisabler::OnDisableEasyUnlockHostResult(
    multidevice::RemoteDeviceRef device,
    device_sync::mojom::NetworkRequestResult result_code) {
  bool success =
      result_code == device_sync::mojom::NetworkRequestResult::kSuccess;

  if (success) {
    PA_LOG(VERBOSE) << "Successfully disabled kSmartLockHost on device "
                    << device.GetInstanceIdDeviceIdForLogs();
  } else {
    PA_LOG(WARNING) << "Failed to disable kSmartLockHost on device "
                    << device.GetInstanceIdDeviceIdForLogs()
                    << ", Error code: " << result_code;
  }

  // Return if the EasyUnlock host to disable changed between calls to
  // DisableEasyUnlockHostIfNecessary() and OnDisableEasyUnlockHostResult().
  if (device != GetEasyUnlockHostToDisable())
    return;

  if (success) {
    SetPotentialEasyUnlockHostToDisable(std::nullopt);
    return;
  }

  PA_LOG(WARNING) << "Retrying in " << kNumMinutesBetweenRetries
                  << " minutes if necessary.";
  timer_->Start(FROM_HERE, base::Minutes(kNumMinutesBetweenRetries),
                base::BindOnce(&GrandfatheredEasyUnlockHostDisabler::
                                   DisableEasyUnlockHostIfNecessary,
                               base::Unretained(this)));
}

void GrandfatheredEasyUnlockHostDisabler::SetPotentialEasyUnlockHostToDisable(
    std::optional<multidevice::RemoteDeviceRef> device) {
  pref_service_->SetString(kEasyUnlockHostIdToDisablePrefName,
                           !device || device->GetDeviceId().empty()
                               ? kNoDevice
                               : device->GetDeviceId());
  pref_service_->SetString(kEasyUnlockHostInstanceIdToDisablePrefName,
                           !device || device->instance_id().empty()
                               ? kNoDevice
                               : device->instance_id());
}

std::optional<multidevice::RemoteDeviceRef>
GrandfatheredEasyUnlockHostDisabler::GetEasyUnlockHostToDisable() {
  std::string legacy_device_id =
      pref_service_->GetString(kEasyUnlockHostIdToDisablePrefName);
  std::string instance_id =
      pref_service_->GetString(kEasyUnlockHostInstanceIdToDisablePrefName);
  if (legacy_device_id == kNoDevice && instance_id == kNoDevice)
    return std::nullopt;

  multidevice::RemoteDeviceRefList synced_devices =
      device_sync_client_->GetSyncedDevices();
  auto it = base::ranges::find_if(
      synced_devices,
      [&legacy_device_id, &instance_id](const auto& remote_device) {
        return (legacy_device_id != kNoDevice &&
                remote_device.GetDeviceId() == legacy_device_id) ||
               (instance_id != kNoDevice &&
                remote_device.instance_id() == instance_id);
      });

  // The device does not need to have kSmartLockHost disabled if any of the
  // following are true:
  //   - the device is not in the list of synced devices anymore,
  //   - the device is not the current EasyUnlock host, or
  //   - the device is the BetterTogether host.
  if (it == synced_devices.end() || !IsEasyUnlockHost(*it) ||
      *it == current_better_together_host_) {
    SetPotentialEasyUnlockHostToDisable(std::nullopt);
    return std::nullopt;
  }

  return *it;
}

}  // namespace multidevice_setup

}  // namespace ash
