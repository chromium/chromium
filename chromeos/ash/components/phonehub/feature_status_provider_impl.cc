// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/feature_status_provider_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::phonehub {

namespace {

using multidevice::RemoteDeviceRef;
using multidevice::RemoteDeviceRefList;
using multidevice::SoftwareFeature;
using multidevice::SoftwareFeatureState;
using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

bool IsEligiblePhoneHubHost(const RemoteDeviceRef& device) {
  // Device must be capable of being a multi-device host.
  if (device.GetSoftwareFeatureState(SoftwareFeature::kBetterTogetherHost) ==
      SoftwareFeatureState::kNotSupported) {
    return false;
  }

  if (device.GetSoftwareFeatureState(SoftwareFeature::kPhoneHubHost) ==
      SoftwareFeatureState::kNotSupported) {
    return false;
  }

  // Device must have a synced Bluetooth public address, which is used to
  // bootstrap Phone Hub connections.
  return !device.bluetooth_public_address().empty();
}

bool IsEligibleForFeature(
    const std::optional<multidevice::RemoteDeviceRef>& local_device,
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice host_status,
    const RemoteDeviceRefList& remote_devices,
    FeatureState feature_state) {
  // If the feature is prohibited by policy, we don't initialize Phone Hub
  // classes at all. But, there is an edge case where a user session starts up
  // normally, then an administrator prohibits the policy during the user
  // session. If this occurs, we consider the session ineligible for using Phone
  // Hub.
  if (feature_state == FeatureState::kProhibitedByPolicy) {
    return false;
  }

  if (feature_state == FeatureState::kNotSupportedByChromebook) {
    return false;
  }

  // If the local device has not yet been enrolled, no phone can serve as its
  // Phone Hub host.
  if (!local_device) {
    return false;
  }

  // If the local device does not support being a Phone Hub client, no phone can
  // serve as its host.
  if (local_device->GetSoftwareFeatureState(SoftwareFeature::kPhoneHubClient) ==
      SoftwareFeatureState::kNotSupported) {
    return false;
  }

  // If the local device does not have an enrolled Bluetooth address, no phone
  // can serve as its host.
  if (local_device->bluetooth_public_address().empty()) {
    return false;
  }

  // If the host device is not an eligible host, do not initialize Phone Hub.
  if (host_status.first == HostStatus::kNoEligibleHosts) {
    return false;
  }

  // If there is a host device available, check if the device is eligible for
  // Phonehub.
  if (host_status.second.has_value()) {
    return IsEligiblePhoneHubHost(*(host_status.second));
  }

  // Otherwise, check if there is any available remote device that is
  // eligible for Phonehub.
  for (const RemoteDeviceRef& device : remote_devices) {
    if (IsEligiblePhoneHubHost(device)) {
      return true;
    }
  }

  // If none of the devices return true above, there are no phones capable of
  // Phone Hub connections on the account.
  return false;
}

bool IsPhonePendingSetup(HostStatus host_status, FeatureState feature_state) {
  // The user has completed the opt-in flow, but we have not yet notified the
  // back-end of this selection. One common cause of this state is when the user
  // completes setup while offline.
  if (host_status ==
      HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    return true;
  }

  // The device has been set up with the back-end, but the phone has not yet
  // enabled itself.
  if (host_status == HostStatus::kHostSetButNotYetVerified) {
    return true;
  }

  // The phone has enabled itself for the multi-device suite but has not yet
  // enabled itself for Phone Hub. Note that kNotSupportedByPhone is a bit of a
  // misnomer here; this value means that the phone has advertised support for
  // the feature but has not yet enabled it.
  return host_status == HostStatus::kHostVerified &&
         feature_state == FeatureState::kNotSupportedByPhone;
}

bool IsFeatureDisabledByUser(FeatureState feature_state) {
  return feature_state == FeatureState::kDisabledByUser ||
         feature_state == FeatureState::kUnavailableSuiteDisabled ||
         feature_state == FeatureState::kUnavailableTopLevelFeatureDisabled;
}

}  // namespace

FeatureStatusProviderImpl::FeatureStatusProviderImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::ConnectionManager* connection_manager,
    session_manager::SessionManager* session_manager,
    chromeos::PowerManagerClient* power_manager_client,
    PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger)
    : device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client),
      connection_manager_(connection_manager),
      session_manager_(session_manager),
      power_manager_client_(power_manager_client),
      phone_hub_structured_metrics_logger_(
          phone_hub_structured_metrics_logger) {
  DCHECK(session_manager_);
  DCHECK(power_manager_client_);
  device_sync_client_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
  connection_manager_->AddObserver(this);
  session_manager_->AddObserver(this);
  power_manager_client_->AddObserver(this);

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&FeatureStatusProviderImpl::OnBluetoothAdapterReceived,
                     weak_ptr_factory_.GetWeakPtr()));

  status_ = ComputeStatus();
}

FeatureStatusProviderImpl::~FeatureStatusProviderImpl() {
  device_sync_client_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
  if (bluetooth_adapter_) {
    bluetooth_adapter_->RemoveObserver(this);
  }
  session_manager_->RemoveObserver(this);
  power_manager_client_->RemoveObserver(this);
}

FeatureStatus FeatureStatusProviderImpl::GetStatus() const {
  PA_LOG(VERBOSE) << __func__ << ": status = " << *status_;
  return *status_;
}

void FeatureStatusProviderImpl::OnReady() {
  UpdateStatus();
}

