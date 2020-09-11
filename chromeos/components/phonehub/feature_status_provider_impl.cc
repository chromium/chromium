// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/feature_status_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {
namespace phonehub {
namespace {

using multidevice::RemoteDeviceRef;
using multidevice::RemoteDeviceRefList;
using multidevice::SoftwareFeature;
using multidevice::SoftwareFeatureState;

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

bool IsEligibleForFeature(
    const base::Optional<multidevice::RemoteDeviceRef>& local_device,
    const RemoteDeviceRefList& remote_devices,
    FeatureState feature_state) {
  // If the feature is prohibited by policy, we don't initialize Phone Hub
  // classes at all. But, there is an edge case where a user session starts up
  // normally, then an administrator prohibits the policy during the user
  // session. If this occurs, we consider the session ineligible for using Phone
  // Hub.
  if (feature_state == FeatureState::kProhibitedByPolicy)
    return false;

  // If the local device has not yet been enrolled, no phone can serve as its
  // Phone Hub host.
  if (!local_device)
    return false;

  // If the local device does not support being a Phone Hub client, no phone can
  // serve as its host.
  if (local_device->GetSoftwareFeatureState(SoftwareFeature::kPhoneHubClient) ==
      SoftwareFeatureState::kNotSupported) {
    return false;
  }

  // If the local device does not have an enrolled Bluetooth address, no phone
  // can serve as its host.
  if (local_device->bluetooth_public_address().empty())
    return false;

  for (const RemoteDeviceRef& device : remote_devices) {
    // Device must be capable of being a multi-device host.
    if (device.GetSoftwareFeatureState(SoftwareFeature::kBetterTogetherHost) ==
        SoftwareFeatureState::kNotSupported) {
      continue;
    }

    // Device must be capable of being a Phone Hub host.
    if (device.GetSoftwareFeatureState(SoftwareFeature::kPhoneHubHost) ==
        SoftwareFeatureState::kNotSupported) {
      continue;
    }

    // Device must have a synced Bluetooth public address, which is used to
    // bootstrap Phone Hub connections.
    if (device.bluetooth_public_address().empty())
      continue;

    return true;
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
  if (host_status == HostStatus::kHostSetButNotYetVerified)
    return true;

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
    ConnectionManager* connection_manager)
    : device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client),
      connection_manager_(connection_manager) {
  device_sync_client_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
  connection_manager_->AddObserver(this);

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&FeatureStatusProviderImpl::OnBluetoothAdapterReceived,
                     weak_ptr_factory_.GetWeakPtr()));

  status_ = ComputeStatus();
}

FeatureStatusProviderImpl::~FeatureStatusProviderImpl() {
  device_sync_client_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

FeatureStatus FeatureStatusProviderImpl::GetStatus() const {
  return *status_;
}

void FeatureStatusProviderImpl::OnReady() {
  UpdateStatus();
}

void FeatureStatusProviderImpl::OnNewDevicesSynced() {
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
  if (status_.has_value())
    UpdateStatus();
}

void FeatureStatusProviderImpl::OnConnectionStatusChanged() {
  UpdateStatus();
}

void FeatureStatusProviderImpl::UpdateStatus() {
  DCHECK(status_.has_value());

  FeatureStatus computed_status = ComputeStatus();
  if (computed_status == *status_)
    return;

  PA_LOG(INFO) << "Phone Hub feature status: " << *status_ << " => "
               << computed_status;
  *status_ = computed_status;
  NotifyStatusChanged();
}

FeatureStatus FeatureStatusProviderImpl::ComputeStatus() {
  FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub);

  // Note: If |device_sync_client_| is not yet ready, it has not initialized
  // itself with device metadata, so we assume that we are ineligible for the
  // feature until proven otherwise.
  if (!device_sync_client_->is_ready() ||
      !IsEligibleForFeature(device_sync_client_->GetLocalDeviceMetadata(),
                            device_sync_client_->GetSyncedDevices(),
                            feature_state)) {
    return FeatureStatus::kNotEligibleForFeature;
  }

  HostStatus host_status = multidevice_setup_client_->GetHostStatus().first;

  if (host_status == HostStatus::kEligibleHostExistsButNoHostSet)
    return FeatureStatus::kEligiblePhoneButNotSetUp;

  if (IsPhonePendingSetup(host_status, feature_state))
    return FeatureStatus::kPhoneSelectedAndPendingSetup;

  if (IsFeatureDisabledByUser(feature_state))
    return FeatureStatus::kDisabled;

  if (!IsBluetoothOn())
    return FeatureStatus::kUnavailableBluetoothOff;

  switch (connection_manager_->GetStatus()) {
    case ConnectionManager::Status::kDisconnected:
      return FeatureStatus::kEnabledButDisconnected;
    case ConnectionManager::Status::kConnecting:
      return FeatureStatus::kEnabledAndConnecting;
    case ConnectionManager::Status::kConnected:
      return FeatureStatus::kEnabledAndConnected;
  }

  return FeatureStatus::kEnabledButDisconnected;
}

bool FeatureStatusProviderImpl::IsBluetoothOn() const {
  if (!bluetooth_adapter_)
    return false;

  return bluetooth_adapter_->IsPresent() && bluetooth_adapter_->IsPowered();
}

}  // namespace phonehub
}  // namespace chromeos
