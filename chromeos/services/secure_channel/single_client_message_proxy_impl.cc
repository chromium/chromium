// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/single_client_message_proxy_impl.h"

#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
SingleClientMessageProxyImpl::Factory*
    SingleClientMessageProxyImpl::Factory::test_factory_ = nullptr;

// static
SingleClientMessageProxyImpl::Factory*
SingleClientMessageProxyImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SingleClientMessageProxyImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  test_factory_ = factory;
}

SingleClientMessageProxyImpl::Factory::~Factory() = default;

std::unique_ptr<SingleClientMessageProxy>
SingleClientMessageProxyImpl::Factory::BuildInstance(
    SingleClientMessageProxy::Delegate* delegate,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters) {
  return base::WrapUnique(new SingleClientMessageProxyImpl(
      delegate, std::move(client_connection_parameters)));
}

SingleClientMessageProxyImpl::SingleClientMessageProxyImpl(
    SingleClientMessageProxy::Delegate* delegate,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters)
    : SingleClientMessageProxy(delegate),
      client_connection_parameters_(std::move(client_connection_parameters)),
      channel_(std::make_unique<ChannelImpl>(this /* delegate */)) {
  DCHECK(client_connection_parameters_);
  client_connection_parameters_->SetConnectionSucceeded(
      channel_->GenerateRemote(),
      message_receiver_remote_.BindNewPipeAndPassReceiver());
}

SingleClientMessageProxyImpl::~SingleClientMessageProxyImpl() = default;

const base::UnguessableToken& SingleClientMessageProxyImpl::GetProxyId() {
  return client_connection_parameters_->id();
}

void SingleClientMessageProxyImpl::HandleReceivedMessage(
    const std::string& feature,
    const std::string& payload) {
  // Ignore messages intended for other clients.
  if (feature != client_connection_parameters_->feature())
    return;

  message_receiver_remote_->OnMessageReceived(payload);
}

void SingleClientMessageProxyImpl::HandleRemoteDeviceDisconnection() {
  channel_->HandleRemoteDeviceDisconnection();
}

void SingleClientMessageProxyImpl::OnSendMessageRequested(
    const std::string& message,
    base::OnceClosure on_sent_callback) {
  NotifySendMessageRequested(client_connection_parameters_->feature(), message,
                             std::move(on_sent_callback));
}

void SingleClientMessageProxyImpl::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  GetConnectionMetadataFromDelegate(std::move(callback));
}

void SingleClientMessageProxyImpl::OnClientDisconnected() {
  NotifyClientDisconnected();
}

void SingleClientMessageProxyImpl::FlushForTesting() {
  DCHECK(message_receiver_remote_);
  message_receiver_remote_.FlushForTesting();
}

}  // namespace secure_channel

}  // namespace chromeos
