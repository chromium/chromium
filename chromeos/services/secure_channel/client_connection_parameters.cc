// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/client_connection_parameters.h"

#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace secure_channel {

ClientConnectionParameters::ClientConnectionParameters(
    const std::string& feature)
    : feature_(feature), id_(base::UnguessableToken::Create()) {
  DCHECK(!feature_.empty());
}

ClientConnectionParameters::~ClientConnectionParameters() = default;

void ClientConnectionParameters::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ClientConnectionParameters::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ClientConnectionParameters::IsClientWaitingForResponse() {
  return !has_invoked_delegate_function_ && !HasClientCanceledRequest();
}

void ClientConnectionParameters::SetConnectionAttemptFailed(
    mojom::ConnectionAttemptFailureReason reason) {
  static const std::string kFunctionName = "SetConnectionAttemptFailed";
  VerifyDelegateWaitingForResponse(kFunctionName);
  has_invoked_delegate_function_ = true;
  PerformSetConnectionAttemptFailed(reason);
}

void ClientConnectionParameters::SetConnectionSucceeded(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver) {
  static const std::string kFunctionName = "SetConnectionSucceeded";
  VerifyDelegateWaitingForResponse(kFunctionName);
  has_invoked_delegate_function_ = true;
  PerformSetConnectionSucceeded(std::move(channel),
                                std::move(message_receiver_receiver));
}

bool ClientConnectionParameters::operator==(
    const ClientConnectionParameters& other) const {
  return id() == other.id();
}

bool ClientConnectionParameters::operator<(
    const ClientConnectionParameters& other) const {
  return id() < other.id();
}

void ClientConnectionParameters::NotifyConnectionRequestCanceled() {
  for (auto& observer : observer_list_)
    observer.OnConnectionRequestCanceled();
}

void ClientConnectionParameters::VerifyDelegateWaitingForResponse(
    const std::string& function_name) {
  if (has_invoked_delegate_function_) {
    PA_LOG(ERROR) << "ClientConnectionParameters::" << function_name << "(): "
                  << "Attempted to notify ConnectionDelegate when a delegate "
                  << "function had already been invoked. Cannot proceed.";
    NOTREACHED();
  }

  if (HasClientCanceledRequest()) {
    PA_LOG(ERROR) << "ClientConnectionParameters::" << function_name << "(): "
                  << "Attempted to notify ConnectionDelegate when the client "
                  << "had already canceled the connection. Cannot proceed.";
    NOTREACHED();
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const ClientConnectionParameters& details) {
  stream << "{feature: \"" << details.feature() << "\", "
         << "id: \"" << details.id().ToString() << "\"}";
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
