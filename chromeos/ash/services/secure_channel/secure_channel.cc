// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/secure_channel.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"

namespace ash::secure_channel {

// static
SecureChannel::Factory* SecureChannel::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<SecureChannel> SecureChannel::Factory::Create(
    std::unique_ptr<Connection> connection) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(std::move(connection));

  return base::WrapUnique(new SecureChannel(std::move(connection)));
}

// static
void SecureChannel::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

// static
std::string SecureChannel::StatusToString(const Status& status) {
  switch (status) {
    case Status::DISCONNECTED:
      return "[disconnected]";
    case Status::CONNECTING:
      return "[connecting]";
    case Status::CONNECTED:
      return "[connected]";
    case Status::AUTHENTICATING:
      return "[authenticating]";
    case Status::AUTHENTICATED:
      return "[authenticated]";
    case Status::DISCONNECTING:
      return "[disconnecting]";
    default:
      return "[unknown status]";
  }
}

SecureChannel::PendingMessage::PendingMessage(const std::string& feature,
                                              const std::string& payload,
                                              int sequence_number)
    : feature(feature), payload(payload), sequence_number(sequence_number) {}

SecureChannel::PendingMessage::~PendingMessage() {}

SecureChannel::SecureChannel(std::unique_ptr<Connection> connection)
    : status_(Status::DISCONNECTED), connection_(std::move(connection)) {
  connection_->AddObserver(this);
  connection_->AddNearbyConnectionObserver(this);
}

SecureChannel::~SecureChannel() {
  connection_->RemoveObserver(this);
}

void SecureChannel::Initialize() {
  DCHECK(status_ == Status::DISCONNECTED);
  connection_->Connect();
  TransitionToStatus(Status::CONNECTING);
}

int SecureChannel::SendMessage(const std::string& feature,
                               const std::string& payload) {
  DCHECK(status_ == Status::AUTHENTICATED);

  int sequence_number = next_sequence_number_;
  next_sequence_number_++;

  queued_messages_.emplace(
      std::make_unique<PendingMessage>(feature, payload, sequence_number));
  ProcessMessageQueue();

  return sequence_number;
}

void SecureChannel::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  DCHECK(status_ == Status::AUTHENTICATED);
  connection_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                   std::move(file_transfer_update_callback),
                                   std::move(registration_result_callback));
}

void SecureChannel::Disconnect() {
  if (connection_->IsConnected()) {
    TransitionToStatus(Status::DISCONNECTING);

    // If |connection_| is active, calling Disconnect() will eventually cause
    // its status to transition to DISCONNECTED, which will in turn cause this
    // class to transition to DISCONNECTED.
    connection_->Disconnect();
    return;
  }

  TransitionToStatus(Status::DISCONNECTED);
}

