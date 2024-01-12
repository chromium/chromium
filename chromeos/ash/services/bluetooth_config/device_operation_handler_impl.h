// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_OPERATION_HANDLER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_OPERATION_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"
#include "chromeos/ash/services/bluetooth_config/device_operation_handler.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash::bluetooth_config {

// Concrete DeviceOperationHandler implementation that calls
// device::BluetoothDevice methods.
class DeviceOperationHandlerImpl : public DeviceOperationHandler {
 public:
  DeviceOperationHandlerImpl(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceNameManager* device_name_manager,
      FastPairDelegate* fast_pair_delegate);
  ~DeviceOperationHandlerImpl() override;

 private:
  // DeviceOperationHandler:
  void PerformConnect(const std::string& device_id) override;
  void PerformDisconnect(const std::string& device_id) override;
  void PerformForget(const std::string& device_id) override;
  void HandleOperationTimeout(const PendingOperation& operation) override;
  device::BluetoothDevice* FindDevice(
      const std::string& device_id) const override;
  void RecordUserInitiatedReconnectionMetrics(
      const device::BluetoothTransport transport,
      std::optional<base::Time> reconnection_attempt_start,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code)
      const override;

  // device::BluetoothDevice::Connect() callback.
  void OnDeviceConnect(
      device::BluetoothTransport transport,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<DeviceNameManager> device_name_manager_;
  raw_ptr<FastPairDelegate> fast_pair_delegate_;

  base::Time last_reconnection_attempt_start_;

  base::WeakPtrFactory<DeviceOperationHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_OPERATION_HANDLER_IMPL_H_
