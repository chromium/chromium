// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/discovered_devices_provider.h"
#include "chromeos/ash/services/bluetooth_config/discovery_session_manager.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_pairing_handler.h"

namespace ash::bluetooth_config {

class FakeDiscoverySessionManager : public DiscoverySessionManager {
 public:
  FakeDiscoverySessionManager(
      AdapterStateController* adapter_state_controller,
      DiscoveredDevicesProvider* discovered_devices_provider);
  ~FakeDiscoverySessionManager() override;

  // Sets whether a discovery session is active and notifies delegates and
  // observers of the change.
  void SetIsDiscoverySessionActive(bool is_active);
  bool IsDiscoverySessionActive() const override;

  using DiscoverySessionManager::HasAtLeastOneDiscoveryClient;

  std::vector<raw_ptr<FakeDevicePairingHandler, VectorExperimental>>&
  device_pairing_handlers() {
    return device_pairing_handlers_;
  }

 private:
  // DiscoverySessionManager:
  void OnHasAtLeastOneDiscoveryClientChanged() override;
  std::unique_ptr<DevicePairingHandler> CreateDevicePairingHandler(
      AdapterStateController* adapter_state_controller,
      mojo::PendingReceiver<mojom::DevicePairingHandler> receiver) override;

  bool is_discovery_session_active_ = false;

  std::vector<raw_ptr<FakeDevicePairingHandler, VectorExperimental>>
      device_pairing_handlers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_MANAGER_H_
