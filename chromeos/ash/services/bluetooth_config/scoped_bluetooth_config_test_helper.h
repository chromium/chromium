// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SCOPED_BLUETOOTH_CONFIG_TEST_HELPER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SCOPED_BLUETOOTH_CONFIG_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/initializer.h"

#include "components/session_manager/core/session_manager.h"

namespace ash::bluetooth_config {

class FakeAdapterStateController;
class FakeBluetoothDeviceStatusNotifier;
class FakeBluetoothPowerController;
class FakeDeviceCache;
class FakeDeviceNameManager;
class FakeDeviceOperationHandler;
class FakeDiscoveredDevicesProvider;
class FakeDiscoverySessionManager;

// Test helper which provides access to fake implementations. This class
// automatically overrides CrosBluetoothConfig when created and reverses the
// override when it is deleted.
class ScopedBluetoothConfigTestHelper : public Initializer {
 public:
  ScopedBluetoothConfigTestHelper();
  ScopedBluetoothConfigTestHelper(const ScopedBluetoothConfigTestHelper&) =
      delete;
  ScopedBluetoothConfigTestHelper& operator=(
      const ScopedBluetoothConfigTestHelper&) = delete;
  ~ScopedBluetoothConfigTestHelper() override;

  FakeAdapterStateController* fake_adapter_state_controller() {
    return fake_adapter_state_controller_;
  }

  FakeBluetoothDeviceStatusNotifier* fake_bluetooth_device_status_notifier() {
    return fake_bluetooth_device_status_notifier_;
  }

  FakeBluetoothPowerController* fake_bluetooth_power_controller() {
    return fake_bluetooth_power_controller_;
  }

  FakeDeviceNameManager* fake_device_name_manager() {
    return fake_device_name_manager_;
  }

  FakeDeviceCache* fake_device_cache() { return fake_device_cache_; }

  FakeDiscoveredDevicesProvider* fake_discovered_devices_provider() {
    return fake_discovered_devices_provider_;
  }

  FakeDiscoverySessionManager* fake_discovery_session_manager() {
    return fake_discovery_session_manager_;
  }

  FakeDeviceOperationHandler* fake_device_operation_handler() {
    return fake_device_operation_handler_;
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

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

  raw_ptr<FakeAdapterStateController, DanglingUntriaged>
      fake_adapter_state_controller_;
  raw_ptr<FakeBluetoothDeviceStatusNotifier, DanglingUntriaged>
      fake_bluetooth_device_status_notifier_;
  raw_ptr<FakeBluetoothPowerController, DanglingUntriaged>
      fake_bluetooth_power_controller_;
  raw_ptr<FakeDeviceNameManager, DanglingUntriaged> fake_device_name_manager_;
  raw_ptr<FakeDeviceCache, DanglingUntriaged> fake_device_cache_;
  raw_ptr<FakeDiscoveredDevicesProvider, DanglingUntriaged>
      fake_discovered_devices_provider_;
  raw_ptr<FakeDiscoverySessionManager, DanglingUntriaged>
      fake_discovery_session_manager_;
  raw_ptr<FakeDeviceOperationHandler, DanglingUntriaged>
      fake_device_operation_handler_;
  session_manager::SessionManager session_manager_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SCOPED_BLUETOOTH_CONFIG_TEST_HELPER_H_
