// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/initializer_impl.h"

#include "chromeos/services/bluetooth_config/adapter_state_controller_impl.h"
#include "chromeos/services/bluetooth_config/bluetooth_device_status_notifier_impl.h"
#include "chromeos/services/bluetooth_config/bluetooth_power_controller_impl.h"
#include "chromeos/services/bluetooth_config/device_cache_impl.h"
#include "chromeos/services/bluetooth_config/device_name_manager_impl.h"
#include "chromeos/services/bluetooth_config/device_operation_handler_impl.h"
#include "chromeos/services/bluetooth_config/discovered_devices_provider_impl.h"
#include "chromeos/services/bluetooth_config/discovery_session_manager_impl.h"

namespace chromeos {
namespace bluetooth_config {

InitializerImpl::InitializerImpl() = default;

InitializerImpl::~InitializerImpl() = default;

std::unique_ptr<AdapterStateController>
InitializerImpl::CreateAdapterStateController(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  return std::make_unique<AdapterStateControllerImpl>(
      std::move(bluetooth_adapter));
}

std::unique_ptr<BluetoothDeviceStatusNotifier>
InitializerImpl::CreateBluetoothDeviceStatusNotifier(
    DeviceCache* device_cache) {
  return std::make_unique<BluetoothDeviceStatusNotifierImpl>(device_cache);
}

std::unique_ptr<BluetoothPowerController>
InitializerImpl::CreateBluetoothPowerController(
    AdapterStateController* adapter_state_controller) {
  return std::make_unique<BluetoothPowerControllerImpl>(
      adapter_state_controller);
}

std::unique_ptr<DeviceNameManager> InitializerImpl::CreateDeviceNameManager(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  return std::make_unique<DeviceNameManagerImpl>(std::move(bluetooth_adapter));
}

std::unique_ptr<DeviceCache> InitializerImpl::CreateDeviceCache(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceNameManager* device_name_manager) {
  return std::make_unique<DeviceCacheImpl>(adapter_state_controller,
                                           std::move(bluetooth_adapter),
                                           device_name_manager);
}

std::unique_ptr<DiscoveredDevicesProvider>
InitializerImpl::CreateDiscoveredDevicesProvider(DeviceCache* device_cache) {
  return std::make_unique<DiscoveredDevicesProviderImpl>(device_cache);
}

std::unique_ptr<DiscoverySessionManager>
InitializerImpl::CreateDiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DiscoveredDevicesProvider* discovered_devices_provider) {
  return std::make_unique<DiscoverySessionManagerImpl>(
      adapter_state_controller, std::move(bluetooth_adapter),
      discovered_devices_provider);
}

std::unique_ptr<DeviceOperationHandler>
InitializerImpl::CreateDeviceOperationHandler(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceNameManager* device_name_manager) {
  return std::make_unique<DeviceOperationHandlerImpl>(
      adapter_state_controller, std::move(bluetooth_adapter),
      device_name_manager);
}

}  // namespace bluetooth_config
}  // namespace chromeos
