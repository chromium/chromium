// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/initializer_impl.h"

#include "chromeos/services/bluetooth_config/adapter_state_controller_impl.h"
#include "chromeos/services/bluetooth_config/device_cache_impl.h"
#include "chromeos/services/bluetooth_config/device_name_manager_impl.h"
#include "chromeos/services/bluetooth_config/device_operation_handler_impl.h"
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

std::unique_ptr<DiscoverySessionManager>
InitializerImpl::CreateDiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceCache* device_cache) {
  return std::make_unique<DiscoverySessionManagerImpl>(
      adapter_state_controller, std::move(bluetooth_adapter), device_cache);
}

std::unique_ptr<DeviceOperationHandler>
InitializerImpl::CreateDeviceOperationHandler(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  return std::make_unique<DeviceOperationHandlerImpl>(
      adapter_state_controller, std::move(bluetooth_adapter));
}

}  // namespace bluetooth_config
}  // namespace chromeos
