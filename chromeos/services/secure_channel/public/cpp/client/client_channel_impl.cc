// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/client_channel_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
ClientChannelImpl::Factory* ClientChannelImpl::Factory::test_factory_ = nullptr;

// static
ClientChannelImpl::Factory* ClientChannelImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<ClientChannelImpl::Factory> factory;
  return factory.get();
}

// static
void ClientChannelImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

ClientChannelImpl::Factory::~Factory() = default;

std::unique_ptr<ClientChannel> ClientChannelImpl::Factory::BuildInstance(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver) {
  return base::WrapUnique(new ClientChannelImpl(
      std::move(channel), std::move(message_receiver_receiver)));
}

ClientChannelImpl::ClientChannelImpl(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver)
    : channel_(std::move(channel)),
      receiver_(this, std::move(message_receiver_receiver)) {
  channel_.set_disconnect_with_reason_handler(
      base::BindOnce(&ClientChannelImpl::OnChannelDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

ClientChannelImpl::~ClientChannelImpl() = default;

void ClientChannelImpl::PerformGetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  channel_->GetConnectionMetadata(
      base::BindOnce(&ClientChannelImpl::OnGetConnectionMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientChannelImpl::OnMessageReceived(const std::string& message) {
  NotifyMessageReceived(message);
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

void ClientChannelImpl::OnChannelDisconnected(
    uint32_t disconnection_reason,
    const std::string& disconnection_description) {
  if (disconnection_reason != mojom::Channel::kConnectionDroppedReason) {
    LOG(ERROR) << "Received unexpected disconnection reason: "
               << disconnection_description;
  }

  channel_.reset();
  receiver_.reset();
  NotifyDisconnected();
}

void ClientChannelImpl::FlushForTesting() {
  channel_.FlushForTesting();
}

}  // namespace secure_channel

}  // namespace chromeos
