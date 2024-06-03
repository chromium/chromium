// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/message_transfer_operation.h"

#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/timer_factory/timer_factory_impl.h"

namespace ash::tether {

MessageTransferOperation::MessageTransferOperation(
    const TetherHost& tether_host,
    HostConnection::Factory::ConnectionPriority connection_priority,
    raw_ptr<HostConnection::Factory> host_connection_factory)
    : tether_host_(tether_host),
      connection_priority_(connection_priority),
      host_connection_factory_(host_connection_factory),
      timer_factory_(ash::timer_factory::TimerFactoryImpl::Factory::Create()) {}

MessageTransferOperation::~MessageTransferOperation() {
  // If initialization never occurred, devices were never registered.
  if (!initialized_) {
    return;
  }

  shutting_down_ = true;

  // Stop the operation if it's in flight, as the operation itself
  // will be destroyed.
  StopOperation();
}

void MessageTransferOperation::Initialize() {
  if (initialized_) {
    return;
  }

  initialized_ = true;

  // Store the message type for this connection as a private field. This is
  // necessary because when UnregisterDevice() is called in the destructor, the
  // derived class has already been destroyed, so invoking
  // GetMessageTypeForConnection() will fail due to it being a pure virtual
  // function at that time.
  message_type_for_connection_ = GetMessageTypeForConnection();

  OnOperationStarted();

  StartConnectionTimerForDevice();
  host_connection_factory_->Create(
      tether_host_, connection_priority_, /*payload_listener=*/this,
      base::BindOnce(&MessageTransferOperation::OnDisconnected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MessageTransferOperation::OnConnectionAttemptComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MessageTransferOperation::StopOperation() {
  // Note: This function may be called from the destructor. It is invalid to
  // invoke any virtual methods if |shutting_down_| is true.

  StopTimerForDeviceIfRunning();

  host_connection_.reset();

  if (!shutting_down_) {
    OnOperationFinished();
  }
}

void MessageTransferOperation::SendMessage(
    std::unique_ptr<MessageWrapper> message_wrapper,
    HostConnection::OnMessageSentCallback on_message_sent) {
  CHECK(host_connection_);
  host_connection_->SendMessage(std::move(message_wrapper),
                                std::move(on_message_sent));
}

uint32_t MessageTransferOperation::GetMessageTimeoutSeconds() {
  return MessageTransferOperation::kDefaultMessageTimeoutSeconds;
}

void MessageTransferOperation::OnConnectionAttemptComplete(
    std::unique_ptr<HostConnection> host_connection) {
  if (!host_connection) {
    PA_LOG(WARNING) << "Failed to connect to device ["
                    << GetDeviceId(/*truncate_for_logs=*/true) << "].";
    StopOperation();
  } else {
    host_connection_ = std::move(host_connection);

    // Stop the timer which was started from StartConnectionTimerForDevice()
    // since the connection has now been established. Start another timer now
    // via StartMessageTimerForDevice() while waiting for messages to be sent to
    // and received by |remote_device|.
    StopTimerForDeviceIfRunning();
    StartMessageTimerForDevice();

    PA_LOG(INFO) << "Successfully opened connection to ["
                 << GetDeviceId(/*truncate_for_logs=*/true) << "].";
    OnDeviceAuthenticated();
  }
}

void MessageTransferOperation::OnDisconnected() {
  PA_LOG(VERBOSE) << "Remote device disconnected from this device: "
                  << GetDeviceId(/*truncate_for_logs=*/true);
  StopOperation();
}

void MessageTransferOperation::StartConnectionTimerForDevice() {
  StartTimerForDevice(kConnectionTimeoutSeconds);
}

void MessageTransferOperation::StartMessageTimerForDevice() {
  StartTimerForDevice(GetMessageTimeoutSeconds());
}

void MessageTransferOperation::StartTimerForDevice(uint32_t timeout_seconds) {
  PA_LOG(VERBOSE) << "Starting timer for operation with message type "
                  << message_type_for_connection_ << " from device with ID "
                  << GetDeviceId(/*truncate_for_logs=*/true) << ".";

  remote_device_timer_ = timer_factory_->CreateOneShotTimer();
  remote_device_timer_->Start(
      FROM_HERE, base::Seconds(timeout_seconds),
      base::BindOnce(&MessageTransferOperation::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MessageTransferOperation::StopTimerForDeviceIfRunning() {
  if (!remote_device_timer_) {
    return;
  }

  remote_device_timer_->Stop();
  remote_device_timer_.reset();
}

void MessageTransferOperation::OnTimeout() {
  PA_LOG(WARNING) << "Timed out operation for message type "
                  << message_type_for_connection_ << " from device with ID "
                  << GetDeviceId(/*truncate_for_logs=*/true) << ".";

  remote_device_timer_.reset();
  StopOperation();
}

void MessageTransferOperation::SetTimerFactoryForTest(
    std::unique_ptr<ash::timer_factory::TimerFactory> timer_factory_for_test) {
  timer_factory_ = std::move(timer_factory_for_test);
}

const std::string MessageTransferOperation::GetDeviceId(
    bool truncate_for_logs) const {
  if (truncate_for_logs) {
    return tether_host_.GetTruncatedDeviceIdForLogs();
  } else {
    return tether_host_.GetDeviceId();
  }
}

}  // namespace ash::tether
