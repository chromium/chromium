// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/message_transfer_operation.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/timer_factory.h"

namespace chromeos {

namespace tether {

namespace {

const char kTetherFeature[] = "magic_tether";

multidevice::RemoteDeviceRefList RemoveDuplicatesFromVector(
    const multidevice::RemoteDeviceRefList& remote_devices) {
  multidevice::RemoteDeviceRefList updated_remote_devices;
  std::set<multidevice::RemoteDeviceRef> remote_devices_set;
  for (const auto& remote_device : remote_devices) {
    // Only add the device to the output vector if it has not already been put
    // into the set.
    if (remote_devices_set.find(remote_device) == remote_devices_set.end()) {
      remote_devices_set.insert(remote_device);
      updated_remote_devices.push_back(remote_device);
    }
  }
  return updated_remote_devices;
}

}  // namespace

MessageTransferOperation::ConnectionAttemptDelegate::ConnectionAttemptDelegate(
    MessageTransferOperation* operation,
    multidevice::RemoteDeviceRef remote_device,
    std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt)
    : operation_(operation),
      remote_device_(remote_device),
      connection_attempt_(std::move(connection_attempt)) {
  connection_attempt_->SetDelegate(this);
}

MessageTransferOperation::ConnectionAttemptDelegate::
    ~ConnectionAttemptDelegate() = default;

void MessageTransferOperation::ConnectionAttemptDelegate::
    OnConnectionAttemptFailure(
        secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  operation_->OnConnectionAttemptFailure(remote_device_, reason);
}

void MessageTransferOperation::ConnectionAttemptDelegate::OnConnection(
    std::unique_ptr<secure_channel::ClientChannel> channel) {
  operation_->OnConnection(remote_device_, std::move(channel));
}

MessageTransferOperation::ClientChannelObserver::ClientChannelObserver(
    MessageTransferOperation* operation,
    multidevice::RemoteDeviceRef remote_device,
    std::unique_ptr<secure_channel::ClientChannel> client_channel)
    : operation_(operation),
      remote_device_(remote_device),
      client_channel_(std::move(client_channel)) {
  client_channel_->AddObserver(this);
}

MessageTransferOperation::ClientChannelObserver::~ClientChannelObserver() {
  client_channel_->RemoveObserver(this);
}

void MessageTransferOperation::ClientChannelObserver::OnDisconnected() {
  operation_->OnDisconnected(remote_device_);
}

void MessageTransferOperation::ClientChannelObserver::OnMessageReceived(
    const std::string& payload) {
  operation_->OnMessageReceived(remote_device_.GetDeviceId(), payload);
}

MessageTransferOperation::MessageTransferOperation(
    const multidevice::RemoteDeviceRefList& devices_to_connect,
    secure_channel::ConnectionPriority connection_priority,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : remote_devices_(RemoveDuplicatesFromVector(devices_to_connect)),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      connection_priority_(connection_priority),
      timer_factory_(std::make_unique<TimerFactory>()) {}

MessageTransferOperation::~MessageTransferOperation() {
  // If initialization never occurred, devices were never registered.
  if (!initialized_)
    return;

  shutting_down_ = true;

  // Unregister any devices that are still registered; otherwise, Bluetooth
  // connections will continue to stay alive until the Tether component is shut
  // down (see crbug.com/761106). Note that a copy of |remote_devices_| is used
  // here because UnregisterDevice() will modify |remote_devices_| internally.
  multidevice::RemoteDeviceRefList remote_devices_copy = remote_devices_;
  for (const auto& remote_device : remote_devices_copy)
    UnregisterDevice(remote_device);
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

  for (const auto& remote_device : remote_devices_) {
    StartConnectionTimerForDevice(remote_device);
    remote_device_to_connection_attempt_delegate_map_[remote_device] =
        std::make_unique<ConnectionAttemptDelegate>(
            this, remote_device,
            secure_channel_client_->ListenForConnectionFromDevice(
                remote_device, *device_sync_client_->GetLocalDeviceMetadata(),
                kTetherFeature, connection_priority_));
  }
}

void MessageTransferOperation::OnMessageReceived(const std::string& device_id,
                                                 const std::string& payload) {
  base::Optional<multidevice::RemoteDeviceRef> remote_device =
      GetRemoteDevice(device_id);
  if (!remote_device) {
    // If the device from which the message has been received does not
    // correspond to any of the devices passed to this MessageTransferOperation
    // instance, ignore the message.
    return;
  }

  std::unique_ptr<MessageWrapper> message_wrapper =
      MessageWrapper::FromRawMessage(payload);
  if (message_wrapper) {
    OnMessageReceived(std::move(message_wrapper), *remote_device);
  }
}

void MessageTransferOperation::UnregisterDevice(
    multidevice::RemoteDeviceRef remote_device) {
  // Note: This function may be called from the destructor. It is invalid to
  // invoke any virtual methods if |shutting_down_| is true.

  // Make a copy of |remote_device| before continuing, since the code below may
  // cause the original reference to be deleted.
  multidevice::RemoteDeviceRef remote_device_copy = remote_device;

  remote_devices_.erase(std::remove(remote_devices_.begin(),
                                    remote_devices_.end(), remote_device_copy),
                        remote_devices_.end());
  StopTimerForDeviceIfRunning(remote_device_copy);

  remote_device_to_connection_attempt_delegate_map_.erase(remote_device);

  if (base::Contains(remote_device_to_client_channel_observer_map_,
                     remote_device)) {
    remote_device_to_client_channel_observer_map_.erase(remote_device);
  }

  if (!shutting_down_ && remote_devices_.empty())
    OnOperationFinished();
}

int MessageTransferOperation::SendMessageToDevice(
    multidevice::RemoteDeviceRef remote_device,
    std::unique_ptr<MessageWrapper> message_wrapper) {
  DCHECK(base::Contains(remote_device_to_client_channel_observer_map_,
                        remote_device));
  int sequence_number = next_message_sequence_number_++;
  bool success =
      remote_device_to_client_channel_observer_map_[remote_device]
          ->channel()
          ->SendMessage(
              message_wrapper->ToRawMessage(),
              base::BindOnce(&MessageTransferOperation::OnMessageSent,
                             weak_ptr_factory_.GetWeakPtr(), sequence_number));
  return success ? sequence_number : -1;
}

uint32_t MessageTransferOperation::GetMessageTimeoutSeconds() {
  return MessageTransferOperation::kDefaultMessageTimeoutSeconds;
}

void MessageTransferOperation::OnConnectionAttemptFailure(
    multidevice::RemoteDeviceRef remote_device,
    secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(WARNING) << "Failed to connect to device "
                  << remote_device.GetTruncatedDeviceIdForLogs()
                  << ", error: " << reason;
  UnregisterDevice(remote_device);
}

void MessageTransferOperation::OnConnection(
    multidevice::RemoteDeviceRef remote_device,
    std::unique_ptr<secure_channel::ClientChannel> channel) {
  remote_device_to_client_channel_observer_map_[remote_device] =
      std::make_unique<ClientChannelObserver>(this, remote_device,
                                              std::move(channel));

  // Stop the timer which was started from StartConnectionTimerForDevice() since
  // the connection has now been established. Start another timer now via
  // StartMessageTimerForDevice() while waiting for messages to be sent to and
  // received by |remote_device|.
  StopTimerForDeviceIfRunning(remote_device);
  StartMessageTimerForDevice(remote_device);

  OnDeviceAuthenticated(remote_device);
}

void MessageTransferOperation::OnDisconnected(
    multidevice::RemoteDeviceRef remote_device) {
  PA_LOG(VERBOSE) << "Remote device disconnected from this device: "
                  << remote_device.GetTruncatedDeviceIdForLogs();
  UnregisterDevice(remote_device);
}

void MessageTransferOperation::StartConnectionTimerForDevice(
    multidevice::RemoteDeviceRef remote_device) {
  StartTimerForDevice(remote_device, kConnectionTimeoutSeconds);
}

void MessageTransferOperation::StartMessageTimerForDevice(
    multidevice::RemoteDeviceRef remote_device) {
  StartTimerForDevice(remote_device, GetMessageTimeoutSeconds());
}

void MessageTransferOperation::StartTimerForDevice(
    multidevice::RemoteDeviceRef remote_device,
    uint32_t timeout_seconds) {
  PA_LOG(VERBOSE) << "Starting timer for operation with message type "
                  << message_type_for_connection_ << " from device with ID "
                  << remote_device.GetTruncatedDeviceIdForLogs() << ".";

  remote_device_to_timer_map_.emplace(remote_device,
                                      timer_factory_->CreateOneShotTimer());
  remote_device_to_timer_map_[remote_device]->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(timeout_seconds),
      base::BindOnce(&MessageTransferOperation::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr(), remote_device));
}

void MessageTransferOperation::StopTimerForDeviceIfRunning(
    multidevice::RemoteDeviceRef remote_device) {
  if (!remote_device_to_timer_map_[remote_device])
    return;

  remote_device_to_timer_map_[remote_device]->Stop();
  remote_device_to_timer_map_.erase(remote_device);
}

void MessageTransferOperation::OnTimeout(
    multidevice::RemoteDeviceRef remote_device) {
  PA_LOG(WARNING) << "Timed out operation for message type "
                  << message_type_for_connection_ << " from device with ID "
                  << remote_device.GetTruncatedDeviceIdForLogs() << ".";

  remote_device_to_timer_map_.erase(remote_device);
  UnregisterDevice(remote_device);
}

base::Optional<multidevice::RemoteDeviceRef>
MessageTransferOperation::GetRemoteDevice(const std::string& device_id) {
  for (auto& remote_device : remote_devices_) {
    if (remote_device.GetDeviceId() == device_id)
      return remote_device;
  }

  return base::nullopt;
}

void MessageTransferOperation::SetTimerFactoryForTest(
    std::unique_ptr<TimerFactory> timer_factory_for_test) {
  timer_factory_ = std::move(timer_factory_for_test);
}

}  // namespace tether

}  // namespace chromeos