void FeatureStatusProviderImpl::OnNewDevicesSynced() {
  if (features::IsPhoneHubOnboardingNotifierRevampEnabled() &&
      ComputeStatus() == FeatureStatus::kEligiblePhoneButNotSetUp) {
    CheckEligibleDevicesForNudge();
  }
  UpdateStatus();
}

void FeatureStatusProviderImpl::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  UpdateStatus();
}

void FeatureStatusProviderImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  UpdateStatus();
}

void FeatureStatusProviderImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  UpdateStatus();
}

void FeatureStatusProviderImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  UpdateStatus();
}

void FeatureStatusProviderImpl::OnBluetoothAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  bluetooth_adapter_ = std::move(bluetooth_adapter);
  bluetooth_adapter_->AddObserver(this);

  // If |status_| has not yet been set, this call occurred synchronously in the
  // constructor, so status_ has not yet been initialized.
  if (status_.has_value()) {
    UpdateStatus();
  }
}

void FeatureStatusProviderImpl::OnConnectionStatusChanged() {
  UpdateStatus();
}

void FeatureStatusProviderImpl::OnSessionStateChanged() {
  TRACE_EVENT0("login", "FeatureStatusProviderImpl::OnSessionStateChanged");
  UpdateStatus();
}

void FeatureStatusProviderImpl::UpdateStatus() {
  TRACE_EVENT0("ui", "FeatureStatusProviderImpl::UpdateStatus");
  DCHECK(status_.has_value());

  FeatureStatus computed_status = ComputeStatus();
  if (computed_status == *status_) {
    return;
  }

  PA_LOG(INFO) << "Phone Hub feature status: " << *status_ << " => "
               << computed_status;
  *status_ = computed_status;
  switch (status_.value()) {
    case FeatureStatus::kDisabled:
    case FeatureStatus::kLockOrSuspended:
      phone_hub_structured_metrics_logger_->ResetSessionId();
      break;
    case FeatureStatus::kEligiblePhoneButNotSetUp:
    case FeatureStatus::kNotEligibleForFeature:
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      phone_hub_structured_metrics_logger_->ResetCachedInformation();
      break;
    case FeatureStatus::kEnabledAndConnecting:
    case FeatureStatus::kEnabledAndConnected:
    case FeatureStatus::kUnavailableBluetoothOff:
    case FeatureStatus::kEnabledButDisconnected:
      break;
  }
  NotifyStatusChanged();

  UMA_HISTOGRAM_ENUMERATION("PhoneHub.Adoption.FeatureStatusChangesSinceLogin",
                            GetStatus());
}

FeatureStatus FeatureStatusProviderImpl::ComputeStatus() {
  FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub);

  HostStatus host_status = multidevice_setup_client_->GetHostStatus().first;

  // Note: If |device_sync_client_| is not yet ready, it has not initialized
  // itself with device metadata, so we assume that we are ineligible for the
  // feature until proven otherwise.
  if (!device_sync_client_->is_ready() ||
      !IsEligibleForFeature(device_sync_client_->GetLocalDeviceMetadata(),
                            multidevice_setup_client_->GetHostStatus(),
                            device_sync_client_->GetSyncedDevices(),
                            feature_state)) {
    return FeatureStatus::kNotEligibleForFeature;
  }

  if (session_manager_->IsScreenLocked() || is_suspended_) {
    return FeatureStatus::kLockOrSuspended;
  }

  if (host_status == HostStatus::kEligibleHostExistsButNoHostSet) {
    return FeatureStatus::kEligiblePhoneButNotSetUp;
  }

  if (IsPhonePendingSetup(host_status, feature_state)) {
    return FeatureStatus::kPhoneSelectedAndPendingSetup;
  }

  if (IsFeatureDisabledByUser(feature_state)) {
    return FeatureStatus::kDisabled;
  }

  if (!IsBluetoothOn()) {
    return FeatureStatus::kUnavailableBluetoothOff;
  }

  switch (connection_manager_->GetStatus()) {
    case secure_channel::ConnectionManager::Status::kDisconnected:
      return FeatureStatus::kEnabledButDisconnected;
    case secure_channel::ConnectionManager::Status::kConnecting:
      return FeatureStatus::kEnabledAndConnecting;
    case secure_channel::ConnectionManager::Status::kConnected:
      return FeatureStatus::kEnabledAndConnected;
  }

  return FeatureStatus::kEnabledButDisconnected;
}

bool FeatureStatusProviderImpl::IsBluetoothOn() const {
  if (!bluetooth_adapter_) {
    return false;
  }

  return bluetooth_adapter_->IsPresent() && bluetooth_adapter_->IsPowered();
}

void FeatureStatusProviderImpl::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  PA_LOG(INFO) << "Device is suspending";
  is_suspended_ = true;
  UpdateStatus();
}

void FeatureStatusProviderImpl::SuspendDone(base::TimeDelta sleep_duration) {
  PA_LOG(INFO) << "Device has stopped suspending";
  is_suspended_ = false;
  UpdateStatus();
}

void FeatureStatusProviderImpl::CheckEligibleDevicesForNudge() {
  RemoteDeviceRefList eligible_devices;
  for (const RemoteDeviceRef& device :
       device_sync_client_->GetSyncedDevices()) {
    if (IsEligiblePhoneHubHost(device)) {
      eligible_devices.push_back(device);
    }
  }
  NotifyEligibleDevicesFound(eligible_devices);
}

}  // namespace ash::phonehub
