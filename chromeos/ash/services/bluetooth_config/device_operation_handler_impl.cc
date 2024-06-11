// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_operation_handler_impl.h"

#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

namespace ash::bluetooth_config {

DeviceOperationHandlerImpl::DeviceOperationHandlerImpl(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceNameManager* device_name_manager,
    FastPairDelegate* fast_pair_delegate)
    : DeviceOperationHandler(adapter_state_controller),
      bluetooth_adapter_(std::move(bluetooth_adapter)),
      device_name_manager_(device_name_manager),
      fast_pair_delegate_(fast_pair_delegate) {}

DeviceOperationHandlerImpl::~DeviceOperationHandlerImpl() = default;

void DeviceOperationHandlerImpl::PerformConnect(const std::string& device_id) {
  device::BluetoothDevice* device = FindDevice(device_id);
  last_reconnection_attempt_start_ = base::Time::Now();
  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Connect failed due to device not being "
                            "found, device id: "
                         << device_id;
    RecordUserInitiatedReconnectionMetrics(
        device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID,
        last_reconnection_attempt_start_,
        device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    HandleFinishedOperation(/*success=*/false);
    return;
  }

  // Explicitly create Bluetooth classic connections to avoid wrongfully
  // establishing low-energy connections to a disconnected audio-capable device.
  device->ConnectClassic(
      /*delegate=*/nullptr,
      base::BindOnce(&DeviceOperationHandlerImpl::OnDeviceConnect,
                     weak_ptr_factory_.GetWeakPtr(), device->GetType()));
}

void DeviceOperationHandlerImpl::PerformDisconnect(
    const std::string& device_id) {
  device::BluetoothDevice* device = FindDevice(device_id);
  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Disconnect failed due to device not being "
                            "found, device id: "
                         << device_id;
    HandleFinishedOperation(/*success=*/false);
    return;
  }

  device->Disconnect(
      base::BindOnce(&DeviceOperationHandlerImpl::HandleFinishedOperation,
                     weak_ptr_factory_.GetWeakPtr(), /*success=*/true),
      base::BindOnce(&DeviceOperationHandlerImpl::HandleFinishedOperation,
                     weak_ptr_factory_.GetWeakPtr(), /*success=*/false));
}

void DeviceOperationHandlerImpl::PerformForget(const std::string& device_id) {
  device::BluetoothDevice* device = FindDevice(device_id);
  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Forget failed due to device not being "
                            "found, device id: "
                         << device_id;
    HandleFinishedOperation(/*success=*/false);
    return;
  }

  // Cache the address here in case the device stops existing after forget.
  const std::string address = device->GetAddress();

  // We do not expect "Forget" operations to ever fail, so don't bother passing
  // success and failure callbacks here.
  device->Forget(base::DoNothing(), base::BindOnce(
                                        [](const std::string device_id) {
                                          BLUETOOTH_LOG(ERROR)
                                              << "Forget failed, device id: "
                                              << device_id;
                                        },
                                        device_id));

  device_name_manager_->RemoveDeviceNickname(device_id);
  // This delegate will not exist if the Fast Pair feature flag is disabled.
  if (fast_pair_delegate_) {
    fast_pair_delegate_->ForgetDevice(address);
  }
  HandleFinishedOperation(/*success=*/true);
}

void DeviceOperationHandlerImpl::HandleOperationTimeout(
    const PendingOperation& operation) {
  // Invalidate all BluetoothDevice callbacks for the current operation.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (operation.operation != Operation::kConnect) {
    return;
  }

  RecordUserInitiatedReconnectionMetrics(
      operation.transport_type, last_reconnection_attempt_start_,
      device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
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

void DeviceOperationHandlerImpl::RecordUserInitiatedReconnectionMetrics(
    const device::BluetoothTransport transport,
    std::optional<base::Time> reconnection_attempt_start,
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) const {
  std::optional<device::ConnectionFailureReason> failure_reason =
      error_code ? std::make_optional(GetConnectionFailureReason(*error_code))
                 : std::nullopt;
  device::RecordUserInitiatedReconnectionAttemptResult(
      failure_reason, device::UserInitiatedReconnectionUISurfaces::kSettings);
  if (reconnection_attempt_start) {
    device::RecordUserInitiatedReconnectionAttemptDuration(
        failure_reason, transport,
        base::Time::Now() - reconnection_attempt_start.value());
  }
}

void DeviceOperationHandlerImpl::OnDeviceConnect(
    device::BluetoothTransport transport,
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Connect failed with error code: "
                         << error_code.value();
  }

  RecordUserInitiatedReconnectionMetrics(
      transport, last_reconnection_attempt_start_, error_code);

  HandleFinishedOperation(!error_code.has_value());
}

}  // namespace ash::bluetooth_config
