// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/discovery_session_manager.h"

#include "base/functional/bind.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::bluetooth_config {

DiscoverySessionManager::DiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    DiscoveredDevicesProvider* discovered_devices_provider)
    : adapter_state_controller_(adapter_state_controller),
      discovered_devices_provider_(discovered_devices_provider) {
  adapter_state_controller_observation_.Observe(
      adapter_state_controller_.get());
  discovered_devices_provider_observation_.Observe(
      discovered_devices_provider_.get());
  delegates_.set_disconnect_handler(
      base::BindRepeating(&DiscoverySessionManager::OnDelegateDisconnected,
                          base::Unretained(this)));
}

DiscoverySessionManager::~DiscoverySessionManager() = default;

void DiscoverySessionManager::StartDiscovery(
    mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate) {
  // If Bluetooth is not enabled, we cannot start discovery.
  if (!IsBluetoothEnabled()) {
    BLUETOOTH_LOG(ERROR)
        << "StartDiscovery() called while Bluetooth is not enabled";
    mojo::Remote<mojom::BluetoothDiscoveryDelegate> delegate_remote(
        std::move(delegate));
    delegate_remote->OnBluetoothDiscoveryStopped();
    return;
  }

  const bool had_client_before_call = HasAtLeastOneDiscoveryClient();

  mojo::RemoteSetElementId id = delegates_.Add(std::move(delegate));

  // The number of clients has increased from 0 to 1.
  if (!had_client_before_call) {
    BLUETOOTH_LOG(EVENT) << "StartDiscovery() called as the first client";
    OnHasAtLeastOneDiscoveryClientChanged();
    NotifyHasAtLeastOneDiscoverySessionChanged(
        /*has_at_least_one_discovery_session=*/true);
    return;
  }

  // If discovery is already active, notify the delegate that discovery has
  // started and of the current discovered devices list.
  if (IsDiscoverySessionActive()) {
    BLUETOOTH_LOG(EVENT)
        << "StartDiscovery() called with a discovery session already active";
    delegates_.Get(id)->OnBluetoothDiscoveryStarted(
        RegisterNewDevicePairingHandler(id));
    delegates_.Get(id)->OnDiscoveredDevicesListChanged(
        discovered_devices_provider_->GetDiscoveredDevices());
  }
}

void DiscoverySessionManager::NotifyDiscoveryStarted() {
  for (auto it = delegates_.begin(); it != delegates_.end(); ++it) {
    (*it)->OnBluetoothDiscoveryStarted(
        RegisterNewDevicePairingHandler(it.id()));
  }
}

void DiscoverySessionManager::NotifyDiscoveryStoppedAndClearActiveClients() {
  BLUETOOTH_LOG(EVENT) << "Notifying discovery stopped";

  if (!HasAtLeastOneDiscoveryClient())
    return;

  for (auto& delegate : delegates_)
    delegate->OnBluetoothDiscoveryStopped();

  // Since discovery has stopped, disconnect all delegates and handlers since
  // they are no longer actionable.
  delegates_.Clear();
  id_to_pairing_handler_map_.clear();

  // The number of clients has decreased from >0 to 0.
  OnHasAtLeastOneDiscoveryClientChanged();
  NotifyHasAtLeastOneDiscoverySessionChanged(
      /*has_at_least_one_discovery_session=*/false);
}

bool DiscoverySessionManager::HasAtLeastOneDiscoveryClient() const {
  return !delegates_.empty();
}

void DiscoverySessionManager::NotifyDiscoveredDevicesListChanged() {
  for (auto& delegate : delegates_) {
    delegate->OnDiscoveredDevicesListChanged(
        discovered_devices_provider_->GetDiscoveredDevices());
  }
}

void DiscoverySessionManager::OnAdapterStateChanged() {
  // We only need to handle the case where we have at least one client and
  // Bluetooth is no longer enabled.
  if (!HasAtLeastOneDiscoveryClient() || IsBluetoothEnabled())
    return;

  NotifyDiscoveryStoppedAndClearActiveClients();
}

void DiscoverySessionManager::OnDiscoveredDevicesListChanged() {
  NotifyDiscoveredDevicesListChanged();
}

mojo::PendingRemote<mojom::DevicePairingHandler>
DiscoverySessionManager::RegisterNewDevicePairingHandler(
    mojo::RemoteSetElementId id) {
  mojo::PendingRemote<mojom::DevicePairingHandler> remote;
  id_to_pairing_handler_map_[id] = CreateDevicePairingHandler(
      adapter_state_controller_, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

bool DiscoverySessionManager::IsBluetoothEnabled() const {
  return adapter_state_controller_->GetAdapterState() ==
         mojom::BluetoothSystemState::kEnabled;
}

void DiscoverySessionManager::OnDelegateDisconnected(
    mojo::RemoteSetElementId id) {
  id_to_pairing_handler_map_.erase(id);

  // If the disconnected client was the last one, the number of clients has
  // decreased from 1 to 0.
  if (!HasAtLeastOneDiscoveryClient()) {
    BLUETOOTH_LOG(EVENT)
        << "The number of discovery clients has decreased from 1 to 0";
    OnHasAtLeastOneDiscoveryClientChanged();
    NotifyHasAtLeastOneDiscoverySessionChanged(
        /*has_at_least_one_discovery_session=*/false);
  }
}

void DiscoverySessionManager::FlushForTesting() {
  delegates_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::bluetooth_config
