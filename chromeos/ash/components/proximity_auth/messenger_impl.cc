// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/messenger_impl.h"

#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/messenger_observer.h"
#include "chromeos/ash/components/proximity_auth/remote_status_update.h"

namespace proximity_auth {

namespace {

// The key names of JSON fields for messages sent between the devices.
const char kTypeKey[] = "type";
const char kNameKey[] = "name";
const char kDataKey[] = "data";
const char kEncryptedDataKey[] = "encrypted_data";

// The types of messages that can be sent and received.
const char kMessageTypeLocalEvent[] = "event";
const char kMessageTypeRemoteStatusUpdate[] = "status_update";
const char kMessageTypeDecryptRequest[] = "decrypt_request";
const char kMessageTypeDecryptResponse[] = "decrypt_response";
const char kMessageTypeUnlockRequest[] = "unlock_request";
const char kMessageTypeUnlockResponse[] = "unlock_response";

// The name for an unlock event originating from the local device.
const char kUnlockEventName[] = "easy_unlock";

// Serializes the |value| to a JSON string and returns the result.
std::string SerializeValueToJson(const base::Value::Dict& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

// Returns the message type represented by the |message|. This is a convenience
// wrapper that should only be called when the |message| is known to specify its
// message type, i.e. this should not be called for untrusted input.
std::string GetMessageType(const base::Value::Dict& message) {
  const std::string* type = message.FindString(kTypeKey);
  return type ? *type : std::string();
}

}  // namespace

MessengerImpl::MessengerImpl(
    std::unique_ptr<ash::secure_channel::ClientChannel> channel)
    : channel_(std::move(channel)) {
  DCHECK(!channel_->is_disconnected());
  channel_->AddObserver(this);
}

MessengerImpl::~MessengerImpl() {
  channel_->RemoveObserver(this);
}

void MessengerImpl::AddObserver(MessengerObserver* observer) {
  observers_.AddObserver(observer);
}

void MessengerImpl::RemoveObserver(MessengerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MessengerImpl::DispatchUnlockEvent() {
  base::Value::Dict message;
  message.Set(kTypeKey, kMessageTypeLocalEvent);
  message.Set(kNameKey, kUnlockEventName);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

void MessengerImpl::RequestDecryption(const std::string& challenge) {
  const std::string encrypted_message_data = challenge;
  std::string encrypted_message_data_base64;
  base::Base64UrlEncode(encrypted_message_data,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encrypted_message_data_base64);

  base::Value::Dict message;
  message.Set(kTypeKey, kMessageTypeDecryptRequest);
  message.Set(kEncryptedDataKey, encrypted_message_data_base64);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

void MessengerImpl::RequestUnlock() {
  base::Value::Dict message;
  message.Set(kTypeKey, kMessageTypeUnlockRequest);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

ash::secure_channel::ClientChannel* MessengerImpl::GetChannel() const {
  if (channel_->is_disconnected())
    return nullptr;

  return channel_.get();
}

MessengerImpl::PendingMessage::PendingMessage() = default;

MessengerImpl::PendingMessage::~PendingMessage() = default;

MessengerImpl::PendingMessage::PendingMessage(const base::Value::Dict& message)
    : json_message(SerializeValueToJson(message)),
      type(GetMessageType(message)) {}

MessengerImpl::PendingMessage::PendingMessage(const std::string& message)
    : json_message(message), type(std::string()) {}

void MessengerImpl::ProcessMessageQueue() {
  if (pending_message_ || queued_messages_.empty())
    return;

  if (channel_->is_disconnected())
    return;

  pending_message_ = std::make_unique<PendingMessage>(queued_messages_.front());
  queued_messages_.pop_front();

  channel_->SendMessage(
      pending_message_->json_message,
      base::BindOnce(&MessengerImpl::OnSendMessageResult,
                     weak_ptr_factory_.GetWeakPtr(), true /* success */));
}

void MessengerImpl::HandleRemoteStatusUpdateMessage(
    const base::Value::Dict& message) {
  std::unique_ptr<RemoteStatusUpdate> status_update =
      RemoteStatusUpdate::Deserialize(message);
  if (!status_update) {
    PA_LOG(ERROR) << "Unexpected remote status update: " << message;
    return;
  }

  for (auto& observer : observers_)
    observer.OnRemoteStatusUpdate(*status_update);
}

void MessengerImpl::HandleDecryptResponseMessage(
    const base::Value::Dict& message) {
  const std::string* base64_data = message.FindString(kDataKey);
  std::string decrypted_data;
  if (!base64_data || base64_data->empty()) {
    PA_LOG(ERROR) << "Decrypt response missing '" << kDataKey << "' value.";
  } else if (!base::Base64UrlDecode(
                 *base64_data, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                 &decrypted_data)) {
    PA_LOG(ERROR) << "Unable to base64-decode decrypt response.";
  }

  for (auto& observer : observers_)
    observer.OnDecryptResponse(decrypted_data);
}

void MessengerImpl::HandleUnlockResponseMessage(
    const base::Value::Dict& message) {
  for (auto& observer : observers_)
    observer.OnUnlockResponse(true);
}

void MessengerImpl::OnDisconnected() {
  for (auto& observer : observers_)
    observer.OnDisconnected();
}

void MessengerImpl::OnMessageReceived(const std::string& payload) {
  HandleMessage(payload);
}

void MessengerImpl::HandleMessage(const std::string& message) {
  // The decoded message should be a JSON string.
  absl::optional<base::Value> message_value = base::JSONReader::Read(message);
  if (!message_value || !message_value->is_dict()) {
    PA_LOG(ERROR) << "Unable to parse message as JSON:\n" << message;
    return;
  }

  const base::Value::Dict& message_dictionary = message_value->GetDict();
  const std::string* type = message_dictionary.FindString(kTypeKey);
  if (!type) {
    PA_LOG(ERROR) << "Missing '" << kTypeKey << "' key in message:\n "
                  << message;
    return;
  }

  // Remote status updates can be received out of the blue.
  if (*type == kMessageTypeRemoteStatusUpdate) {
    HandleRemoteStatusUpdateMessage(message_dictionary);
    return;
  }

  // All other messages should only be received in response to a message that
  // the messenger sent.
  if (!pending_message_) {
    PA_LOG(WARNING) << "Unexpected message received: " << message;
    return;
  }

  std::string expected_type;
  if (pending_message_->type == kMessageTypeDecryptRequest)
    expected_type = kMessageTypeDecryptResponse;
  else if (pending_message_->type == kMessageTypeUnlockRequest)
    expected_type = kMessageTypeUnlockResponse;
  else
    NOTREACHED();  // There are no other message types that expect a response.

  if (*type != expected_type) {
    PA_LOG(ERROR) << "Unexpected '" << kTypeKey << "' value in message. "
                  << "Expected '" << expected_type << "' but received '"
                  << *type << "'.";
    return;
  }

  if (*type == kMessageTypeDecryptResponse)
    HandleDecryptResponseMessage(message_dictionary);
  else if (*type == kMessageTypeUnlockResponse)
    HandleUnlockResponseMessage(message_dictionary);
  else
    NOTREACHED();  // There are no other message types that expect a response.

  pending_message_.reset();
  ProcessMessageQueue();
}

void MessengerImpl::OnSendMessageResult(bool success) {
  if (!pending_message_) {
    PA_LOG(ERROR) << "Unexpected message sent.";
    return;
  }

  // In the common case, wait for a response from the remote device.
  // Don't wait if the message could not be sent, as there won't ever be a
  // response in that case. Likewise, don't wait for a response to local
  // event messages, as there is no response for such messages.
  if (success && pending_message_->type != kMessageTypeLocalEvent)
    return;

  // Notify observer of failure if sending the message fails.
  // For local events, we don't expect a response, so on success, we
  // notify observers right away.
  if (pending_message_->type == kMessageTypeDecryptRequest) {
    for (auto& observer : observers_)
      observer.OnDecryptResponse(std::string());
  } else if (pending_message_->type == kMessageTypeUnlockRequest) {
    for (auto& observer : observers_)
      observer.OnUnlockResponse(false);
  } else if (pending_message_->type == kMessageTypeLocalEvent) {
    for (auto& observer : observers_)
      observer.OnUnlockEventSent(success);
  } else {
    PA_LOG(ERROR) << "Message of unknown type '" << pending_message_->type
                  << "' sent.";
  }

  pending_message_.reset();
  ProcessMessageQueue();
}

}  // namespace proximity_auth