void SecureChannel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SecureChannel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SecureChannel::GetConnectionRssi(
    base::OnceCallback<void(std::optional<int32_t>)> callback) {
  if (!connection_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  connection_->GetConnectionRssi(std::move(callback));
}

std::optional<std::string> SecureChannel::GetChannelBindingData() {
  if (secure_context_)
    return secure_context_->GetChannelBindingData();

  return std::nullopt;
}

void SecureChannel::OnConnectionStatusChanged(Connection* connection,
                                              Connection::Status old_status,
                                              Connection::Status new_status) {
  DCHECK(connection == connection_.get());

  if (new_status == Connection::Status::CONNECTED) {
    TransitionToStatus(Status::CONNECTED);

    // Once the connection has succeeded, authenticate the connection by
    // initiating the security handshake.
    Authenticate();
    return;
  }

  if (new_status == Connection::Status::DISCONNECTED) {
    // If the connection is no longer active, disconnect.
    Disconnect();
    return;
  }
}

void SecureChannel::OnMessageReceived(const Connection& connection,
                                      const WireMessage& wire_message) {
  DCHECK(&connection == const_cast<const Connection*>(connection_.get()));
  if (wire_message.feature() == Authenticator::kAuthenticationFeature) {
    // If the message received was part of the authentication handshake, it
    // is a low-level message and should not be forwarded to observers.
    return;
  }

  if (!secure_context_) {
    PA_LOG(WARNING) << "Received unexpected message before authentication "
                    << "was complete. Feature: " << wire_message.feature()
                    << ", Payload size: " << wire_message.payload().size()
                    << " byte(s)";
    return;
  }

  secure_context_->DecodeAndDequeue(
      wire_message.payload(),
      base::BindRepeating(&SecureChannel::OnMessageDecoded,
                          weak_ptr_factory_.GetWeakPtr(),
                          wire_message.feature()));
}

void SecureChannel::OnSendCompleted(const Connection& connection,
                                    const WireMessage& wire_message,
                                    bool success) {
  if (wire_message.feature() == Authenticator::kAuthenticationFeature) {
    // No need to process authentication messages; these are handled by
    // |authenticator_|.
    return;
  }

  if (!pending_message_) {
    PA_LOG(ERROR) << "OnSendCompleted(), but a send was not expected to be in "
                  << "progress. Disconnecting from "
                  << connection_->GetDeviceAddress();
    Disconnect();
    return;
  }

  if (success && status_ != Status::DISCONNECTED) {
    pending_message_.reset();

    // Create a WeakPtr to |this| before invoking observer callbacks. It is
    // possible that an Observer will respond to the OnMessageSent() call by
    // destroying the connection (e.g., if the client only wanted to send one
    // message and destroyed the connection after the message was sent).
    base::WeakPtr<SecureChannel> weak_this = weak_ptr_factory_.GetWeakPtr();

    if (wire_message.sequence_number() != -1) {
      for (auto& observer : observer_list_)
        observer.OnMessageSent(this, wire_message.sequence_number());
    }

    // Process the next message if possible. Note that if the SecureChannel was
    // deleted by the OnMessageSent() callback, this will be a no-op since
    // |weak_this| will have been invalidated in that case.
    if (weak_this.get())
      weak_this->ProcessMessageQueue();

    return;
  }

  PA_LOG(ERROR) << "Could not send message: {"
                << "payload size: " << pending_message_->payload.size()
                << " byte(s), feature: \"" << pending_message_->feature << "\""
                << "}";
  pending_message_.reset();

  // The connection automatically retries failed messages, so if |success| is
  // |false| here, a fatal error has occurred. Thus, there is no need to retry
  // the message; instead, disconnect.
  Disconnect();
}

void SecureChannel::OnNearbyConnectionStateChagned(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  for (auto& observer : observer_list_) {
    observer.OnNearbyConnectionStateChanged(this, step, result);
  }
}

void SecureChannel::OnAuthenticationStateChanged(
    mojom::SecureChannelState secure_channel_state) {
  for (auto& observer : observer_list_) {
    observer.OnSecureChannelAuthenticationStateChanged(this,
                                                       secure_channel_state);
  }
}

void SecureChannel::TransitionToStatus(const Status& new_status) {
  if (new_status == status_) {
    // Only report changes to state.
    return;
  }

  Status old_status = status_;
  status_ = new_status;

  for (auto& observer : observer_list_)
    observer.OnSecureChannelStatusChanged(this, old_status, status_);
}

void SecureChannel::Authenticate() {
  DCHECK(status_ == Status::CONNECTED);
  DCHECK(!authenticator_);

  authenticator_ = DeviceToDeviceAuthenticator::Factory::Create(
      connection_.get(),
      multidevice::SecureMessageDelegateImpl::Factory::Create());
  authenticator_->AddObserver(this);
  authenticator_->Authenticate(base::BindOnce(
      &SecureChannel::OnAuthenticationResult, weak_ptr_factory_.GetWeakPtr()));

  TransitionToStatus(Status::AUTHENTICATING);
}

void SecureChannel::ProcessMessageQueue() {
  if (pending_message_ || queued_messages_.empty()) {
    return;
  }

  DCHECK(!connection_->is_sending_message());

  pending_message_ = std::move(queued_messages_.front());
  queued_messages_.pop();

  PA_LOG(INFO) << "Sending message to " << connection_->GetDeviceAddress()
               << ": {"
               << "feature: \"" << pending_message_->feature << "\", "
               << "payload size: " << pending_message_->payload.size()
               << " byte(s)"
               << "}";

  secure_context_->Encode(
      pending_message_->payload,
      base::BindOnce(&SecureChannel::OnMessageEncoded,
                     weak_ptr_factory_.GetWeakPtr(), pending_message_->feature,
                     pending_message_->sequence_number));
}

void SecureChannel::OnMessageEncoded(const std::string& feature,
                                     int sequence_number,
                                     const std::string& encoded_message) {
  connection_->SendMessage(
      std::make_unique<WireMessage>(encoded_message, feature, sequence_number));
}

void SecureChannel::OnMessageDecoded(const std::string& feature,
                                     const std::string& decoded_message) {
  PA_LOG(VERBOSE) << "Received message from " << connection_->GetDeviceAddress()
                  << ": {"
                  << "feature: \"" << feature << "\", "
                  << "payload size: " << decoded_message.size() << " byte(s)"
                  << "}";

  for (auto& observer : observer_list_)
    observer.OnMessageReceived(this, feature, decoded_message);
}

void SecureChannel::OnAuthenticationResult(
    Authenticator::Result result,
    std::unique_ptr<SecureContext> secure_context) {
  DCHECK(status_ == Status::AUTHENTICATING);

  // The authenticator is no longer needed, so release it.
  authenticator_->RemoveObserver(this);
  authenticator_.reset();

  if (result != Authenticator::Result::SUCCESS) {
    PA_LOG(WARNING)
        << "Failed to authenticate connection to device with ID "
        << connection_->remote_device().GetTruncatedDeviceIdForLogs();
    Disconnect();
    return;
  }

  secure_context_ = std::move(secure_context);
  TransitionToStatus(Status::AUTHENTICATED);
}

}  // namespace ash::secure_channel
