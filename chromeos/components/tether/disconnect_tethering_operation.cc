// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/disconnect_tethering_operation.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"

namespace chromeos {

namespace tether {

// static
DisconnectTetheringOperation::Factory*
    DisconnectTetheringOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<DisconnectTetheringOperation>
DisconnectTetheringOperation::Factory::Create(
    multidevice::RemoteDeviceRef device_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        device_to_connect, device_sync_client, secure_channel_client);
  }

  return base::WrapUnique(new DisconnectTetheringOperation(
      device_to_connect, device_sync_client, secure_channel_client));
}

// static
void DisconnectTetheringOperation::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

DisconnectTetheringOperation::Factory::~Factory() = default;

DisconnectTetheringOperation::DisconnectTetheringOperation(
    multidevice::RemoteDeviceRef device_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : MessageTransferOperation(
          multidevice::RemoteDeviceRefList{device_to_connect},
          secure_channel::ConnectionPriority::kHigh,
          device_sync_client,
          secure_channel_client),
      remote_device_(device_to_connect),
      has_sent_message_(false),
      clock_(base::DefaultClock::GetInstance()) {}

DisconnectTetheringOperation::~DisconnectTetheringOperation() = default;

void DisconnectTetheringOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DisconnectTetheringOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DisconnectTetheringOperation::NotifyObserversOperationFinished(
    bool success) {
  for (auto& observer : observer_list_) {
    observer.OnOperationFinished(remote_device_.GetDeviceId(), success);
  }
}

void DisconnectTetheringOperation::OnDeviceAuthenticated(
    multidevice::RemoteDeviceRef remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);

  disconnect_message_sequence_number_ = SendMessageToDevice(
      remote_device,
      std::make_unique<MessageWrapper>(DisconnectTetheringRequest()));
  disconnect_start_time_ = clock_->Now();
}

void DisconnectTetheringOperation::OnOperationFinished() {
  NotifyObserversOperationFinished(has_sent_message_);
}

MessageType DisconnectTetheringOperation::GetMessageTypeForConnection() {
  return MessageType::DISCONNECT_TETHERING_REQUEST;
}

void DisconnectTetheringOperation::OnMessageSent(int sequence_number) {
  if (sequence_number != disconnect_message_sequence_number_)
    return;

  has_sent_message_ = true;

  DCHECK(!disconnect_start_time_.is_null());
  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration",
      clock_->Now() - disconnect_start_time_);
  disconnect_start_time_ = base::Time();

  UnregisterDevice(remote_device_);
}

void DisconnectTetheringOperation::SetClockForTest(
    base::Clock* clock_for_test) {
  clock_ = clock_for_test;
}

}  // namespace tether

}  // namespace chromeos
