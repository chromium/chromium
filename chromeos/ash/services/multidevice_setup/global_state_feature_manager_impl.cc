// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager_impl.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/multidevice_setup/wifi_sync_notification_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace multidevice_setup {

namespace {

// This pref name is left in a legacy format to maintain compatibility.
const char kWifiSyncPendingStatePrefName[] =
    "multidevice_setup.pending_set_wifi_sync_enabled_request";

// The number of minutes to wait before retrying a failed attempt.
const int kNumMinutesBetweenRetries = 5;

}  // namespace

// static
GlobalStateFeatureManagerImpl::Factory*
    GlobalStateFeatureManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<GlobalStateFeatureManager>
GlobalStateFeatureManagerImpl::Factory::Create(
    Option option,
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(option, host_status_provider,
                                         pref_service, device_sync_client,
                                         std::move(timer));
  }

  mojom::Feature managed_feature;
  multidevice::SoftwareFeature managed_host_feature;
  std::string pending_state_pref_name;
  switch (option) {
    case Option::kWifiSync:
      managed_feature = mojom::Feature::kWifiSync;
      managed_host_feature = multidevice::SoftwareFeature::kWifiSyncHost;
      pending_state_pref_name = kWifiSyncPendingStatePrefName;
      break;
  }
  return base::WrapUnique(new GlobalStateFeatureManagerImpl(
      managed_feature, managed_host_feature, pending_state_pref_name,
      host_status_provider, pref_service, device_sync_client,
      std::move(timer)));
}

// static
void GlobalStateFeatureManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

GlobalStateFeatureManagerImpl::Factory::~Factory() = default;

void GlobalStateFeatureManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kWifiSyncPendingStatePrefName,
                                static_cast<int>(PendingState::kPendingNone));
}

GlobalStateFeatureManagerImpl::GlobalStateFeatureManagerImpl(
    mojom::Feature managed_feature,
    multidevice::SoftwareFeature managed_host_feature,
    const std::string& pending_state_pref_name,
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    std::unique_ptr<base::OneShotTimer> timer)
    : GlobalStateFeatureManager(),
      managed_feature_(managed_feature),
      managed_host_feature_(managed_host_feature),
      pending_state_pref_name_(pending_state_pref_name),
      host_status_provider_(host_status_provider),
      pref_service_(pref_service),
      device_sync_client_(device_sync_client),
      timer_(std::move(timer)) {
  host_status_provider_->AddObserver(this);
  device_sync_client_->AddObserver(this);

  if (GetCurrentState() == CurrentState::kValidPendingRequest) {
    AttemptSetHostStateNetworkRequest(false /* is_retry */);
  }

  if (ShouldEnableOnVerify()) {
    ProcessEnableOnVerifyAttempt();
  }
}

GlobalStateFeatureManagerImpl::~GlobalStateFeatureManagerImpl() {
  host_status_provider_->RemoveObserver(this);
  device_sync_client_->RemoveObserver(this);
}

void GlobalStateFeatureManagerImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  if (GetCurrentState() == CurrentState::kNoVerifiedHost &&
      !ShouldEnableOnVerify()) {
    ResetPendingNetworkRequest();
  }

  if (ShouldAttemptToEnableAfterHostVerified()) {
    SetPendingState(PendingState::kSetPendingEnableOnVerify);
    return;
  }

  if (ShouldEnableOnVerify()) {
    ProcessEnableOnVerifyAttempt();
  }
}

void GlobalStateFeatureManagerImpl::OnNewDevicesSynced() {
  if (GetCurrentState() != CurrentState::kValidPendingRequest &&
      !ShouldEnableOnVerify()) {
    ResetPendingNetworkRequest();
  }
}

