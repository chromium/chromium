// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_nearby_connector.h"

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::secure_channel {

FakeNearbyConnector::FakeConnection::FakeConnection(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingReceiver<mojom::NearbyMessageSender>
        message_sender_pending_receiver,
    mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
        nearby_file_payload_handler_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver>
        message_receiver_pending_remote)
    : bluetooth_public_address_(bluetooth_public_address),
      message_sender_receiver_(this,
                               std::move(message_sender_pending_receiver)),
      file_payload_handler_receiver_(
          this,
          std::move(nearby_file_payload_handler_receiver)),
      message_receiver_remote_(std::move(message_receiver_pending_remote)) {}

FakeNearbyConnector::FakeConnection::~FakeConnection() = default;

void FakeNearbyConnector::FakeConnection::Disconnect() {
  message_sender_receiver_.reset();
  file_payload_handler_receiver_.reset();
  message_receiver_remote_.reset();
  register_payload_file_requests_.clear();
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

FakeNearbyConnector::FakeConnection::RegisterPayloadFileRequest::
    RegisterPayloadFileRequest(
        int64_t payload_id,
        mojo::PendingRemote<mojom::FilePayloadListener> file_payload_listener)
    : payload_id(payload_id),
      file_payload_listener(std::move(file_payload_listener)) {}

FakeNearbyConnector::FakeConnection::RegisterPayloadFileRequest::
    RegisterPayloadFileRequest(RegisterPayloadFileRequest&&) = default;

FakeNearbyConnector::FakeConnection::RegisterPayloadFileRequest&
FakeNearbyConnector::FakeConnection::RegisterPayloadFileRequest::operator=(
    RegisterPayloadFileRequest&&) = default;

FakeNearbyConnector::FakeConnection::RegisterPayloadFileRequest::
    ~RegisterPayloadFileRequest() = default;

void FakeNearbyConnector::FakeConnection::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    mojo::PendingRemote<mojom::FilePayloadListener> listener,
    RegisterPayloadFileCallback callback) {
  register_payload_file_requests_.emplace(
      payload_id, RegisterPayloadFileRequest(payload_id, std::move(listener)));
  std::move(callback).Run(/*success=*/true);
}

void FakeNearbyConnector::FakeConnection::SendFileTransferUpdate(
    int64_t payload_id,
    mojom::FileTransferStatus status,
    uint64_t total_bytes,
    uint64_t bytes_transferred) {
  register_payload_file_requests_.at(payload_id)
      .file_payload_listener->OnFileTransferUpdate(
          mojom::FileTransferUpdate::New(payload_id, status, total_bytes,
                                         bytes_transferred));

  if (status != mojom::FileTransferStatus::kInProgress) {
    register_payload_file_requests_.erase(payload_id);
  }
}

void FakeNearbyConnector::FakeConnection::SendUnexpectedFileTransferUpdate(
    int64_t unexpected_payload_id) {
  register_payload_file_requests_.begin()
      ->second.file_payload_listener->OnFileTransferUpdate(
          mojom::FileTransferUpdate::New(unexpected_payload_id,
                                         mojom::FileTransferStatus::kSuccess,
                                         /*total_bytes=*/1000,
                                         /*bytes_transferred=*/1000));
}

void FakeNearbyConnector::FakeConnection::DisconnectPendingFileTransfers() {
  register_payload_file_requests_.clear();
}

FakeNearbyConnector::ConnectArgs::ConnectArgs(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    ConnectCallback callback)
    : bluetooth_public_address(bluetooth_public_address),
      message_receiver(std::move(message_receiver)),
      nearby_connection_state_listener(
          std::move(nearby_connection_state_listener)),
      callback(std::move(callback)) {}

FakeNearbyConnector::ConnectArgs::~ConnectArgs() = default;

FakeNearbyConnector::FakeNearbyConnector() = default;

FakeNearbyConnector::~FakeNearbyConnector() = default;

void FakeNearbyConnector::FailQueuedCallback() {
  DCHECK(!queued_connect_args_.empty());
  std::move(queued_connect_args_.front()->callback)
      .Run(mojo::NullRemote(), mojo::NullRemote());
  queued_connect_args_.pop();
}

FakeNearbyConnector::FakeConnection*
FakeNearbyConnector::ConnectQueuedCallback() {
  DCHECK(!queued_connect_args_.empty());

  mojo::PendingRemote<mojom::NearbyMessageSender> message_sender_pending_remote;
  mojo::PendingReceiver<mojom::NearbyMessageSender>
      message_sender_pending_receiver =
          message_sender_pending_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
      file_payload_handler_pending_remote;
  mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
      file_payload_handler_pending_receiver =
          file_payload_handler_pending_remote.InitWithNewPipeAndPassReceiver();

  auto fake_connection = std::make_unique<FakeConnection>(
      queued_connect_args_.front()->bluetooth_public_address,
      std::move(message_sender_pending_receiver),
      std::move(file_payload_handler_pending_receiver),
      std::move(queued_connect_args_.front()->message_receiver));
  std::move(queued_connect_args_.front()->callback)
      .Run(std::move(message_sender_pending_remote),
           std::move(file_payload_handler_pending_remote));
  queued_connect_args_.pop();

  FakeConnection* fake_connection_ptr = fake_connection.get();
  fake_connections_.emplace_back(std::move(fake_connection));
  return fake_connection_ptr;
}

void FakeNearbyConnector::Connect(
    const std::vector<uint8_t>& bluetooth_public_address,
    const std::vector<uint8_t>& eid,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    ConnectCallback callback) {
  queued_connect_args_.emplace(std::make_unique<ConnectArgs>(
      bluetooth_public_address, std::move(message_receiver),
      std::move(nearby_connection_state_listener), std::move(callback)));
  std::move(on_connect_closure).Run();
}

}  // namespace ash::secure_channel
