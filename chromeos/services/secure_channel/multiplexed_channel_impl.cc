// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/multiplexed_channel_impl.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/single_client_message_proxy_impl.h"

namespace chromeos {

namespace secure_channel {

// static
MultiplexedChannelImpl::Factory*
    MultiplexedChannelImpl::Factory::test_factory_ = nullptr;

// static
MultiplexedChannelImpl::Factory* MultiplexedChannelImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<MultiplexedChannelImpl::Factory> factory;
  return factory.get();
}

// static
void MultiplexedChannelImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiplexedChannelImpl::Factory::~Factory() = default;

std::unique_ptr<MultiplexedChannel>
MultiplexedChannelImpl::Factory::BuildInstance(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    MultiplexedChannel::Delegate* delegate,
    ConnectionDetails connection_details,
    std::vector<std::unique_ptr<ClientConnectionParameters>>* initial_clients) {
  DCHECK(authenticated_channel);
  DCHECK(!authenticated_channel->is_disconnected());
  DCHECK(delegate);
  DCHECK(initial_clients);
  DCHECK(!initial_clients->empty());

  auto channel = base::WrapUnique(new MultiplexedChannelImpl(
      std::move(authenticated_channel), delegate, connection_details));
  for (auto& client_connection_parameters : *initial_clients) {
    bool success =
        channel->AddClientToChannel(std::move(client_connection_parameters));
    if (!success) {
      PA_LOG(ERROR) << "MultiplexedChannelImpl::Factory::BuildInstance(): "
                    << "Failed to add initial client.";
      NOTREACHED();
    }
  }

  return channel;
}

MultiplexedChannelImpl::MultiplexedChannelImpl(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    MultiplexedChannel::Delegate* delegate,
    ConnectionDetails connection_details)
    : MultiplexedChannel(delegate, connection_details),
      authenticated_channel_(std::move(authenticated_channel)) {
  authenticated_channel_->AddObserver(this);
}

MultiplexedChannelImpl::~MultiplexedChannelImpl() {
  authenticated_channel_->RemoveObserver(this);
}

bool MultiplexedChannelImpl::IsDisconnecting() const {
  return is_disconnecting_;
}

bool MultiplexedChannelImpl::IsDisconnected() const {
  return is_disconnected_;
}

void MultiplexedChannelImpl::PerformAddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters) {
  DCHECK(client_connection_parameters->IsClientWaitingForResponse());

  auto proxy = SingleClientMessageProxyImpl::Factory::Get()->BuildInstance(
      this /* delegate */, std::move(client_connection_parameters));
  DCHECK(!base::Contains(id_to_proxy_map_, proxy->GetProxyId()));
  id_to_proxy_map_[proxy->GetProxyId()] = std::move(proxy);
}

void MultiplexedChannelImpl::OnDisconnected() {
  is_disconnecting_ = false;
  is_disconnected_ = true;

  for (auto& proxy_entry : id_to_proxy_map_)
    proxy_entry.second->HandleRemoteDeviceDisconnection();

  NotifyDisconnected();
}

void MultiplexedChannelImpl::OnMessageReceived(const std::string& feature,
                                               const std::string& payload) {
  for (auto& proxy_entry : id_to_proxy_map_)
    proxy_entry.second->HandleReceivedMessage(feature, payload);
}

void MultiplexedChannelImpl::OnSendMessageRequested(
    const std::string& message_feaure,
    const std::string& message_payload,
    base::OnceClosure on_sent_callback) {
  authenticated_channel_->SendMessage(message_feaure, message_payload,
                                      std::move(on_sent_callback));
}

void MultiplexedChannelImpl::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  authenticated_channel_->GetConnectionMetadata(std::move(callback));
}

void MultiplexedChannelImpl::OnClientDisconnected(
    const base::UnguessableToken& proxy_id) {
  size_t num_entries_deleted = id_to_proxy_map_.erase(proxy_id);
  if (num_entries_deleted != 1u) {
    PA_LOG(ERROR) << "MultiplexedChannelImpl::OnClientDisconnected(): Client "
                  << "disconnected, but no entry in the map existed.";
    NOTREACHED();
  }

  if (!id_to_proxy_map_.empty())
    return;

  // If there are no clients remaining, the underlying channel should be
  // disconnected.
  is_disconnecting_ = true;
  authenticated_channel_->Disconnect();
}

}  // namespace secure_channel

}  // namespace chromeos
