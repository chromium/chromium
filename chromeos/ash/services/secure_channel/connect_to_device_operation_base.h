// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_BASE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_BASE_H_

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace ash::secure_channel {

// Base class for concrete ConnectToDeviceOperation implementations.
// ConnectToDeviceOperationBase objects take a device to which to connect as a
// parameter to their constructors and immediately attempt a connection to the
// device when instantiated.
//
// Derived classes should alert clients of success/failure by invoking the
// OnSuccessfulConnectionAttempt() or OnFailedConnectionAttempt() functions.
template <typename FailureDetailType>
class ConnectToDeviceOperationBase
    : public ConnectToDeviceOperation<FailureDetailType> {
 public:
  ConnectToDeviceOperationBase(const ConnectToDeviceOperationBase&) = delete;
  ConnectToDeviceOperationBase& operator=(const ConnectToDeviceOperationBase&) =
      delete;

 protected:
  ConnectToDeviceOperationBase(
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionFailedCallback failure_callback,
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      scoped_refptr<base::TaskRunner> task_runner =
          base::SingleThreadTaskRunner::GetCurrentDefault())
      : ConnectToDeviceOperation<FailureDetailType>(std::move(success_callback),
                                                    std::move(failure_callback),
                                                    connection_priority),
        device_id_pair_(device_id_pair),
        task_runner_(task_runner),
        pending_connection_attempt_priority_(connection_priority) {
    // Attempt a connection; however, post this as a task to be run after the
    // constructor is finished. This ensures that the derived type is fully
    // constructed before a virtual function is invoked.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ConnectToDeviceOperationBase<
                           FailureDetailType>::AttemptConnectionToDevice,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ~ConnectToDeviceOperationBase() override = default;

  void AttemptConnectionToDevice() {
    // If there is no longer a pending connection attempt, this means that the
    // attempt was canceled before it could be started.
    if (!pending_connection_attempt_priority_)
      return;

    // Get the current priority, then reset the pending request.
    ConnectionPriority priority = *pending_connection_attempt_priority_;
    pending_connection_attempt_priority_.reset();

    PerformAttemptConnectionToDevice(priority);
  }

  void CancelInternal() override {
    // If the attempt is still pending, it has been canceled before it even
    // was started. In this case, invalidate the pending request to ensure
    // that it never ends up being attempted.
    if (pending_connection_attempt_priority_) {
      pending_connection_attempt_priority_.reset();
      return;
    }

    // Otherwise, the attempt is already active, so cancel it directly.
    PerformCancellation();
  }

  void UpdateConnectionPriorityInternal(
      ConnectionPriority connection_priority) override {
    // If the attempt is still pending, update the connection priorty so that
    // when the attempt is started, the correct priority will be provided.
    if (pending_connection_attempt_priority_) {
      pending_connection_attempt_priority_ = connection_priority;
      return;
    }

    // Otherwise, the attempt is already active, so update its priority.
    PerformUpdateConnectionPriority(connection_priority);
  }

  virtual void PerformAttemptConnectionToDevice(
      ConnectionPriority connection_priority) = 0;
  virtual void PerformCancellation() = 0;
  virtual void PerformUpdateConnectionPriority(
      ConnectionPriority connection_priority) = 0;

  const DeviceIdPair& device_id_pair() const { return device_id_pair_; }

 private:
  DeviceIdPair device_id_pair_;
  scoped_refptr<base::TaskRunner> task_runner_;
  std::optional<ConnectionPriority> pending_connection_attempt_priority_;
  base::WeakPtrFactory<ConnectToDeviceOperationBase> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_BASE_H_
