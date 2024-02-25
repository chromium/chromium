// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/single_client_proxy_impl.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

// static
SingleClientProxyImpl::Factory* SingleClientProxyImpl::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<SingleClientProxy> SingleClientProxyImpl::Factory::Create(
    SingleClientProxy::Delegate* delegate,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        delegate, std::move(client_connection_parameters));
  }

  return base::WrapUnique(new SingleClientProxyImpl(
      delegate, std::move(client_connection_parameters)));
}

// static
void SingleClientProxyImpl::Factory::SetFactoryForTesting(Factory* factory) {
  test_factory_ = factory;
}

SingleClientProxyImpl::Factory::~Factory() = default;

SingleClientProxyImpl::SingleClientProxyImpl(
    SingleClientProxy::Delegate* delegate,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters)
    : SingleClientProxy(delegate),
      client_connection_parameters_(std::move(client_connection_parameters)),
      channel_(std::make_unique<ChannelImpl>(this /* delegate */)) {
  DCHECK(client_connection_parameters_);
  client_connection_parameters_->SetConnectionSucceeded(
      channel_->GenerateRemote(),
      message_receiver_remote_.BindNewPipeAndPassReceiver(),
      nearby_connection_state_listener_remote_.BindNewPipeAndPassReceiver());
}

SingleClientProxyImpl::~SingleClientProxyImpl() = default;

const base::UnguessableToken& SingleClientProxyImpl::GetProxyId() {
  return client_connection_parameters_->id();
}

void SingleClientProxyImpl::HandleReceivedMessage(const std::string& feature,
                                                  const std::string& payload) {
  // Ignore messages intended for other clients.
  if (feature != client_connection_parameters_->feature())
    return;

  message_receiver_remote_->OnMessageReceived(payload);
}

void SingleClientProxyImpl::HandleNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  nearby_connection_state_listener_remote_->OnNearbyConnectionStateChanged(
      step, result);
}

void SingleClientProxyImpl::HandleRemoteDeviceDisconnection() {
  channel_->HandleRemoteDeviceDisconnection();
}

void SingleClientProxyImpl::OnSendMessageRequested(
    const std::string& message,
    base::OnceClosure on_sent_callback) {
  NotifySendMessageRequested(client_connection_parameters_->feature(), message,
                             std::move(on_sent_callback));
}

void SingleClientProxyImpl::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  RegisterPayloadFileWithDelegate(payload_id, std::move(payload_files),
                                  std::move(file_transfer_update_callback),
                                  std::move(registration_result_callback));
}

void SingleClientProxyImpl::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  GetConnectionMetadataFromDelegate(std::move(callback));
}

void SingleClientProxyImpl::OnClientDisconnected() {
  NotifyClientDisconnected();
}

void SingleClientProxyImpl::FlushForTesting() {
  DCHECK(message_receiver_remote_);
  message_receiver_remote_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::secure_channel