void GlobalStateFeatureManagerImpl::SetIsFeatureEnabled(bool enabled) {
  if (GetCurrentState() == CurrentState::kNoVerifiedHost) {
    PA_LOG(ERROR) << "GlobalStateFeatureManagerImpl::SetIsFeatureEnabled:  "
                     "Network request "
                     "not attempted because there is No Verified Host";
    ResetPendingNetworkRequest();
    return;
  }

  SetPendingState(enabled ? PendingState::kPendingEnable
                          : PendingState::kPendingDisable);

  if (managed_feature_ == mojom::Feature::kWifiSync)
    pref_service_->SetBoolean(kCanShowWifiSyncAnnouncementPrefName, false);

  // Stop timer since new attempt is started.
  timer_->Stop();
  AttemptSetHostStateNetworkRequest(false /* is_retry */);
}

bool GlobalStateFeatureManagerImpl::IsFeatureEnabled() {
  CurrentState current_state = GetCurrentState();
  if (current_state == CurrentState::kNoVerifiedHost) {
    return false;
  }

  if (current_state == CurrentState::kValidPendingRequest) {
    return GetPendingState() == PendingState::kPendingEnable;
  }

  return host_status_provider_->GetHostWithStatus()
             .host_device()
             ->GetSoftwareFeatureState(managed_host_feature_) ==
         multidevice::SoftwareFeatureState::kEnabled;
}

void GlobalStateFeatureManagerImpl::ResetPendingNetworkRequest() {
  SetPendingState(PendingState::kPendingNone);
  timer_->Stop();
}

void GlobalStateFeatureManagerImpl::SetPendingState(
    PendingState pending_state) {
  pref_service_->SetInteger(pending_state_pref_name_,
                            static_cast<int>(pending_state));
}

GlobalStateFeatureManagerImpl::PendingState
GlobalStateFeatureManagerImpl::GetPendingState() {
  return static_cast<PendingState>(
      pref_service_->GetInteger(pending_state_pref_name_));
}

GlobalStateFeatureManagerImpl::CurrentState
GlobalStateFeatureManagerImpl::GetCurrentState() {
  if (host_status_provider_->GetHostWithStatus().host_status() !=
      mojom::HostStatus::kHostVerified) {
    return CurrentState::kNoVerifiedHost;
  }

  PendingState pending_state = GetPendingState();

  // If the pending request is kSetPendingEnableOnVerify then there is no
  // actionable pending equest. The pending request will be changed from
  // kSetPendingEnableOnVerify when the host has been verified.
  if (pending_state == PendingState::kPendingNone ||
      pending_state == PendingState::kSetPendingEnableOnVerify) {
    return CurrentState::kNoPendingRequest;
  }

  bool enabled_on_host =
      (host_status_provider_->GetHostWithStatus()
           .host_device()
           ->GetSoftwareFeatureState(managed_host_feature_) ==
       multidevice::SoftwareFeatureState::kEnabled);
  bool pending_enabled = (pending_state == PendingState::kPendingEnable);

  if (pending_enabled == enabled_on_host) {
    return CurrentState::kPendingMatchesBackend;
  }

  return CurrentState::kValidPendingRequest;
}

void GlobalStateFeatureManagerImpl::AttemptSetHostStateNetworkRequest(
    bool is_retry) {
  if (network_request_in_flight_) {
    return;
  }

  bool pending_enabled = (GetPendingState() == PendingState::kPendingEnable);

  PA_LOG(INFO) << "GlobalStateFeatureManagerImpl::" << __func__ << ": "
               << (is_retry ? "Retrying attempt" : "Attempting") << " to "
               << (pending_enabled ? "enable" : "disable");

  network_request_in_flight_ = true;
  multidevice::RemoteDeviceRef host_device =
      *host_status_provider_->GetHostWithStatus().host_device();

  if (features::ShouldUseV1DeviceSync()) {
    // Even if the |device_to_set| has a non-trivial Instance ID, we still
    // invoke the v1 DeviceSync RPC to set the feature state. This ensures that
    // GmsCore will be notified of the change regardless of what version of
    // DeviceSync it is running. The v1 and v2 RPCs to change feature states
    // ultimately update the same backend database entry. Note: The
    // RemoteDeviceProvider guarantees that every device will have a public key
    // while v1 DeviceSync is enabled.
    device_sync_client_->SetSoftwareFeatureState(
        host_device.public_key(), managed_host_feature_,
        pending_enabled /* enabled */, pending_enabled /* is_exclusive */,
        base::BindOnce(&GlobalStateFeatureManagerImpl::
                           OnSetHostStateNetworkRequestFinished,
                       weak_ptr_factory_.GetWeakPtr(), pending_enabled));
  } else {
    device_sync_client_->SetFeatureStatus(
        host_device.instance_id(), managed_host_feature_,
        pending_enabled ? device_sync::FeatureStatusChange::kEnableExclusively
                        : device_sync::FeatureStatusChange::kDisable,
        base::BindOnce(&GlobalStateFeatureManagerImpl::
                           OnSetHostStateNetworkRequestFinished,
                       weak_ptr_factory_.GetWeakPtr(), pending_enabled));
  }
}

