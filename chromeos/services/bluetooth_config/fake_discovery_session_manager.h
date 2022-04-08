// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_MANAGER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_MANAGER_H_

#include "chromeos/services/bluetooth_config/discovered_devices_provider.h"
#include "chromeos/services/bluetooth_config/discovery_session_manager.h"
#include "chromeos/services/bluetooth_config/fake_device_pairing_handler.h"

namespace chromeos {
namespace bluetooth_config {

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

  std::vector<FakeDevicePairingHandler*>& device_pairing_handlers() {
    return device_pairing_handlers_;
  }

 private:
  // DiscoverySessionManager:
  void OnHasAtLeastOneDiscoveryClientChanged() override;
  std::unique_ptr<DevicePairingHandler> CreateDevicePairingHandler(
      AdapterStateController* adapter_state_controller,
      mojo::PendingReceiver<mojom::DevicePairingHandler> receiver) override;

  bool is_discovery_session_active_ = false;

  std::vector<FakeDevicePairingHandler*> device_pairing_handlers_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_MANAGER_H_
