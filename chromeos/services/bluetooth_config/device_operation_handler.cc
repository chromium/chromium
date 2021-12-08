// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_operation_handler.h"

#include "components/device_event_log/device_event_log.h"

namespace chromeos {
namespace bluetooth_config {

DeviceOperationHandler::PendingOperation::PendingOperation(
    Operation operation_,
    const std::string& device_id_,
    OperationCallback callback_)
    : operation(operation_),
      device_id(device_id_),
      callback(std::move(callback_)) {}

DeviceOperationHandler::PendingOperation::PendingOperation(
    PendingOperation&& other) {
  operation = other.operation;
  device_id = other.device_id;
  callback = std::move(other.callback);
}

DeviceOperationHandler::PendingOperation&
DeviceOperationHandler::PendingOperation::operator=(PendingOperation other) {
  operation = other.operation;
  device_id = other.device_id;
  callback = std::move(other.callback);
  return *this;
}

DeviceOperationHandler::PendingOperation::~PendingOperation() = default;

DeviceOperationHandler::DeviceOperationHandler(
    AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {
  adapter_state_controller_observation_.Observe(adapter_state_controller_);
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
                       << (queue_.size() + 1) << " operations already queued)";
  queue_.emplace(operation, device_id, std::move(callback));
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
    HandleFinishedOperation(/*success=*/false);
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Device with id: " << current_operation_->device_id
                       << " starting operation: "
                       << current_operation_->operation;

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

}  // namespace bluetooth_config
}  // namespace chromeos
