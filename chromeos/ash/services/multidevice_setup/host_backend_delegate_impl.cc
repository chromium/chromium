// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_backend_delegate_impl.h"

#include <sstream>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::multidevice_setup {

namespace {

// Name of the pref which stores the ID of the host which is pending being set
// on the back-end.
const char kPendingRequestHostIdPrefName[] =
    "multidevice_setup.pending_request_host_id";

// String to use for the pending host ID preference entry when the pending
// request is to remove the current host.
const char kPendingRemovalOfCurrentHost[] = "pendingRemovalOfCurrentHost";

// String to use for the pending host ID when there is no pending request.
const char kNoPendingRequest[] = "";

// The number of minutes to wait before retrying a failed attempt.
const int kNumMinutesBetweenRetries = 5;

const char kNoHostForLogging[] = "[no host]";

}  // namespace

// static
HostBackendDelegateImpl::Factory*
    HostBackendDelegateImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<HostBackendDelegate> HostBackendDelegateImpl::Factory::Create(
    EligibleHostDevicesProvider* eligible_host_devices_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(eligible_host_devices_provider,
                                         pref_service, device_sync_client,
                                         std::move(timer));
  }

  return base::WrapUnique(
      new HostBackendDelegateImpl(eligible_host_devices_provider, pref_service,
                                  device_sync_client, std::move(timer)));
}

// static
void HostBackendDelegateImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

HostBackendDelegateImpl::Factory::~Factory() = default;

// static
void HostBackendDelegateImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kPendingRequestHostIdPrefName,
                               kNoPendingRequest);
}

HostBackendDelegateImpl::HostBackendDelegateImpl(
    EligibleHostDevicesProvider* eligible_host_devices_provider,
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    std::unique_ptr<base::OneShotTimer> timer)
    : HostBackendDelegate(),
      eligible_host_devices_provider_(eligible_host_devices_provider),
      pref_service_(pref_service),
      device_sync_client_(device_sync_client),
      timer_(std::move(timer)) {
  device_sync_client_->AddObserver(this);

  host_from_last_sync_ = GetHostFromDeviceSync();

  if (HasPendingHostRequest())
    AttemptNetworkRequest(false /* is_retry */);
}

HostBackendDelegateImpl::~HostBackendDelegateImpl() {
  device_sync_client_->RemoveObserver(this);
}

void HostBackendDelegateImpl::AttemptToSetMultiDeviceHostOnBackend(
    const std::optional<multidevice::RemoteDeviceRef>& host_device) {
  if (host_device && !IsHostEligible(*host_device)) {
    PA_LOG(WARNING) << "HostBackendDelegateImpl::"
                    << "AttemptToSetMultiDeviceHostOnBackend(): Tried to set a "
                    << "device as host, but that device is not an eligible "
                    << "host: " << host_device->GetInstanceIdDeviceIdForLogs();
    return;
  }

  // If the device on the back-end is already |host_device|, there no longer
  // needs to be a pending request.
  if (host_from_last_sync_ == host_device) {
    SetPendingHostRequest(kNoPendingRequest);
    return;
  }

  // Stop the timer, since a new attempt is being started.
  timer_->Stop();

  if (host_device) {
    if (features::ShouldUseV1DeviceSync()) {
      SetPendingHostRequest(host_device->GetDeviceId());
    } else {
      DCHECK(!host_device->instance_id().empty());
      SetPendingHostRequest(host_device->instance_id());
    }
  } else {
    SetPendingHostRequest(kPendingRemovalOfCurrentHost);
  }

  AttemptNetworkRequest(false /* is_retry */);
}

bool HostBackendDelegateImpl::HasPendingHostRequest() {
  const std::string pending_host_id_from_prefs =
      pref_service_->GetString(kPendingRequestHostIdPrefName);

  if (pending_host_id_from_prefs == kNoPendingRequest)
    return false;

  if (pending_host_id_from_prefs == kPendingRemovalOfCurrentHost) {
    // If the pending request is to remove the current host but there is no
    // current host, the host was removed by another device while this device
    // was offline.
    if (!host_from_last_sync_) {
      SetPendingHostRequest(kNoPendingRequest);
      return false;
    }

    // Otherwise, there still is a pending request to remove the current host.
    return true;
  }

  // By this point, |pending_host_id_from_prefs| refers to a real device ID and
  // not one of the two sentinel values.
  if (FindDeviceById(pending_host_id_from_prefs))
    return true;

  // If a request was pending for a specific host device, but that device is no
  // longer present on the user's account, there is no longer a pending request.
  // TODO(crbug.com/41443836): Track frequency of unrecognized host IDs.
  // If the following scenarios occur before the pending host request completes,
  // the persisted host ID will not be recognized, and the user will need to go
  // through setup again:
  //  * The device was actually removed from the user's account.
  //  * A public key is persisted, v1 DeviceSync is disabled, and the v2 device
  //    data hasn't been decrypted.
  //  * Instance ID is persisted, v1 DeviceSync in re-enabled and the Instance
  //    ID cannot be found in the device list.
  // We expect all of these scenarios to be very rare.
  SetPendingHostRequest(kNoPendingRequest);
  return false;
}

