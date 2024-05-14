// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/active_connection_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/multiplexed_channel_impl.h"

namespace ash::secure_channel {

// static
ActiveConnectionManagerImpl::Factory*
    ActiveConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ActiveConnectionManager>
ActiveConnectionManagerImpl::Factory::Create(
    ActiveConnectionManager::Delegate* delegate) {
  if (test_factory_)
    return test_factory_->CreateInstance(delegate);

  return base::WrapUnique(new ActiveConnectionManagerImpl(delegate));
}

// static
void ActiveConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ActiveConnectionManagerImpl::Factory::~Factory() = default;

ActiveConnectionManagerImpl::ActiveConnectionManagerImpl(
    ActiveConnectionManager::Delegate* delegate)
    : ActiveConnectionManager(delegate) {}

ActiveConnectionManagerImpl::~ActiveConnectionManagerImpl() = default;

ActiveConnectionManager::ConnectionState
ActiveConnectionManagerImpl::GetConnectionState(
    const ConnectionDetails& connection_details) const {
  auto it = details_to_channel_map_.find(connection_details);
  if (it == details_to_channel_map_.end())
    return ConnectionState::kNoConnectionExists;

  const MultiplexedChannel* channel = it->second.get();
  DCHECK(channel);
  DCHECK(!channel->IsDisconnected());

  return channel->IsDisconnecting()
             ? ConnectionState::kDisconnectingConnectionExists
             : ConnectionState::kActiveConnectionExists;
}

void ActiveConnectionManagerImpl::PerformAddActiveConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
    const ConnectionDetails& connection_details) {
  details_to_channel_map_[connection_details] =
      MultiplexedChannelImpl::Factory::Create(
          std::move(authenticated_channel), this /* delegate */,
          connection_details, &initial_clients);
}

void ActiveConnectionManagerImpl::PerformAddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    const ConnectionDetails& connection_details) {
  bool success =
      details_to_channel_map_[connection_details]->AddClientToChannel(
          std::move(client_connection_parameters));
  if (!success) {
    PA_LOG(ERROR) << "ActiveConnectionManagerImpl::"
                  << "PerformAddClientToChannel(): Could not add "
                  << "ClientConnectionParameters to MultiplexedChannel.";
    NOTREACHED_IN_MIGRATION();
  }
}

void ActiveConnectionManagerImpl::OnDisconnected(
    const ConnectionDetails& connection_details) {
  // Make a copy of |connection_details|, since the owner of these details is
  // deleted below.
  const ConnectionDetails connection_details_copy = connection_details;

  size_t num_deleted = details_to_channel_map_.erase(connection_details);
  if (num_deleted != 1u) {
    PA_LOG(ERROR) << "ActiveConnectionManagerImpl::OnDisconnected(): Tried to "
                  << "delete map entry, but it did not exist.";
    NOTREACHED_IN_MIGRATION();
  }

  OnChannelDisconnected(connection_details_copy);
}

}  // namespace ash::secure_channel
