// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_device_operation_handler.h"

namespace ash::bluetooth_config {

FakeDeviceOperationHandler::FakeDeviceOperationHandler(
    AdapterStateController* adapter_state_controller)
    : DeviceOperationHandler(adapter_state_controller) {}

FakeDeviceOperationHandler::~FakeDeviceOperationHandler() = default;

void FakeDeviceOperationHandler::CompleteCurrentOperation(bool success) {
  HandleFinishedOperation(success);
}

void FakeDeviceOperationHandler::PerformConnect(const std::string& device_id) {
  perform_connect_call_count_++;
  last_perform_connect_device_id_ = device_id;
}

void FakeDeviceOperationHandler::PerformDisconnect(
    const std::string& device_id) {}

void FakeDeviceOperationHandler::PerformForget(const std::string& device_id) {}

device::BluetoothDevice* FakeDeviceOperationHandler::FindDevice(
    const std::string& device_id) const {
  return nullptr;
}

}  // namespace ash::bluetooth_config