std::optional<multidevice::RemoteDeviceRef>
HostBackendDelegateImpl::GetPendingHostRequest() const {
  const std::string pending_host_id_from_prefs =
      pref_service_->GetString(kPendingRequestHostIdPrefName);

  DCHECK_NE(pending_host_id_from_prefs, kNoPendingRequest)
      << "HostBackendDelegateImpl::GetPendingHostRequest(): Tried "
      << "to get pending host request, but there was no pending "
      << "host request.";

  if (pending_host_id_from_prefs == kPendingRemovalOfCurrentHost)
    return std::nullopt;

  std::optional<multidevice::RemoteDeviceRef> pending_host =
      FindDeviceById(pending_host_id_from_prefs);
  DCHECK(pending_host)
      << "HostBackendDelegateImpl::GetPendingHostRequest(): Tried to get "
      << "pending host request, but the pending host ID was not present.";

  return pending_host;
}

std::optional<multidevice::RemoteDeviceRef>
HostBackendDelegateImpl::GetMultiDeviceHostFromBackend() const {
  return host_from_last_sync_;
}

bool HostBackendDelegateImpl::IsHostEligible(
    const multidevice::RemoteDeviceRef& provided_host) {
  return base::Contains(
      eligible_host_devices_provider_->GetEligibleHostDevices(), provided_host);
}

void HostBackendDelegateImpl::SetPendingHostRequest(
    const std::string& pending_host_id) {
  const std::string host_id_from_prefs_before_call =
      pref_service_->GetString(kPendingRequestHostIdPrefName);
  if (pending_host_id == host_id_from_prefs_before_call)
    return;

  pref_service_->SetString(kPendingRequestHostIdPrefName, pending_host_id);
  timer_->Stop();
  NotifyPendingHostRequestChange();
}

std::optional<multidevice::RemoteDeviceRef>
HostBackendDelegateImpl::FindDeviceById(const std::string& id) const {
  DCHECK(!id.empty());
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    if (features::ShouldUseV1DeviceSync()) {
      if (id == remote_device.GetDeviceId())
        return remote_device;
    } else {
      if (id == remote_device.instance_id())
        return remote_device;
    }
  }

  return std::nullopt;
}

void HostBackendDelegateImpl::AttemptNetworkRequest(bool is_retry) {
  DCHECK(HasPendingHostRequest())
      << "HostBackendDelegateImpl::AttemptNetworkRequest(): Tried to attempt a "
      << "network request, but there was no pending host request.";

  std::optional<multidevice::RemoteDeviceRef> pending_host_request =
      GetPendingHostRequest();

  // If |pending_host_request| is non-null, the request should be to set that
  // device. If it is null, the pending request is to remove the current host.
  multidevice::RemoteDeviceRef device_to_set =
      pending_host_request ? *pending_host_request : *host_from_last_sync_;

  // Likewise, if |pending_host_request| is non-null, that device should be
  // enabled, and if it is null, the old device should be disabled.
  bool should_enable = pending_host_request != std::nullopt;

  PA_LOG(INFO) << "HostBackendDelegateImpl::AttemptNetworkRequest(): "
               << (is_retry ? "Retrying attempt" : "Attempting") << " to "
               << (should_enable ? "enable" : "disable")
               << " the host: " << device_to_set.GetInstanceIdDeviceIdForLogs();

  if (features::ShouldUseV1DeviceSync()) {
    // Even if the |device_to_set| has a non-trivial Instance ID, we still
    // invoke the v1 DeviceSync RPC to set the feature state. This ensures that
    // GmsCore will be notified of the change regardless of what version of
    // DeviceSync it is running. The v1 and v2 RPCs to change feature states
    // ultimately update the same backend database entry. Note: The
    // RemoteDeviceProvider guarantees that every device will have a public key
    // while v1 DeviceSync is enabled.
    DCHECK(!device_to_set.public_key().empty());
    device_sync_client_->SetSoftwareFeatureState(
        device_to_set.public_key(),
        multidevice::SoftwareFeature::kBetterTogetherHost,
        should_enable /* enabled */, should_enable /* is_exclusive */,
        base::BindOnce(
            &HostBackendDelegateImpl::OnSetHostNetworkRequestFinished,
            weak_ptr_factory_.GetWeakPtr(), device_to_set, should_enable));
  } else {
    DCHECK(!device_to_set.instance_id().empty());
    device_sync_client_->SetFeatureStatus(
        device_to_set.instance_id(),
        multidevice::SoftwareFeature::kBetterTogetherHost,
        should_enable ? device_sync::FeatureStatusChange::kEnableExclusively
                      : device_sync::FeatureStatusChange::kDisable,
        base::BindOnce(
            &HostBackendDelegateImpl::OnSetHostNetworkRequestFinished,
            weak_ptr_factory_.GetWeakPtr(), device_to_set, should_enable));
  }
}

