// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/wifi_sync_feature_manager_impl.h"

#include <sstream>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/feature_status_change.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const char kPendingWifiSyncRequestEnabledPrefName[] =
    "multidevice_setup.pending_set_wifi_sync_enabled_request";

// The number of minutes to wait before retrying a failed attempt.
const int kNumMinutesBetweenRetries = 5;

}  // namespace

// static
WifiSyncFeatureManagerImpl::Factory*
    WifiSyncFeatureManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<WifiSyncFeatureManager>
WifiSyncFeatureManagerImpl::Factory::Create(
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(host_status_provider, pref_service,
                                         device_sync_client, std::move(timer));
  }

  return base::WrapUnique(
      new WifiSyncFeatureManagerImpl(host_status_provider, pref_service,
                                     device_sync_client, std::move(timer)));
}

// static
void WifiSyncFeatureManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

WifiSyncFeatureManagerImpl::Factory::~Factory() = default;

void WifiSyncFeatureManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kPendingWifiSyncRequestEnabledPrefName,
                                static_cast<int>(PendingState::kPendingNone));
}

WifiSyncFeatureManagerImpl::~WifiSyncFeatureManagerImpl() = default;

WifiSyncFeatureManagerImpl::WifiSyncFeatureManagerImpl(
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    std::unique_ptr<base::OneShotTimer> timer)
    : WifiSyncFeatureManager(),
      host_status_provider_(host_status_provider),
      pref_service_(pref_service),
      device_sync_client_(device_sync_client),
      timer_(std::move(timer)) {
  host_status_provider_->AddObserver(this);
  device_sync_client_->AddObserver(this);

  if (GetCurrentState() == CurrentState::kValidPendingRequest) {
    AttemptSetWifiSyncHostStateNetworkRequest(false /* is_retry */);
  }

  if (ShouldEnableOnVerify()) {
    ProcessEnableOnVerifyAttempt();
  }
}

void WifiSyncFeatureManagerImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  if (GetCurrentState() == CurrentState::kNoVerifiedHost &&
      !ShouldEnableOnVerify()) {
    ResetPendingWifiSyncHostNetworkRequest();
  }

  if (ShouldAttemptToEnableAfterHostVerified()) {
    SetPendingWifiSyncHostNetworkRequest(
        PendingState::kSetPendingEnableOnVerify);
    return;
  }

  if (ShouldEnableOnVerify()) {
    ProcessEnableOnVerifyAttempt();
  }
}

void WifiSyncFeatureManagerImpl::OnNewDevicesSynced() {
  if (GetCurrentState() != CurrentState::kValidPendingRequest &&
      !ShouldEnableOnVerify()) {
    ResetPendingWifiSyncHostNetworkRequest();
  }
}

void WifiSyncFeatureManagerImpl::SetIsWifiSyncEnabled(bool enabled) {
  if (GetCurrentState() == CurrentState::kNoVerifiedHost) {
    PA_LOG(ERROR)
        << "WifiSyncFeatureManagerImpl::SetIsWifiSyncEnabled:  Network request "
           "not attempted because there is No Verified Host";
    ResetPendingWifiSyncHostNetworkRequest();
    return;
  }

  SetPendingWifiSyncHostNetworkRequest(enabled ? PendingState::kPendingEnable
                                               : PendingState::kPendingDisable);

  // Stop timer since new attempt is started.
  timer_->Stop();
  AttemptSetWifiSyncHostStateNetworkRequest(false /* is_retry */);
}

bool WifiSyncFeatureManagerImpl::IsWifiSyncEnabled() {
  CurrentState current_state = GetCurrentState();
  if (current_state == CurrentState::kNoVerifiedHost) {
    return false;
  }

  if (current_state == CurrentState::kValidPendingRequest) {
    return GetPendingState() == PendingState::kPendingEnable;
  }

  return host_status_provider_->GetHostWithStatus()
             .host_device()
             ->GetSoftwareFeatureState(
                 multidevice::SoftwareFeature::kWifiSyncHost) ==
         multidevice::SoftwareFeatureState::kEnabled;
}

void WifiSyncFeatureManagerImpl::ResetPendingWifiSyncHostNetworkRequest() {
  SetPendingWifiSyncHostNetworkRequest(PendingState::kPendingNone);
  timer_->Stop();
}

WifiSyncFeatureManagerImpl::PendingState
WifiSyncFeatureManagerImpl::GetPendingState() {
  return static_cast<PendingState>(
      pref_service_->GetInteger(kPendingWifiSyncRequestEnabledPrefName));
}

WifiSyncFeatureManagerImpl::CurrentState
WifiSyncFeatureManagerImpl::GetCurrentState() {
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
           ->GetSoftwareFeatureState(
               multidevice::SoftwareFeature::kWifiSyncHost) ==
       multidevice::SoftwareFeatureState::kEnabled);
  bool pending_enabled = (pending_state == PendingState::kPendingEnable);

  if (pending_enabled == enabled_on_host) {
    return CurrentState::kPendingMatchesBackend;
  }

  return CurrentState::kValidPendingRequest;
}

