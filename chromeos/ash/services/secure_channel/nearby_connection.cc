// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection.h"

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::secure_channel {

namespace {

const char kBluetoothAddressSeparator[] = ":";

}  // namespace

// static
NearbyConnection::Factory* NearbyConnection::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<Connection> NearbyConnection::Factory::Create(
    multidevice::RemoteDeviceRef remote_device,
    const std::vector<uint8_t>& eid,
    mojom::NearbyConnector* nearby_connector) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(remote_device, eid,
                                             nearby_connector);

  return base::WrapUnique(
      new NearbyConnection(remote_device, eid, nearby_connector));
}

// static
void NearbyConnection::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

NearbyConnection::NearbyConnection(multidevice::RemoteDeviceRef remote_device,
                                   const std::vector<uint8_t>& eid,
                                   mojom::NearbyConnector* nearby_connector)
    : Connection(remote_device),
      nearby_connector_(nearby_connector),
      eid_(eid) {
  DCHECK(nearby_connector_);
  file_payload_listener_receivers_.set_disconnect_handler(base::BindRepeating(
      &NearbyConnection::OnFilePayloadListenerRemoteDisconnected,
      base::Unretained(this)));
}

NearbyConnection::~NearbyConnection() {
  Disconnect();
}

void NearbyConnection::Connect() {
  SetStatus(Status::IN_PROGRESS);
  nearby_connector_->Connect(
      GetRemoteDeviceBluetoothAddressAsVector(), eid_,
      message_receiver_.BindNewPipeAndPassRemote(),
      nearby_connection_state_listener_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyConnection::OnConnectResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnection::Disconnect() {
  message_sender_.reset();
  message_receiver_.reset();
  nearby_connection_state_listener_.reset();
  file_payload_handler_.reset();
  CleanUpPendingFileTransfersOnDisconnect();
  SetStatus(Status::DISCONNECTED);
}

std::string NearbyConnection::GetDeviceAddress() {
  return remote_device().bluetooth_public_address();
}

void NearbyConnection::SendMessageImpl(std::unique_ptr<WireMessage> message) {
  queued_messages_to_send_.emplace(std::move(message));
  ProcessQueuedMessagesToSend();
}

void NearbyConnection::RegisterPayloadFileImpl(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  mojo::PendingRemote<mojom::FilePayloadListener>
      file_payload_listener_pending_remote;
  file_payload_listener_receivers_.Add(
      this,
      file_payload_listener_pending_remote.InitWithNewPipeAndPassReceiver(),
      /*context=*/payload_id);
  file_transfer_update_callbacks_.emplace(
      payload_id, std::move(file_transfer_update_callback));

  file_payload_handler_->RegisterPayloadFile(
      payload_id, std::move(payload_files),
      std::move(file_payload_listener_pending_remote),
      std::move(registration_result_callback));
}

void NearbyConnection::OnMessageReceived(const std::string& message) {
  OnBytesReceived(message);
}

void NearbyConnection::OnNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  SetNearbyConnectionSubStatus(step, result);
}

void NearbyConnection::OnFileTransferUpdate(
    mojom::FileTransferUpdatePtr update) {
  auto it = file_transfer_update_callbacks_.find(update->payload_id);
  if (it == file_transfer_update_callbacks_.end()) {
    PA_LOG(WARNING) << "Received transfer update for unregistered file payload "
                    << update->payload_id;
    Disconnect();
    return;
  }

  bool is_transfer_complete =
      update->status != mojom::FileTransferStatus::kInProgress;
  it->second.Run(std::move(update));
  if (is_transfer_complete) {
    file_transfer_update_callbacks_.erase(it);
  }
}

void NearbyConnection::CleanUpPendingFileTransfersOnDisconnect() {
  // Notify clients of uncompleted file transfers and clean up callbacks and
  // corresponding FilePayloadListener receivers when the connection
  // drops.
  for (auto& id_to_callback : file_transfer_update_callbacks_) {
    id_to_callback.second.Run(mojom::FileTransferUpdate::New(
        id_to_callback.first, mojom::FileTransferStatus::kCanceled,
        /*total_bytes=*/0, /*bytes_transferred=*/0));
  }
  file_transfer_update_callbacks_.clear();
  file_payload_listener_receivers_.Clear();
}

void NearbyConnection::OnFilePayloadListenerRemoteDisconnected() {
  int64_t payload_id = file_payload_listener_receivers_.current_context();
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

std::vector<uint8_t>
NearbyConnection::GetRemoteDeviceBluetoothAddressAsVector() {
  std::vector<std::string> hex_bytes_as_strings =
      base::SplitString(GetDeviceAddress(), kBluetoothAddressSeparator,
                        base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_ALL);

  std::vector<uint8_t> bytes;
  for (const std::string& hex_bytes_as_string : hex_bytes_as_strings) {
    int byte_value;
    base::HexStringToInt(hex_bytes_as_string, &byte_value);
    bytes.push_back(static_cast<uint8_t>(byte_value));
  }

  return bytes;
}

void NearbyConnection::OnConnectResult(
    mojo::PendingRemote<mojom::NearbyMessageSender> message_sender,
    mojo::PendingRemote<mojom::NearbyFilePayloadHandler> file_payload_handler) {
  if (!message_sender) {
    PA_LOG(WARNING) << "NearbyConnector returned invalid MessageSender; "
                    << "stopping connection attempt.";
    Disconnect();
    return;
  }
  if (!file_payload_handler) {
    PA_LOG(WARNING) << "NearbyConnector returned invalid FilePayloadHandler; "
                    << "stopping connection attempt.";
    Disconnect();
    return;
  }

  message_sender_.Bind(std::move(message_sender));
  file_payload_handler_.Bind(std::move(file_payload_handler));

  message_sender_.set_disconnect_handler(
      base::BindOnce(&NearbyConnection::Disconnect, base::Unretained(this)));
  file_payload_handler_.set_disconnect_handler(
      base::BindOnce(&NearbyConnection::Disconnect, base::Unretained(this)));
  message_receiver_.set_disconnect_handler(
      base::BindOnce(&NearbyConnection::Disconnect, base::Unretained(this)));

  SetStatus(Status::CONNECTED);
}

void NearbyConnection::OnSendMessageResult(bool success) {
  OnDidSendMessage(*message_being_sent_, success);

  if (success) {
    message_being_sent_.reset();
    ProcessQueuedMessagesToSend();
    return;
  }

  // Failing to send a message is a fatal error; disconnect.
  PA_LOG(WARNING) << "Sending message failed; disconnecting.";
  Disconnect();
}

void NearbyConnection::ProcessQueuedMessagesToSend() {
  // Message is already being sent.
  if (message_being_sent_)
    return;

  // No pending messages to send.
  if (queued_messages_to_send_.empty())
    return;

  message_being_sent_ = std::move(queued_messages_to_send_.front());
  queued_messages_to_send_.pop();

  message_sender_->SendMessage(
      message_being_sent_->Serialize(),
      base::BindOnce(&NearbyConnection::OnSendMessageResult,
                     base::Unretained(this)));
}

}  // namespace ash::secure_channel
