// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_host_connection.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

FakeHostConnection::Factory::Factory() = default;
FakeHostConnection::Factory::~Factory() = default;

void FakeHostConnection::Factory::ScanForTetherHostAndCreateConnection(
    const std::string& device_id,
    ConnectionPriority connection_priority,
    raw_ptr<PayloadListener> payload_listener,
    OnDisconnectionCallback on_disconnection,
    OnConnectionCreatedCallback on_connection_attempt_finished) {
  if (base::Contains(pending_connection_attempts_, device_id)) {
    if (pending_connection_attempts_.at(device_id)) {
      pending_connection_attempts_.at(device_id)->set_payload_listener(
          payload_listener);
      pending_connection_attempts_.at(device_id)->set_on_disconnection(
          std::move(on_disconnection));
    }

    active_connections_.emplace(
        device_id, pending_connection_attempts_.at(device_id).get());

    std::move(on_connection_attempt_finished)
        .Run(std::move(pending_connection_attempts_.at(device_id)));
    pending_connection_attempts_.erase(device_id);
  }
}

void FakeHostConnection::Factory::Create(
    const TetherHost& tether_host,
    ConnectionPriority connection_priority,
    raw_ptr<PayloadListener> payload_listener,
    OnDisconnectionCallback on_disconnection,
    OnConnectionCreatedCallback on_connection_attempt_finished) {
  ScanForTetherHostAndCreateConnection(
      tether_host.GetDeviceId(), connection_priority, payload_listener,
      std::move(on_disconnection), std::move(on_connection_attempt_finished));
}

void FakeHostConnection::Factory::FailConnectionAttempt(
    const TetherHost& tether_host) {
  pending_connection_attempts_.insert_or_assign(tether_host.GetDeviceId(),
                                                nullptr);
}

void FakeHostConnection::Factory::SetupConnectionAttempt(
    const TetherHost& tether_host) {
  pending_connection_attempts_.insert_or_assign(
      tether_host.GetDeviceId(),
      std::make_unique<FakeHostConnection>(base::BindOnce(
          &FakeHostConnection::Factory::OnConnectionDestroyed,
          weak_ptr_factory_.GetWeakPtr(), tether_host.GetDeviceId())));
}

FakeHostConnection* FakeHostConnection::Factory::GetPendingConnectionAttempt(
    const std::string& device_id) {
  if (!base::Contains(pending_connection_attempts_, device_id)) {
    return nullptr;
  }

  return pending_connection_attempts_.at(device_id).get();
}

FakeHostConnection* FakeHostConnection::Factory::GetActiveConnection(
    const std::string& device_id) {
  if (!base::Contains(active_connections_, device_id)) {
    return nullptr;
  }

  return active_connections_.at(device_id);
}

void FakeHostConnection::Factory::OnConnectionDestroyed(
    const std::string& device_id) {
  active_connections_.erase(device_id);
}

FakeHostConnection::FakeHostConnection(base::OnceClosure on_destroyed)
    : HostConnection(
          /*payload_listener=*/nullptr,
          /*on_disconnection=*/base::DoNothing()),
      on_destroyed_(std::move(on_destroyed)) {}

FakeHostConnection::~FakeHostConnection() {
  std::move(on_destroyed_).Run();
}

void FakeHostConnection::SendMessage(
    std::unique_ptr<MessageWrapper> message,
    OnMessageSentCallback on_message_sent_callback) {
  sent_messages_.emplace_back(std::move(message),
                              std::move(on_message_sent_callback));
}

void FakeHostConnection::ReceiveMessage(
    std::unique_ptr<MessageWrapper> message) {
  payload_listener_->OnMessageReceived(std::move(message));
}

void FakeHostConnection::Close() {
  std::move(on_disconnection_).Run();
}

void FakeHostConnection::FinishSendingMessages() {
  for (auto& message : sent_messages_) {
    std::move(message.second).Run();
  }
}

}  // namespace ash::tether
