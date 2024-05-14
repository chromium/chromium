// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_H_

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace ash::secure_channel {

class AuthenticatedChannel;

// Performs an operation which creates a connection to a remote device. A
// ConnectToDeviceOperation can only be used for a single connection attempt; if
// clients wish to retry a failed connection attempt, a new
// ConnectToDeviceOperation object should be created.
template <typename FailureDetailType>
class ConnectToDeviceOperation {
 public:
  using ConnectionSuccessCallback =
      base::OnceCallback<void(std::unique_ptr<AuthenticatedChannel>)>;
  using ConnectionFailedCallback =
      base::RepeatingCallback<void(FailureDetailType)>;

  ConnectToDeviceOperation(const ConnectToDeviceOperation&) = delete;
  ConnectToDeviceOperation& operator=(const ConnectToDeviceOperation&) = delete;

  virtual ~ConnectToDeviceOperation() {
    if (has_finished_)
      return;

    PA_LOG(ERROR) << "ConnectToDeviceOperation::~ConnectToDeviceOperation(): "
                  << "Operation deleted before it finished or was canceled.";
    NOTREACHED_IN_MIGRATION();
  }

  // Updates the priority for this operation.
  void UpdateConnectionPriority(ConnectionPriority connection_priority) {
    if (has_finished_) {
      PA_LOG(ERROR) << "ConnectToDeviceOperation::UpdateConnectionPriority(): "
                    << "Connection priority update requested, but the "
                    << "operation was no longer active.";
      NOTREACHED_IN_MIGRATION();
      return;
    }

    connection_priority_ = connection_priority;
    UpdateConnectionPriorityInternal(connection_priority);
  }

  // Note: Canceling an ongoing connection attempt will not cause either of the
  // success/failure callbacks passed to the constructor to be invoked.
  void Cancel() {
    if (has_finished_) {
      PA_LOG(ERROR) << "ConnectToDeviceOperation::Cancel(): Tried to cancel "
                    << "operation after it had already finished.";
      NOTREACHED_IN_MIGRATION();
      return;
    }

    has_finished_ = true;
    CancelInternal();
  }

  ConnectionPriority connection_priority() const {
    return connection_priority_;
  }

 protected:
  ConnectToDeviceOperation(ConnectionSuccessCallback success_callback,
                           const ConnectionFailedCallback& failure_callback,
                           ConnectionPriority connection_priority)
      : success_callback_(std::move(success_callback)),
        failure_callback_(failure_callback),
        connection_priority_(connection_priority) {}

  // Derived types can override these functions to implement cancellation and
  // updating of priority.
  virtual void CancelInternal() {}
  virtual void UpdateConnectionPriorityInternal(
      ConnectionPriority connection_priority) {}

  void OnSuccessfulConnectionAttempt(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    if (has_finished_) {
      PA_LOG(ERROR) << "ConnectToDeviceOperation::"
                    << "OnSuccessfulConnectionAttempt(): Tried to "
                    << "complete operation after it had already finished.";
      return;
    }

    has_finished_ = true;
    std::move(success_callback_).Run(std::move(authenticated_channel));
  }

  void OnFailedConnectionAttempt(FailureDetailType failure_detail) {
    failure_callback_.Run(failure_detail);
  }

 private:
  bool has_finished_ = false;

  ConnectionSuccessCallback success_callback_;
  ConnectionFailedCallback failure_callback_;
  ConnectionPriority connection_priority_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_H_
