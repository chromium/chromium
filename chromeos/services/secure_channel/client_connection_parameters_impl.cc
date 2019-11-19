// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/client_connection_parameters_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
ClientConnectionParametersImpl::Factory*
    ClientConnectionParametersImpl::Factory::test_factory_ = nullptr;

// static
ClientConnectionParametersImpl::Factory*
ClientConnectionParametersImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void ClientConnectionParametersImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ClientConnectionParametersImpl::Factory::~Factory() = default;

std::unique_ptr<ClientConnectionParameters>
ClientConnectionParametersImpl::Factory::BuildInstance(
    const std::string& feature,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote) {
  return base::WrapUnique(new ClientConnectionParametersImpl(
      feature, std::move(connection_delegate_remote)));
}

ClientConnectionParametersImpl::ClientConnectionParametersImpl(
    const std::string& feature,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote)
    : ClientConnectionParameters(feature),
      connection_delegate_remote_(std::move(connection_delegate_remote)) {
  // If the client disconnects its delegate, the client is signaling that the
  // connection request has been canceled.
  connection_delegate_remote_.set_disconnect_handler(base::BindOnce(
      &ClientConnectionParametersImpl::OnConnectionDelegateRemoteDisconnected,
      base::Unretained(this)));
}

ClientConnectionParametersImpl::~ClientConnectionParametersImpl() = default;

bool ClientConnectionParametersImpl::HasClientCanceledRequest() {
  return !connection_delegate_remote_.is_connected();
}

void ClientConnectionParametersImpl::PerformSetConnectionAttemptFailed(
    mojom::ConnectionAttemptFailureReason reason) {
  connection_delegate_remote_->OnConnectionAttemptFailure(reason);
}

void ClientConnectionParametersImpl::PerformSetConnectionSucceeded(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver) {
  connection_delegate_remote_->OnConnection(
      std::move(channel), std::move(message_receiver_receiver));
}

void ClientConnectionParametersImpl::OnConnectionDelegateRemoteDisconnected() {
  NotifyConnectionRequestCanceled();
}

}  // namespace secure_channel

}  // namespace chromeos
