// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_discovery_session_manager.h"

namespace ash::bluetooth_config {

FakeDiscoverySessionManager::FakeDiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    DiscoveredDevicesProvider* discovered_devices_provider)
    : DiscoverySessionManager(adapter_state_controller,
                              discovered_devices_provider) {}

FakeDiscoverySessionManager::~FakeDiscoverySessionManager() = default;

void FakeDiscoverySessionManager::SetIsDiscoverySessionActive(bool is_active) {
  if (is_discovery_session_active_ == is_active)
    return;

  is_discovery_session_active_ = is_active;

  if (is_discovery_session_active_) {
    NotifyDiscoveryStarted();
    NotifyHasAtLeastOneDiscoverySessionChanged(is_discovery_session_active_);
    NotifyDiscoveredDevicesListChanged();
  } else {
    NotifyDiscoveryStoppedAndClearActiveClients();
    // DiscoverySessionStatusObservers would have been notified by the above
    // call but this fake manager always early returns before notifying
    // observers. Explicitly notify observers.
    NotifyHasAtLeastOneDiscoverySessionChanged(is_discovery_session_active_);
  }
}

void FakeDiscoverySessionManager::OnHasAtLeastOneDiscoveryClientChanged() {
  SetIsDiscoverySessionActive(HasAtLeastOneDiscoveryClient());
}

bool FakeDiscoverySessionManager::IsDiscoverySessionActive() const {
  return is_discovery_session_active_;
}

std::unique_ptr<DevicePairingHandler>
FakeDiscoverySessionManager::CreateDevicePairingHandler(
    AdapterStateController* adapter_state_controller,
    mojo::PendingReceiver<mojom::DevicePairingHandler> receiver) {
  auto fake_device_pairing_handler = std::make_unique<FakeDevicePairingHandler>(
      std::move(receiver), adapter_state_controller);
  device_pairing_handlers_.push_back(fake_device_pairing_handler.get());
  return fake_device_pairing_handler;
}

}  // namespace ash::bluetooth_config
