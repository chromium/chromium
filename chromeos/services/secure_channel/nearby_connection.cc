// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/nearby_connection.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/wire_message.h"

namespace chromeos {

namespace secure_channel {

namespace {
const char kBluetoothAddressSeparator[] = ":";
}  // namespace

// static
NearbyConnection::Factory* NearbyConnection::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<Connection> NearbyConnection::Factory::Create(
    multidevice::RemoteDeviceRef remote_device,
    mojom::NearbyConnector* nearby_connector) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(remote_device, nearby_connector);

  return base::WrapUnique(
      new NearbyConnection(remote_device, nearby_connector));
}

// static
void NearbyConnection::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

NearbyConnection::NearbyConnection(multidevice::RemoteDeviceRef remote_device,
                                   mojom::NearbyConnector* nearby_connector)
    : Connection(remote_device), nearby_connector_(nearby_connector) {
  DCHECK(nearby_connector_);
}

NearbyConnection::~NearbyConnection() {
  Disconnect();
}

void NearbyConnection::Connect() {
  SetStatus(Status::IN_PROGRESS);
  nearby_connector_->Connect(GetRemoteDeviceBluetoothAddressAsVector(),
                             message_receiver_.BindNewPipeAndPassRemote(),
                             base::BindOnce(&NearbyConnection::OnConnectResult,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnection::Disconnect() {
  message_sender_.reset();
  message_receiver_.reset();
  SetStatus(Status::DISCONNECTED);
}

std::string NearbyConnection::GetDeviceAddress() {
  return remote_device().bluetooth_public_address();
}

void NearbyConnection::SendMessageImpl(std::unique_ptr<WireMessage> message) {
  queued_messages_to_send_.emplace(std::move(message));
  ProcessQueuedMessagesToSend();
}

void NearbyConnection::OnMessageReceived(const std::string& message) {
  OnBytesReceived(message);
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
    mojo::PendingRemote<mojom::NearbyMessageSender> message_sender) {
  // If a connection failed to be established, disconnect.
  if (!message_sender) {
    PA_LOG(WARNING) << "NearbyConnector returned invalid MessageSender; "
                    << "stopping connection attempt.";
    Disconnect();
    return;
  }

  message_sender_.Bind(std::move(message_sender));

  message_sender_.set_disconnect_handler(
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

}  // namespace secure_channel

}  // namespace chromeos
