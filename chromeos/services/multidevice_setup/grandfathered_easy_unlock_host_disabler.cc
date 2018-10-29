// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace multidevice_setup {

namespace {

// Name of the pref that stores the ID of the device which still potentially
// needs to have EASY_UNLOCK_HOST disabled on it.
const char kEasyUnlockHostIdToDisablePrefName[] =
    "multidevice_setup.easy_unlock_host_id_to_disable";

const char kNoDevice[] = "";

// The number of minutes to wait before retrying a failed attempt.
const int kNumMinutesBetweenRetries = 5;

bool IsEasyUnlockHost(const cryptauth::RemoteDeviceRef& device) {
  return device.GetSoftwareFeatureState(
             cryptauth::SoftwareFeature::EASY_UNLOCK_HOST) ==
         cryptauth::SoftwareFeatureState::kEnabled;
}

}  // namespace

// static
GrandfatheredEasyUnlockHostDisabler::Factory*
    GrandfatheredEasyUnlockHostDisabler::Factory::test_factory_ = nullptr;

// static
GrandfatheredEasyUnlockHostDisabler::Factory*
GrandfatheredEasyUnlockHostDisabler::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void GrandfatheredEasyUnlockHostDisabler::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

GrandfatheredEasyUnlockHostDisabler::Factory::~Factory() = default;

std::unique_ptr<GrandfatheredEasyUnlockHostDisabler>
GrandfatheredEasyUnlockHostDisabler::Factory::BuildInstance(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new GrandfatheredEasyUnlockHostDisabler(
      host_backend_delegate, device_sync_client, pref_service,
      std::move(timer)));
}

// static
void GrandfatheredEasyUnlockHostDisabler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kEasyUnlockHostIdToDisablePrefName, kNoDevice);
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

  // There might be a device stored in the pref waiting for EASY_UNLOCK_HOST to
  // be disabled.
  DisableEasyUnlockHostIfNecessary();
}

void GrandfatheredEasyUnlockHostDisabler::OnHostChangedOnBackend() {
  // EASY_UNLOCK_HOST possibly needs to be disabled on the previous
  // BetterTogether host.
  SetPotentialEasyUnlockHostToDisable(current_better_together_host_);

  // Retrieve the new BetterTogether host.
  current_better_together_host_ =
      host_backend_delegate_->GetMultiDeviceHostFromBackend();

  DisableEasyUnlockHostIfNecessary();
}

void GrandfatheredEasyUnlockHostDisabler::DisableEasyUnlockHostIfNecessary() {
  timer_->Stop();

  base::Optional<cryptauth::RemoteDeviceRef> host_to_disable =
      GetEasyUnlockHostToDisable();

  if (!host_to_disable)
    return;

  PA_LOG(VERBOSE) << "Attempting to disable EASY_UNLOCK_HOST on device "
                  << host_to_disable->GetTruncatedDeviceIdForLogs();
  device_sync_client_->SetSoftwareFeatureState(
      host_to_disable->public_key(),
      cryptauth::SoftwareFeature::EASY_UNLOCK_HOST, false /* enabled */,
      false /* is_exclusive */,
      base::BindOnce(
          &GrandfatheredEasyUnlockHostDisabler::OnSetSoftwareFeatureStateResult,
          base::Unretained(this), *host_to_disable));
}

void GrandfatheredEasyUnlockHostDisabler::OnSetSoftwareFeatureStateResult(
    cryptauth::RemoteDeviceRef device,
    device_sync::mojom::NetworkRequestResult result_code) {
  bool success =
      result_code == device_sync::mojom::NetworkRequestResult::kSuccess;

  if (success) {
    PA_LOG(INFO) << "Successfully disabled EASY_UNLOCK_HOST on device "
                 << device.GetTruncatedDeviceIdForLogs();
  } else {
    PA_LOG(WARNING) << "Failed to disable EASY_UNLOCK_HOST on device "
                    << device.GetTruncatedDeviceIdForLogs()
                    << ", Error code: " << result_code;
  }

  // Return if the EasyUnlock host to disable changed between calls to
  // SetSoftwareFeatureState() and OnSetSoftwareFeatureStateResult().
  if (device != GetEasyUnlockHostToDisable())
    return;

  if (success) {
    SetPotentialEasyUnlockHostToDisable(base::nullopt);
    return;
  }

  PA_LOG(WARNING) << "Retrying in " << kNumMinutesBetweenRetries
                  << " minutes if necessary.";
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMinutes(kNumMinutesBetweenRetries),
                base::BindOnce(&GrandfatheredEasyUnlockHostDisabler::
                                   DisableEasyUnlockHostIfNecessary,
                               base::Unretained(this)));
}

void GrandfatheredEasyUnlockHostDisabler::SetPotentialEasyUnlockHostToDisable(
    base::Optional<cryptauth::RemoteDeviceRef> device) {
  pref_service_->SetString(kEasyUnlockHostIdToDisablePrefName,
                           device ? device->GetDeviceId() : kNoDevice);
}

base::Optional<cryptauth::RemoteDeviceRef>
GrandfatheredEasyUnlockHostDisabler::GetEasyUnlockHostToDisable() {
  std::string device_id =
      pref_service_->GetString(kEasyUnlockHostIdToDisablePrefName);

  if (device_id == kNoDevice)
    return base::nullopt;

  cryptauth::RemoteDeviceRefList synced_devices =
      device_sync_client_->GetSyncedDevices();
  auto it = std::find_if(synced_devices.begin(), synced_devices.end(),
                         [&device_id](const auto& remote_device) {
                           return remote_device.GetDeviceId() == device_id;
                         });

  // The device does not need to have EASY_UNLOCK_HOST disabled if any of the
  // following are true:
  //   - the device is not in the list of synced devices anymore,
  //   - the device is not the current EasyUnlock host, or
  //   - the device is the BetterTogether host.
  if (it == synced_devices.end() || !IsEasyUnlockHost(*it) ||
      *it == current_better_together_host_) {
    SetPotentialEasyUnlockHostToDisable(base::nullopt);
    return base::nullopt;
  }

  return *it;
}

}  // namespace multidevice_setup

}  // namespace chromeos
