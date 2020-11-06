// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/fake_nearby_connector.h"

namespace chromeos {
namespace secure_channel {

FakeNearbyConnector::FakeConnection::FakeConnection(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingReceiver<mojom::NearbyMessageSender>
        message_sender_pending_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver>
        message_receiver_pending_remote)
    : bluetooth_public_address_(bluetooth_public_address),
      message_sender_receiver_(this,
                               std::move(message_sender_pending_receiver)),
      message_receiver_remote_(std::move(message_receiver_pending_remote)) {}

FakeNearbyConnector::FakeConnection::~FakeConnection() = default;

void FakeNearbyConnector::FakeConnection::Disconnect() {
  message_sender_receiver_.reset();
  message_receiver_remote_.reset();
}

void FakeNearbyConnector::FakeConnection::ReceiveMessage(
    const std::string& message) {
  message_receiver_remote_->OnMessageReceived(message);
}

void FakeNearbyConnector::FakeConnection::SendMessage(
    const std::string& message,
    SendMessageCallback callback) {
  sent_messages_.push_back(message);
  std::move(callback).Run(should_send_succeed_);
}

FakeNearbyConnector::ConnectArgs::ConnectArgs(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
    ConnectCallback callback)
    : bluetooth_public_address(bluetooth_public_address),
      message_receiver(std::move(message_receiver)),
      callback(std::move(callback)) {}

FakeNearbyConnector::ConnectArgs::~ConnectArgs() = default;

FakeNearbyConnector::FakeNearbyConnector() = default;

FakeNearbyConnector::~FakeNearbyConnector() = default;

void FakeNearbyConnector::FailQueuedCallback() {
  DCHECK(!queued_connect_args_.empty());
  std::move(queued_connect_args_.front()->callback).Run(mojo::NullRemote());
  queued_connect_args_.pop();
}

FakeNearbyConnector::FakeConnection*
FakeNearbyConnector::ConnectQueuedCallback() {
  DCHECK(!queued_connect_args_.empty());

  mojo::PendingRemote<mojom::NearbyMessageSender> message_sender_pending_remote;
  mojo::PendingReceiver<mojom::NearbyMessageSender>
      message_sender_pending_receiver =
          message_sender_pending_remote.InitWithNewPipeAndPassReceiver();

  auto fake_connection = std::make_unique<FakeConnection>(
      queued_connect_args_.front()->bluetooth_public_address,
      std::move(message_sender_pending_receiver),
      std::move(queued_connect_args_.front()->message_receiver));
  std::move(queued_connect_args_.front()->callback)
      .Run(std::move(message_sender_pending_remote));
  queued_connect_args_.pop();

  FakeConnection* fake_connection_ptr = fake_connection.get();
  fake_connections_.emplace_back(std::move(fake_connection));
  return fake_connection_ptr;
}

void FakeNearbyConnector::Connect(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
    ConnectCallback callback) {
  queued_connect_args_.emplace(std::make_unique<ConnectArgs>(
      bluetooth_public_address, std::move(message_receiver),
      std::move(callback)));
  std::move(on_connect_closure).Run();
}

}  // namespace secure_channel
}  // namespace chromeos
