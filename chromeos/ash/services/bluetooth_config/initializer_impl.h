// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_INITIALIZER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_INITIALIZER_IMPL_H_

#include "chromeos/ash/services/bluetooth_config/initializer.h"

namespace ash::bluetooth_config {

// Concrete Initializer implementation.
class InitializerImpl : public Initializer {
 public:
  InitializerImpl();
  InitializerImpl(const InitializerImpl&) = delete;
  InitializerImpl& operator=(const InitializerImpl&) = delete;
  ~InitializerImpl() override;

 private:
  // Initializer:
  std::unique_ptr<AdapterStateController> CreateAdapterStateController(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override;
  std::unique_ptr<BluetoothDeviceStatusNotifier>
  CreateBluetoothDeviceStatusNotifier(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceCache* device_cache) override;
  std::unique_ptr<BluetoothPowerController> CreateBluetoothPowerController(
      AdapterStateController* adapter_state_controller) override;
  std::unique_ptr<DeviceNameManager> CreateDeviceNameManager(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override;
  std::unique_ptr<DeviceCache> CreateDeviceCache(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceNameManager* device_name_manager,
      FastPairDelegate* fast_pair_delegate) override;
  std::unique_ptr<DiscoveredDevicesProvider> CreateDiscoveredDevicesProvider(
      DeviceCache* device_cache) override;
  std::unique_ptr<DiscoverySessionManager> CreateDiscoverySessionManager(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DiscoveredDevicesProvider* discovered_devices_provider,
      FastPairDelegate* fast_pair_delegate) override;
  std::unique_ptr<DeviceOperationHandler> CreateDeviceOperationHandler(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceNameManager* device_name_manager,
      FastPairDelegate* fast_pair_delegate) override;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_INITIALIZER_IMPL_H_
