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
    FakeHostConnection* fake_host_connection =
        pending_connection_attempts_[device_id];
    if (fake_host_connection) {
      fake_host_connection->set_payload_listener(payload_listener);
      fake_host_connection->set_on_disconnection(std::move(on_disconnection));
    }

    std::unique_ptr<HostConnection> host_connection =
        base::WrapUnique<HostConnection>(fake_host_connection);
    pending_connection_attempts_.erase(device_id);
    std::move(on_connection_attempt_finished).Run(std::move(host_connection));
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

FakeHostConnection* FakeHostConnection::Factory::SetupConnectionAttempt(
    const TetherHost& tether_host) {
  auto* fake_host_connection = new FakeHostConnection();
  pending_connection_attempts_[tether_host.GetDeviceId()] =
      fake_host_connection;
  return fake_host_connection;
}

void FakeHostConnection::Factory::SetupConnectionAttempt(
    const std::string& device_id,
    FakeHostConnection* host_connection) {
  EXPECT_FALSE(base::Contains(pending_connection_attempts_, device_id));
  pending_connection_attempts_[device_id] = host_connection;
}

FakeHostConnection* FakeHostConnection::Factory::GetPendingConnectionAttempt(
    const std::string& device_id) {
  if (!base::Contains(pending_connection_attempts_, device_id)) {
    return nullptr;
  }

  return pending_connection_attempts_[device_id];
}

FakeHostConnection::FakeHostConnection()
    : HostConnection(
          /*payload_listener=*/nullptr,
          /*on_disconnection=*/base::DoNothing()) {}

FakeHostConnection::~FakeHostConnection() = default;

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
