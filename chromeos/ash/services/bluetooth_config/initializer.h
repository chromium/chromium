// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_INITIALIZER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_INITIALIZER_H_

#include <memory>

#include "base/memory/ref_counted.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash::bluetooth_config {

class AdapterStateController;
class BluetoothDeviceStatusNotifier;
class BluetoothPowerController;
class DeviceCache;
class DeviceNameManager;
class DeviceOperationHandler;
class DiscoveredDevicesProvider;
class DiscoverySessionManager;
class FastPairDelegate;

// Responsible for initializing the classes needed by the CrosBluetoothConfig
// API.
class Initializer {
 public:
  Initializer(const Initializer&) = delete;
  Initializer& operator=(const Initializer&) = delete;
  virtual ~Initializer() = default;

  virtual std::unique_ptr<AdapterStateController> CreateAdapterStateController(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;
  virtual std::unique_ptr<BluetoothDeviceStatusNotifier>
  CreateBluetoothDeviceStatusNotifier(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceCache* device_cache) = 0;
  virtual std::unique_ptr<BluetoothPowerController>
  CreateBluetoothPowerController(
      AdapterStateController* adapter_state_controller) = 0;
  virtual std::unique_ptr<DeviceNameManager> CreateDeviceNameManager(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;
  virtual std::unique_ptr<DeviceCache> CreateDeviceCache(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceNameManager* device_name_manager,
      FastPairDelegate* fast_pair_delegate) = 0;
  virtual std::unique_ptr<DiscoveredDevicesProvider>
  CreateDiscoveredDevicesProvider(DeviceCache* device_cache) = 0;
  virtual std::unique_ptr<DiscoverySessionManager>
  CreateDiscoverySessionManager(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DiscoveredDevicesProvider* discovered_devices_provider,
      FastPairDelegate* fast_pair_delegate) = 0;
  virtual std::unique_ptr<DeviceOperationHandler> CreateDeviceOperationHandler(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceNameManager* device_name_manager,
      FastPairDelegate* fast_pair_delegate) = 0;

 protected:
  Initializer() = default;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_INITIALIZER_H_
