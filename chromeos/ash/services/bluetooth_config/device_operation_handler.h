// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_OPERATION_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_OPERATION_HANDLER_H_

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash::bluetooth_config {

// Manages device-specific operations, such as connecting or disconnecting to a
// device. Operations are performed sequentially, queueing requests that occur
// simultaneously.
//
// This class uses AdapterStateController to ensure that operations can only be
// initiated when Bluetooth is enabled.
class DeviceOperationHandler : public AdapterStateController::Observer {
 public:
  using OperationCallback = base::OnceCallback<void(bool)>;

  ~DeviceOperationHandler() override;

  // Initiates a connection to the device with ID |device_id|.
  void Connect(const std::string& device_id, OperationCallback callback);

  // Initiates a disconnection from the device with ID |device_id|.
  void Disconnect(const std::string& device_id, OperationCallback callback);

  // Forgets the device with ID |device_id|, which in practice means
  // un-pairing from the device.
  void Forget(const std::string& device_id, OperationCallback callback);

 protected:
  enum class Operation {
    kConnect,
    kDisconnect,
    kForget,
  };

  struct PendingOperation {
    PendingOperation(Operation operation_,
                     const std::string& device_id_,
                     const device::BluetoothTransport& transport_type,
                     OperationCallback callback_);
    PendingOperation(PendingOperation&& other);
    PendingOperation& operator=(PendingOperation other);
    ~PendingOperation();

    Operation operation;
    std::string device_id;
    device::BluetoothTransport transport_type;
    OperationCallback callback;
  };
  explicit DeviceOperationHandler(
      AdapterStateController* adapter_state_controller);

  // Invokes |current_operation_.callback| with the result of the operation.
  void HandleFinishedOperation(bool success);

  // Implementation-specific methods.
  virtual void PerformConnect(const std::string& device_id) = 0;
  virtual void PerformDisconnect(const std::string& device_id) = 0;
  virtual void PerformForget(const std::string& device_id) = 0;

  // Informs derived classes the current operation timed out.
  virtual void HandleOperationTimeout(const PendingOperation& operation) = 0;

  // Finds a BluetoothDevice* based on device_id. If no device is found, nullptr
  // is returned.
  virtual device::BluetoothDevice* FindDevice(
      const std::string& device_id) const = 0;

  virtual void RecordUserInitiatedReconnectionMetrics(
      const device::BluetoothTransport transport,
      std::optional<base::Time> reconnection_attempt_start,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code)
      const = 0;

 private:
  friend class DeviceOperationHandlerImplTest;

  // Timeout after which an operation is considered to have failed.
  static const base::TimeDelta kOperationTimeout;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const Operation& operation);

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // Adds an operation to the queue.
  void EnqueueOperation(Operation operation,
                        const std::string& device_id,
                        OperationCallback callback);
  void ProcessQueue();

  // Attempts to perform the operation at the front of the queue.
  void PerformNextOperation();

  // Method invoked once |current_operation_timer_| expires indicating that
  // |current_operation_| has timed out.
  void OnOperationTimeout();

  bool IsBluetoothEnabled() const;

  std::optional<PendingOperation> current_operation_;
  base::OneShotTimer current_operation_timer_;

  base::queue<PendingOperation> queue_;

  raw_ptr<AdapterStateController> adapter_state_controller_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};

  base::WeakPtrFactory<DeviceOperationHandler> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_OPERATION_HANDLER_H_