void WifiSyncFeatureManagerImpl::SetPendingWifiSyncHostNetworkRequest(
    PendingState pending_state) {
  pref_service_->SetInteger(kPendingWifiSyncRequestEnabledPrefName,
                            static_cast<int>(pending_state));
}

void WifiSyncFeatureManagerImpl::AttemptSetWifiSyncHostStateNetworkRequest(
    bool is_retry) {
  if (network_request_in_flight_) {
    return;
  }

  bool pending_enabled = (GetPendingState() == PendingState::kPendingEnable);

  PA_LOG(INFO) << "WifiSyncFeatureManagerImpl::"
               << "AttemptSetWifiSyncHostStateNetworkRequest(): "
               << (is_retry ? "Retrying attempt" : "Attempting") << " to "
               << (pending_enabled ? "enable" : "disable") << " wifi sync.";

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
        host_device.public_key(), multidevice::SoftwareFeature::kWifiSyncHost,
        pending_enabled /* enabled */, pending_enabled /* is_exclusive */,
        base::BindOnce(&WifiSyncFeatureManagerImpl::
                           OnSetWifiSyncHostStateNetworkRequestFinished,
                       weak_ptr_factory_.GetWeakPtr(), pending_enabled));
  } else {
    device_sync_client_->SetFeatureStatus(
        host_device.instance_id(), multidevice::SoftwareFeature::kWifiSyncHost,
        pending_enabled ? device_sync::FeatureStatusChange::kEnableExclusively
                        : device_sync::FeatureStatusChange::kDisable,
        base::BindOnce(&WifiSyncFeatureManagerImpl::
                           OnSetWifiSyncHostStateNetworkRequestFinished,
                       weak_ptr_factory_.GetWeakPtr(), pending_enabled));
  }
}

void WifiSyncFeatureManagerImpl::OnSetWifiSyncHostStateNetworkRequestFinished(
    bool attempted_to_enable,
    device_sync::mojom::NetworkRequestResult result_code) {
  network_request_in_flight_ = false;

  bool success =
      (result_code == device_sync::mojom::NetworkRequestResult::kSuccess);

  std::stringstream ss;
  ss << "WifiSyncFeatureManagerImpl::"
     << "OnSetWifiSyncHostStateNetworkRequestFinished(): "
     << (success ? "Completed successful" : "Failure requesting") << " "
     << "set WIFI_SYNC_HOST "
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
      AttemptSetWifiSyncHostStateNetworkRequest(false /* is_retry */);
    }
    return;
  }

  ss << ", Error code: " << result_code;
  PA_LOG(WARNING) << ss.str();

  // If the network request failed and there is still a pending network request,
  // schedule a retry.
  if (GetCurrentState() == CurrentState::kValidPendingRequest) {
    timer_->Start(FROM_HERE,
                  base::TimeDelta::FromMinutes(kNumMinutesBetweenRetries),
                  base::BindOnce(&WifiSyncFeatureManagerImpl::
                                     AttemptSetWifiSyncHostStateNetworkRequest,
                                 base::Unretained(this), true /* is_retry */));
  }
}

bool WifiSyncFeatureManagerImpl::ShouldEnableOnVerify() {
  return (GetPendingState() == PendingState::kSetPendingEnableOnVerify);
}

void WifiSyncFeatureManagerImpl::ProcessEnableOnVerifyAttempt() {
  mojom::HostStatus host_status =
      host_status_provider_->GetHostWithStatus().host_status();

  // If host is not set.
  if (host_status == mojom::HostStatus::kNoEligibleHosts ||
      host_status == mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    ResetPendingWifiSyncHostNetworkRequest();
    return;
  }

  if (host_status != mojom::HostStatus::kHostVerified) {
    return;
  }

  if (IsWifiSyncEnabled()) {
    ResetPendingWifiSyncHostNetworkRequest();
    return;
  }

  SetIsWifiSyncEnabled(true);
}

bool WifiSyncFeatureManagerImpl::ShouldAttemptToEnableAfterHostVerified() {
  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  if (host_status_with_device.host_status() !=
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    return false;
  }

  // Check if enterprise policy prohibits Wifi Sync or if feature flag is
  // disabled.
  if (!IsFeatureAllowed(mojom::Feature::kWifiSync, pref_service_)) {
    return false;
  }

  // Check if wifi sync is supported by host device.
  if (host_status_with_device.host_device()->GetSoftwareFeatureState(
          multidevice::SoftwareFeature::kWifiSyncHost) ==
      multidevice::SoftwareFeatureState::kNotSupported) {
    return false;
  }

  return true;
}

}  // namespace multidevice_setup

}  // namespace chromeos
