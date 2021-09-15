// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_operation_handler_impl.h"

#include "components/device_event_log/device_event_log.h"

namespace chromeos {
namespace bluetooth_config {

DeviceOperationHandlerImpl::DeviceOperationHandlerImpl(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : DeviceOperationHandler(adapter_state_controller),
      bluetooth_adapter_(std::move(bluetooth_adapter)) {}

DeviceOperationHandlerImpl::~DeviceOperationHandlerImpl() = default;

void DeviceOperationHandlerImpl::PerformConnect(const std::string& device_id) {
  device::BluetoothDevice* device = FindDevice(device_id);
  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Connect failed due to device not being "
                            "found, device id: "
                         << device_id;
    HandleFinishedOperation(/*success=*/false);
    return;
  }

  device->Connect(
      /*delegate=*/nullptr,
      base::BindOnce(&DeviceOperationHandlerImpl::OnDeviceConnect,
                     weak_ptr_factory_.GetWeakPtr(), device->GetAddress(),
                     device->GetDeviceType()));
}

void DeviceOperationHandlerImpl::OnDeviceConnect(
    const std::string& address,
    device::BluetoothDeviceType device_type,
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Connect failed with error code: "
                         << error_code.value();
  }

  HandleFinishedOperation(!error_code.has_value());
}

device::BluetoothDevice* DeviceOperationHandlerImpl::FindDevice(
    const std::string& device_id) const {
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetIdentifier() != device_id)
      continue;
    return device;
  }
  return nullptr;
}

}  // namespace bluetooth_config
}  // namespace chromeos