void HostBackendDelegateImpl::OnNewDevicesSynced() {
  std::optional<multidevice::RemoteDeviceRef> host_from_sync =
      GetHostFromDeviceSync();
  if (host_from_last_sync_ == host_from_sync)
    return;

  PA_LOG(INFO) << "HostBackendDelegateImpl::OnNewDevicesSynced(): New host "
               << "device has been set. Old host: "
               << (host_from_last_sync_
                       ? host_from_last_sync_->GetInstanceIdDeviceIdForLogs()
                       : kNoHostForLogging)
               << ", New host: "
               << (host_from_sync
                       ? host_from_sync->GetInstanceIdDeviceIdForLogs()
                       : kNoHostForLogging);

  host_from_last_sync_ = host_from_sync;

  // If there is a pending request and the new host fulfills that pending
  // request, there is no longer a pending request.
  if (HasPendingHostRequest() &&
      host_from_last_sync_ == GetPendingHostRequest()) {
    SetPendingHostRequest(kNoPendingRequest);
  }

  NotifyHostChangedOnBackend();
}

std::optional<multidevice::RemoteDeviceRef>
HostBackendDelegateImpl::GetHostFromDeviceSync() {
  multidevice::RemoteDeviceRefList synced_devices =
      device_sync_client_->GetSyncedDevices();
  auto it = base::ranges::find(
      synced_devices, multidevice::SoftwareFeatureState::kEnabled,
      [](const auto& remote_device) {
        return remote_device.GetSoftwareFeatureState(
            multidevice::SoftwareFeature::kBetterTogetherHost);
      });

  if (it == synced_devices.end())
    return std::nullopt;

  return *it;
}

void HostBackendDelegateImpl::OnSetHostNetworkRequestFinished(
    multidevice::RemoteDeviceRef device_for_request,
    bool attempted_to_enable,
    device_sync::mojom::NetworkRequestResult result_code) {
  bool success =
      result_code == device_sync::mojom::NetworkRequestResult::kSuccess;

  std::stringstream ss;
  ss << "HostBackendDelegateImpl::OnSetHostNetworkRequestFinished(): "
     << (success ? "Completed successful" : "Failure requesting") << " "
     << "host change for " << device_for_request.GetInstanceIdDeviceIdForLogs()
     << ". Attempted to enable: " << (attempted_to_enable ? "true" : "false");

  if (success) {
    PA_LOG(VERBOSE) << ss.str();
    return;
  }

  ss << ", Error code: " << result_code;
  PA_LOG(WARNING) << ss.str();

  if (!HasPendingHostRequest())
    return;

  std::optional<multidevice::RemoteDeviceRef> pending_host_request =
      GetPendingHostRequest();

  bool failed_request_was_to_set_pending_host =
      attempted_to_enable && pending_host_request &&
      *pending_host_request == device_for_request;

  bool failed_request_was_to_remove_pending_host =
      !attempted_to_enable && !pending_host_request &&
      device_for_request == host_from_last_sync_;

  // If the request which failed corresponds to the most recent call to
  // AttemptToSetMultiDeviceHostOnBackend(), alert observers that this request
  // failed and schedule a retry.
  if (failed_request_was_to_set_pending_host ||
      failed_request_was_to_remove_pending_host) {
    NotifyBackendRequestFailed();
    timer_->Start(
        FROM_HERE, base::Minutes(kNumMinutesBetweenRetries),
        base::BindOnce(&HostBackendDelegateImpl::AttemptNetworkRequest,
                       base::Unretained(this), true /* is_retry */));
  }
}

}  // namespace ash::multidevice_setup
