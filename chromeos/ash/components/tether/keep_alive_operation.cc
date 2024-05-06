// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/keep_alive_operation.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash::tether {

// static
KeepAliveOperation::Factory* KeepAliveOperation::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<KeepAliveOperation> KeepAliveOperation::Factory::Create(
    const TetherHost& tether_host,
    raw_ptr<HostConnection::Factory> host_connection_factory) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(tether_host,
                                             host_connection_factory);
  }

  return base::WrapUnique(
      new KeepAliveOperation(tether_host, host_connection_factory));
}

// static
void KeepAliveOperation::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

KeepAliveOperation::Factory::~Factory() = default;

KeepAliveOperation::KeepAliveOperation(
    const TetherHost& tether_host,
    raw_ptr<HostConnection::Factory> host_connection_factory)
    : MessageTransferOperation(
          tether_host,
          HostConnection::Factory::ConnectionPriority::kMedium,
          host_connection_factory),
      clock_(base::DefaultClock::GetInstance()) {}

KeepAliveOperation::~KeepAliveOperation() = default;

void KeepAliveOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void KeepAliveOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeepAliveOperation::OnDeviceAuthenticated() {
  keep_alive_tickle_request_start_time_ = clock_->Now();
  SendMessage(std::make_unique<MessageWrapper>(KeepAliveTickle()),
              /*on_message_sent=*/base::DoNothing());
}

void KeepAliveOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper) {
  if (message_wrapper->GetMessageType() !=
      MessageType::KEEP_ALIVE_TICKLE_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  KeepAliveTickleResponse* response =
      static_cast<KeepAliveTickleResponse*>(message_wrapper->GetProto());
  device_status_ = std::make_unique<DeviceStatus>(response->device_status());

  DCHECK(!keep_alive_tickle_request_start_time_.is_null());
  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.KeepAliveTickleResponseDuration",
      clock_->Now() - keep_alive_tickle_request_start_time_);

  // Now that a response has been received, the device can be unregistered.
  StopOperation();
}

void KeepAliveOperation::OnOperationFinished() {
  for (auto& observer : observer_list_) {
    // Note: If the operation did not complete successfully, |device_status_|
    // will still be null.
    observer.OnOperationFinished(
        device_status_ ? std::make_unique<DeviceStatus>(*device_status_)
                       : nullptr);
  }
}

MessageType KeepAliveOperation::GetMessageTypeForConnection() {
  return MessageType::KEEP_ALIVE_TICKLE;
}

void KeepAliveOperation::SetClockForTest(base::Clock* clock_for_test) {
  clock_ = clock_for_test;
}

}  // namespace ash::tether
