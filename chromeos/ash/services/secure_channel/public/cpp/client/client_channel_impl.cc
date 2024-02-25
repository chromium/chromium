// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel_impl.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::secure_channel {

// static
ClientChannelImpl::Factory* ClientChannelImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ClientChannel> ClientChannelImpl::Factory::Create(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
    mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener_receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(channel),
                                         std::move(message_receiver_receiver));
  }

  return base::WrapUnique(new ClientChannelImpl(
      std::move(channel), std::move(message_receiver_receiver),
      std::move(nearby_connection_state_listener_receiver)));
}

// static
void ClientChannelImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

ClientChannelImpl::Factory::~Factory() = default;

ClientChannelImpl::ClientChannelImpl(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
    mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener_receiver)
    : channel_(std::move(channel)),
      receiver_(this, std::move(message_receiver_receiver)),
      nearby_connection_state_listener_receiver_(
          this,
          std::move(nearby_connection_state_listener_receiver)) {
  channel_.set_disconnect_with_reason_handler(
      base::BindOnce(&ClientChannelImpl::OnChannelDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  file_payload_listeners_.set_disconnect_handler(base::BindRepeating(
      &ClientChannelImpl::OnFilePayloadListenerRemoteDisconnected,
      base::Unretained(this)));
}

ClientChannelImpl::~ClientChannelImpl() {
  CleanUpPendingFileTransfers();
}

void ClientChannelImpl::PerformGetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  channel_->GetConnectionMetadata(
      base::BindOnce(&ClientChannelImpl::OnGetConnectionMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientChannelImpl::OnMessageReceived(const std::string& message) {
  NotifyMessageReceived(message);
}

void ClientChannelImpl::OnNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  NotifyNearbyConnectionStateChanged(step, result);
}

void ClientChannelImpl::OnGetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback,
    mojom::ConnectionMetadataPtr connection_metadata_ptr) {
  std::move(callback).Run(std::move(connection_metadata_ptr));
}

void ClientChannelImpl::PerformSendMessage(const std::string& payload,
                                           base::OnceClosure on_sent_callback) {
  channel_->SendMessage(payload, std::move(on_sent_callback));
}

void ClientChannelImpl::PerformRegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
        file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  mojo::PendingRemote<mojom::FilePayloadListener>
      file_payload_listener_pending_remote;
  file_payload_listeners_.Add(
      this,
      file_payload_listener_pending_remote.InitWithNewPipeAndPassReceiver(),
      /*context=*/payload_id);
  file_transfer_update_callbacks_.emplace(
      payload_id, std::move(file_transfer_update_callback));

  channel_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                std::move(file_payload_listener_pending_remote),
                                std::move(registration_result_callback));
}

void ClientChannelImpl::OnFileTransferUpdate(
    mojom::FileTransferUpdatePtr update) {
  DCHECK_EQ(update->payload_id, file_payload_listeners_.current_context());

  auto it = file_transfer_update_callbacks_.find(update->payload_id);
  if (it == file_transfer_update_callbacks_.end()) {
    LOG(ERROR) << "Received unexpected file transfer update for payload "
               << update->payload_id
               << " after the transfer has already been completed";
    return;
  }

  bool is_transfer_complete =
      update->status != mojom::FileTransferStatus::kInProgress;
  it->second.Run(std::move(update));
  if (is_transfer_complete) {
    file_transfer_update_callbacks_.erase(it);
  }
}

void ClientChannelImpl::CleanUpPendingFileTransfers() {
  // Notify clients of uncompleted file transfers and clean up callbacks and
  // corresponding FilePayloadListener receivers when the connection
  // drops.
  for (auto& id_to_callback : file_transfer_update_callbacks_) {
    id_to_callback.second.Run(mojom::FileTransferUpdate::New(
        id_to_callback.first, mojom::FileTransferStatus::kCanceled,
        /*total_bytes=*/0, /*bytes_transferred=*/0));
  }
  file_transfer_update_callbacks_.clear();
  file_payload_listeners_.Clear();
}

void ClientChannelImpl::OnFilePayloadListenerRemoteDisconnected() {
  int64_t payload_id = file_payload_listeners_.current_context();
  auto it = file_transfer_update_callbacks_.find(payload_id);
  if (it != file_transfer_update_callbacks_.end()) {
    // If the file transfer update callback hasn't been removed by the time the
    // corresponding Remote endpoint disconnects, the transfer for this payload
    // hasn't completed yet and we need to send a cancelation update.
    it->second.Run(mojom::FileTransferUpdate::New(
        payload_id, mojom::FileTransferStatus::kCanceled,
        /*total_bytes=*/0, /*bytes_transferred=*/0));
    file_transfer_update_callbacks_.erase(it);
  }
}

void ClientChannelImpl::OnChannelDisconnected(
    uint32_t disconnection_reason,
    const std::string& disconnection_description) {
  if (disconnection_reason != mojom::Channel::kConnectionDroppedReason) {
    LOG(ERROR) << "Received unexpected disconnection reason: "
               << disconnection_description;
  }

  channel_.reset();
  receiver_.reset();
  CleanUpPendingFileTransfers();
  NotifyDisconnected();
}

void ClientChannelImpl::FlushForTesting() {
  channel_.FlushForTesting();
  file_payload_listeners_.FlushForTesting();
}

}  // namespace ash::secure_channel
