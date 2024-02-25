// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_pairing_handler.h"
#include "chromeos/ash/services/bluetooth_config/discovered_devices_provider.h"
#include "chromeos/ash/services/bluetooth_config/discovery_session_manager.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDiscoverySession;
}  // namespace device

namespace ash::bluetooth_config {

// DiscoverySessionManager which uses BluetoothAdapter to start and stop
// discovery sessions.
class DiscoverySessionManagerImpl : public DiscoverySessionManager,
                                    public device::BluetoothAdapter::Observer {
 public:
  DiscoverySessionManagerImpl(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DiscoveredDevicesProvider* discovered_devices_provider,
      FastPairDelegate* fast_pair_delegate);
  ~DiscoverySessionManagerImpl() override;

 private:
  friend class DiscoverySessionManagerImplTest;

  // DiscoverySessionManager:
  bool IsDiscoverySessionActive() const override;
  void OnHasAtLeastOneDiscoveryClientChanged() override;
  std::unique_ptr<DevicePairingHandler> CreateDevicePairingHandler(
      AdapterStateController* adapter_state_controller,
      mojo::PendingReceiver<mojom::DevicePairingHandler> receiver) override;

  // device::BluetoothAdapter::Observer:
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;

  void UpdateDiscoveryState();
  void AttemptDiscovery();
  void OnDiscoverySuccess(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnDiscoveryError();
  void DestroyDiscoverySession();

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<FastPairDelegate> fast_pair_delegate_;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};

  bool is_discovery_attempt_in_progress_ = false;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  base::WeakPtrFactory<DiscoverySessionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_IMPL_H_