void GlobalStateFeatureManagerImpl::OnSetHostStateNetworkRequestFinished(
    bool attempted_to_enable,
    device_sync::mojom::NetworkRequestResult result_code) {
  network_request_in_flight_ = false;

  bool success =
      (result_code == device_sync::mojom::NetworkRequestResult::kSuccess);

  std::stringstream ss;
  ss << "GlobalStateFeatureManagerImpl::" << __func__ << ": Completed with "
     << (success ? "success" : "failure")
     << ". Attempted to enable: " << (attempted_to_enable ? "true" : "false");

  if (success) {
    PA_LOG(VERBOSE) << ss.str();
    PendingState pending_state = GetPendingState();
    if (pending_state == PendingState::kPendingNone) {
      return;
    }

    bool pending_enabled = (pending_state == PendingState::kPendingEnable);
    // If the network request was successful but there is still a pending
    // network request then trigger a network request immediately. This could
    // happen if there was a second attempt to set the backend while the first
    // one was still in progress.
    if (attempted_to_enable != pending_enabled) {
      AttemptSetHostStateNetworkRequest(false /* is_retry */);
    }
    return;
  }

  ss << ", Error code: " << result_code;
  PA_LOG(WARNING) << ss.str();

  // If the network request failed and there is still a pending network request,
  // schedule a retry.
  if (GetCurrentState() == CurrentState::kValidPendingRequest) {
    timer_->Start(
        FROM_HERE, base::Minutes(kNumMinutesBetweenRetries),
        base::BindOnce(
            &GlobalStateFeatureManagerImpl::AttemptSetHostStateNetworkRequest,
            base::Unretained(this), true /* is_retry */));
  }
}

bool GlobalStateFeatureManagerImpl::ShouldEnableOnVerify() {
  return (GetPendingState() == PendingState::kSetPendingEnableOnVerify);
}

void GlobalStateFeatureManagerImpl::ProcessEnableOnVerifyAttempt() {
  mojom::HostStatus host_status =
      host_status_provider_->GetHostWithStatus().host_status();

  // If host is not set.
  if (host_status == mojom::HostStatus::kNoEligibleHosts ||
      host_status == mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    ResetPendingNetworkRequest();
    return;
  }

  if (host_status != mojom::HostStatus::kHostVerified) {
    return;
  }

  if (IsFeatureEnabled()) {
    ResetPendingNetworkRequest();
    return;
  }

  SetIsFeatureEnabled(true);

  if (managed_feature_ == mojom::Feature::kPhoneHubCameraRoll) {
    base::UmaHistogramEnumeration("PhoneHub.CameraRoll.OptInEntryPoint",
                                  mojom::CameraRollOptInEntryPoint::kSetupFlow);
  }
}

bool GlobalStateFeatureManagerImpl::ShouldAttemptToEnableAfterHostVerified() {
  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  if (host_status_with_device.host_status() !=
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    return false;
  }

  // Check if the feature is prohibited by enterprise policy or if feature flag
  // is disabled.
  if (!IsFeatureAllowed(managed_feature_, pref_service_)) {
    return false;
  }

  // Check if the feature is supported by host device.
  if (host_status_with_device.host_device()->GetSoftwareFeatureState(
          managed_host_feature_) ==
      multidevice::SoftwareFeatureState::kNotSupported) {
    return false;
  }

  return true;
}

}  // namespace multidevice_setup

}  // namespace ash
