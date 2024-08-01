// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_operation_handler.h"

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash::bluetooth_config {

DeviceOperationHandler::PendingOperation::PendingOperation(
    Operation operation_,
    const std::string& device_id_,
    const device::BluetoothTransport& transport_type_,
    OperationCallback callback_)
    : operation(operation_),
      device_id(device_id_),
      transport_type(transport_type_),
      callback(std::move(callback_)) {}

DeviceOperationHandler::PendingOperation::PendingOperation(
    PendingOperation&& other) {
  operation = other.operation;
  device_id = other.device_id;
  transport_type = other.transport_type;
  callback = std::move(other.callback);
}

DeviceOperationHandler::PendingOperation&
DeviceOperationHandler::PendingOperation::operator=(PendingOperation other) {
  operation = other.operation;
  device_id = other.device_id;
  transport_type = other.transport_type;
  callback = std::move(other.callback);
  return *this;
}

DeviceOperationHandler::PendingOperation::~PendingOperation() = default;

// static
const base::TimeDelta DeviceOperationHandler::kOperationTimeout =
    base::Seconds(15);

DeviceOperationHandler::DeviceOperationHandler(
    AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {
  adapter_state_controller_observation_.Observe(
      adapter_state_controller_.get());
}

DeviceOperationHandler::~DeviceOperationHandler() = default;

void DeviceOperationHandler::Connect(const std::string& device_id,
                                     OperationCallback callback) {
  EnqueueOperation(Operation::kConnect, device_id, std::move(callback));
}

void DeviceOperationHandler::Disconnect(const std::string& device_id,
                                        OperationCallback callback) {
  EnqueueOperation(Operation::kDisconnect, device_id, std::move(callback));
}

void DeviceOperationHandler::Forget(const std::string& device_id,
                                    OperationCallback callback) {
  EnqueueOperation(Operation::kForget, device_id, std::move(callback));
}

void DeviceOperationHandler::HandleFinishedOperation(bool success) {
  current_operation_timer_.Stop();
  if (success) {
    BLUETOOTH_LOG(EVENT) << "Device with id: " << current_operation_->device_id
                         << " succeeded operation: "
                         << current_operation_->operation;
  } else {
    BLUETOOTH_LOG(ERROR) << "Device with id: " << current_operation_->device_id
                         << " failed operation: "
                         << current_operation_->operation;
  }

  // Return the result of the operation to the client.
  std::move(current_operation_->callback).Run(std::move(success));
  current_operation_.reset();
  ProcessQueue();
}

// TODO(gordonseto): Investigate whether we need to manually fail the current
// operation occurring if Bluetooth disables. If we don't, we can remove this
// observer.
void DeviceOperationHandler::OnAdapterStateChanged() {
  if (current_operation_) {
    BLUETOOTH_LOG(DEBUG) << "Device with id: " << current_operation_->device_id
                         << " adapter state changed during operation: "
                         << current_operation_->operation;
  }
  if (IsBluetoothEnabled())
    return;
}

void DeviceOperationHandler::EnqueueOperation(Operation operation,
                                              const std::string& device_id,
                                              OperationCallback callback) {
  BLUETOOTH_LOG(DEBUG) << "Device with id: " << device_id
                       << " enqueueing operation: " << operation << " ("
                       << queue_.size() << " operations already queued)";
  device::BluetoothDevice* device = FindDevice(device_id);
  device::BluetoothTransport type =
      device ? device->GetType()
             : device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID;
  queue_.emplace(operation, device_id, type, std::move(callback));
  ProcessQueue();
}

void DeviceOperationHandler::ProcessQueue() {
  if (current_operation_) {
    BLUETOOTH_LOG(DEBUG) << "Device with id: " << current_operation_->device_id
                         << " continuing operation: "
                         << current_operation_->operation;
    return;
  }

  if (queue_.empty()) {
    BLUETOOTH_LOG(DEBUG) << "No operations queued";
    return;
  }

  PerformNextOperation();
}

void DeviceOperationHandler::PerformNextOperation() {
  current_operation_ = std::move(queue_.front());
  queue_.pop();

  if (!IsBluetoothEnabled()) {
    BLUETOOTH_LOG(ERROR)
        << "Operation failed due to Bluetooth not being enabled, device id: "
        << current_operation_->device_id;
    RecordUserInitiatedReconnectionMetrics(
        device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID,
        base::Time::Now(),
        device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    HandleFinishedOperation(/*success=*/false);
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Device with id: " << current_operation_->device_id
                       << " starting operation: "
                       << current_operation_->operation;
  current_operation_timer_.Start(
      FROM_HERE, kOperationTimeout,
      base::BindOnce(&DeviceOperationHandler::OnOperationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  switch (current_operation_->operation) {
    case Operation::kConnect:
      PerformConnect(current_operation_->device_id);
      return;
    case Operation::kDisconnect:
      PerformDisconnect(current_operation_->device_id);
      return;
    case Operation::kForget:
      PerformForget(current_operation_->device_id);
      return;
  }
}

void DeviceOperationHandler::OnOperationTimeout() {
  BLUETOOTH_LOG(ERROR) << "Operation for device with id: "
                       << current_operation_->device_id << " timed out.";
  DCHECK(current_operation_);
  HandleOperationTimeout(current_operation_.value());
  HandleFinishedOperation(/*success=*/false);
}

bool DeviceOperationHandler::IsBluetoothEnabled() const {
  return adapter_state_controller_->GetAdapterState() ==
         mojom::BluetoothSystemState::kEnabled;
}

std::ostream& operator<<(std::ostream& stream,
                         const DeviceOperationHandler::Operation& operation) {
  switch (operation) {
    case DeviceOperationHandler::Operation::kConnect:
      stream << "[Connect]";
      break;
    case DeviceOperationHandler::Operation::kDisconnect:
      stream << "[Disconnect]";
      break;
    case DeviceOperationHandler::Operation::kForget:
      stream << "[Forget]";
      break;
  }
  return stream;
}

}  // namespace ash::bluetooth_config
