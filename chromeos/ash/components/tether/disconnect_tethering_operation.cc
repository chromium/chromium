// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_operation.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash::tether {

// static
DisconnectTetheringOperation::Factory*
    DisconnectTetheringOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<DisconnectTetheringOperation>
DisconnectTetheringOperation::Factory::Create(
    const TetherHost& tether_host,
    raw_ptr<HostConnection::Factory> host_connection_factory) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(tether_host,
                                             host_connection_factory);
  }

  return base::WrapUnique(
      new DisconnectTetheringOperation(tether_host, host_connection_factory));
}

// static
void DisconnectTetheringOperation::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

DisconnectTetheringOperation::Factory::~Factory() = default;

DisconnectTetheringOperation::DisconnectTetheringOperation(
    const TetherHost& tether_host,
    raw_ptr<HostConnection::Factory> host_connection_factory)
    : MessageTransferOperation(
          tether_host,
          HostConnection::Factory::ConnectionPriority::kHigh,
          host_connection_factory),
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
    observer.OnOperationFinished(GetDeviceId(/*truncate_for_logs=*/false),
                                 success);
  }
}

void DisconnectTetheringOperation::OnDeviceAuthenticated() {
  SendMessage(std::make_unique<MessageWrapper>(DisconnectTetheringRequest()),
              base::BindOnce(&DisconnectTetheringOperation::OnMessageSent,
                             weak_ptr_factory_.GetWeakPtr()));
  disconnect_start_time_ = clock_->Now();
}

void DisconnectTetheringOperation::OnOperationFinished() {
  NotifyObserversOperationFinished(has_sent_message_);
}

MessageType DisconnectTetheringOperation::GetMessageTypeForConnection() {
  return MessageType::DISCONNECT_TETHERING_REQUEST;
}

void DisconnectTetheringOperation::OnMessageSent() {
  has_sent_message_ = true;

  DCHECK(!disconnect_start_time_.is_null());
  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration",
      clock_->Now() - disconnect_start_time_);
  disconnect_start_time_ = base::Time();

  StopOperation();
}

void DisconnectTetheringOperation::SetClockForTest(
    base::Clock* clock_for_test) {
  clock_ = clock_for_test;
}

}  // namespace ash::tether
