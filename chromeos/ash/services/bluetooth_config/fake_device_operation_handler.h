// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_OPERATION_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_OPERATION_HANDLER_H_

#include <cstddef>
#include <string>

#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_operation_handler.h"

namespace ash::bluetooth_config {

class FakeDeviceOperationHandler : public DeviceOperationHandler {
 public:
  explicit FakeDeviceOperationHandler(
      AdapterStateController* adapter_state_controller);
  ~FakeDeviceOperationHandler() override;

  void CompleteCurrentOperation(bool success);

  size_t perform_connect_call_count() const {
    return perform_connect_call_count_;
  }

  const std::string& last_perform_connect_device_id() const {
    return last_perform_connect_device_id_;
  }

 private:
  // DeviceOperationHandler:
  void PerformConnect(const std::string& device_id) override;
  void PerformDisconnect(const std::string& device_id) override;
  void PerformForget(const std::string& device_id) override;
  void HandleOperationTimeout(const PendingOperation& operation) override {}
  device::BluetoothDevice* FindDevice(
      const std::string& device_id) const override;
  void RecordUserInitiatedReconnectionMetrics(
      const device::BluetoothTransport transport,
      std::optional<base::Time> reconnection_attempt_start,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code)
      const override {}

  size_t perform_connect_call_count_ = 0;
  std::string last_perform_connect_device_id_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_OPERATION_HANDLER_H_
