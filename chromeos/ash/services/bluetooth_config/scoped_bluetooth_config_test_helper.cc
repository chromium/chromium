// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"

#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_power_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_name_manager.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_operation_handler.h"
#include "chromeos/ash/services/bluetooth_config/fake_discovered_devices_provider.h"
#include "chromeos/ash/services/bluetooth_config/fake_discovery_session_manager.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"

namespace ash::bluetooth_config {

ScopedBluetoothConfigTestHelper::ScopedBluetoothConfigTestHelper() {
  if (floss::features::IsFlossEnabled()) {
    if (!floss::FlossDBusManager::IsInitialized()) {
      floss::FlossDBusManager::InitializeFake();
    }
  } else {
    if (!bluez::BluezDBusManager::IsInitialized()) {
      bluez::BluezDBusManager::InitializeFake();
    }
  }

  OverrideInProcessInstanceForTesting(/*initializer=*/this);
}

ScopedBluetoothConfigTestHelper::~ScopedBluetoothConfigTestHelper() {
  OverrideInProcessInstanceForTesting(/*initializer=*/nullptr);
}

std::unique_ptr<AdapterStateController>
ScopedBluetoothConfigTestHelper::CreateAdapterStateController(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_adapter_state_controller =
      std::make_unique<FakeAdapterStateController>();
  fake_adapter_state_controller_ = fake_adapter_state_controller.get();
  return fake_adapter_state_controller;
}

std::unique_ptr<BluetoothDeviceStatusNotifier>
ScopedBluetoothConfigTestHelper::CreateBluetoothDeviceStatusNotifier(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceCache* device_cache) {
  auto fake_bluetooth_device_status_notifier =
      std::make_unique<FakeBluetoothDeviceStatusNotifier>();
  fake_bluetooth_device_status_notifier_ =
      fake_bluetooth_device_status_notifier.get();
  return fake_bluetooth_device_status_notifier;
}

std::unique_ptr<BluetoothPowerController>
ScopedBluetoothConfigTestHelper::CreateBluetoothPowerController(
    AdapterStateController* adapter_state_controller) {
  auto fake_bluetooth_power_controller =
      std::make_unique<FakeBluetoothPowerController>(adapter_state_controller);
  fake_bluetooth_power_controller_ = fake_bluetooth_power_controller.get();
  return fake_bluetooth_power_controller;
}

std::unique_ptr<DeviceNameManager>
ScopedBluetoothConfigTestHelper::CreateDeviceNameManager(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_device_name_manager = std::make_unique<FakeDeviceNameManager>();
  fake_device_name_manager_ = fake_device_name_manager.get();
  return fake_device_name_manager;
}

std::unique_ptr<DeviceCache> ScopedBluetoothConfigTestHelper::CreateDeviceCache(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceNameManager* device_name_manager,
    FastPairDelegate* fast_pair_delegate) {
  auto fake_device_cache =
      std::make_unique<FakeDeviceCache>(adapter_state_controller);
  fake_device_cache_ = fake_device_cache.get();
  return fake_device_cache;
}

std::unique_ptr<DiscoveredDevicesProvider>
ScopedBluetoothConfigTestHelper::CreateDiscoveredDevicesProvider(
    DeviceCache* device_cache) {
  auto fake_discovered_devices_provider =
      std::make_unique<FakeDiscoveredDevicesProvider>();
  fake_discovered_devices_provider_ = fake_discovered_devices_provider.get();
  return fake_discovered_devices_provider;
}

std::unique_ptr<DiscoverySessionManager>
ScopedBluetoothConfigTestHelper::CreateDiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DiscoveredDevicesProvider* discovered_devices_provider,
    FastPairDelegate* fast_pair_delegate) {
  auto fake_discovery_session_manager =
      std::make_unique<FakeDiscoverySessionManager>(
          adapter_state_controller, discovered_devices_provider);
  fake_discovery_session_manager_ = fake_discovery_session_manager.get();
  return fake_discovery_session_manager;
}

std::unique_ptr<DeviceOperationHandler>
ScopedBluetoothConfigTestHelper::CreateDeviceOperationHandler(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceNameManager* device_name_manager,
    FastPairDelegate* fast_pair_delegate) {
  auto fake_device_operation_handler =
      std::make_unique<FakeDeviceOperationHandler>(adapter_state_controller);
  fake_device_operation_handler_ = fake_device_operation_handler.get();
  return fake_device_operation_handler;
}

}  // namespace ash::bluetooth_config
