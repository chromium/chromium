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
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/cross_device/timer_factory/timer_factory_impl.h"

namespace ash::tether {

namespace {

const char kTetherFeature[] = "magic_tether";

}  // namespace

MessageTransferOperation::MessageTransferOperation(
    const TetherHost& tether_host,
    secure_channel::ConnectionPriority connection_priority,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : tether_host_(tether_host),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      connection_priority_(connection_priority),
      timer_factory_(cross_device::TimerFactoryImpl::Factory::Create()) {}

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

  std::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();
  if (!local_device) {
    PA_LOG(ERROR) << "MessageTransferOperation::" << __func__
                  << ": Local device unexpectedly null.";
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
  connection_attempt_ = secure_channel_client_->ListenForConnectionFromDevice(
      tether_host_.remote_device_ref().value(), *local_device, kTetherFeature,
      secure_channel::ConnectionMedium::kBluetoothLowEnergy,
      connection_priority_);

  connection_attempt_->SetDelegate(this);
}

void MessageTransferOperation::OnMessageReceived(const std::string& payload) {
  std::unique_ptr<MessageWrapper> message_wrapper =
      MessageWrapper::FromRawMessage(payload);
  if (message_wrapper) {
    OnMessageReceived(std::move(message_wrapper));
  }
}

void MessageTransferOperation::StopOperation() {
  // Note: This function may be called from the destructor. It is invalid to
  // invoke any virtual methods if |shutting_down_| is true.

  StopTimerForDeviceIfRunning();

  connection_attempt_.reset();
  if (client_channel_ != nullptr) {
    client_channel_.reset();
  }

  if (!shutting_down_) {
    OnOperationFinished();
  }
}

int MessageTransferOperation::SendMessageToDevice(
    std::unique_ptr<MessageWrapper> message_wrapper) {
  DCHECK(client_channel_ != nullptr);
  int sequence_number = next_message_sequence_number_++;
  bool success = client_channel_->SendMessage(
      message_wrapper->ToRawMessage(),
      base::BindOnce(&MessageTransferOperation::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), sequence_number));
  return success ? sequence_number : -1;
}

uint32_t MessageTransferOperation::GetMessageTimeoutSeconds() {
  return MessageTransferOperation::kDefaultMessageTimeoutSeconds;
}

void MessageTransferOperation::OnConnectionAttemptFailure(
    secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(WARNING) << "Failed to connect to device "
                  << GetDeviceId(/*truncate_for_logs=*/true)
                  << ", error: " << reason;
  StopOperation();
}

void MessageTransferOperation::OnConnection(
    std::unique_ptr<secure_channel::ClientChannel> channel) {
  client_channel_ = std::move(channel);
  client_channel_->AddObserver(this);

  // Stop the timer which was started from StartConnectionTimerForDevice() since
  // the connection has now been established. Start another timer now via
  // StartMessageTimerForDevice() while waiting for messages to be sent to and
  // received by |remote_device|.
  StopTimerForDeviceIfRunning();
  StartMessageTimerForDevice();

  OnDeviceAuthenticated();
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
    std::unique_ptr<cross_device::TimerFactory> timer_factory_for_test) {
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
