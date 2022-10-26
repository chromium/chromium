// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_active_connection_manager.h"

#include "base/check_op.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"

namespace ash::secure_channel {

FakeActiveConnectionManager::FakeActiveConnectionManager(
    ActiveConnectionManager::Delegate* delegate)
    : ActiveConnectionManager(delegate) {}

FakeActiveConnectionManager::~FakeActiveConnectionManager() = default;

void FakeActiveConnectionManager::SetDisconnecting(
    const ConnectionDetails& connection_details) {
  auto it = connection_details_to_active_metadata_map_.find(connection_details);
  DCHECK(it != connection_details_to_active_metadata_map_.end());

  ConnectionState& state = std::get<0>(it->second);
  DCHECK_EQ(ConnectionState::kActiveConnectionExists, state);

  state = ConnectionState::kDisconnectingConnectionExists;
}

void FakeActiveConnectionManager::SetDisconnected(
    const ConnectionDetails& connection_details) {
  auto it = connection_details_to_active_metadata_map_.find(connection_details);
  DCHECK(it != connection_details_to_active_metadata_map_.end());
  DCHECK_NE(ConnectionState::kNoConnectionExists, std::get<0>(it->second));
  connection_details_to_active_metadata_map_.erase(it);

  OnChannelDisconnected(connection_details);
}

ActiveConnectionManager::ConnectionState
FakeActiveConnectionManager::GetConnectionState(
    const ConnectionDetails& connection_details) const {
  auto it = connection_details_to_active_metadata_map_.find(connection_details);
  if (it == connection_details_to_active_metadata_map_.end())
    return ConnectionState::kNoConnectionExists;

  return std::get<0>(it->second);
}

void FakeActiveConnectionManager::PerformAddActiveConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
    const ConnectionDetails& connection_details) {
  bool inserted;
  std::tie(std::ignore, inserted) =
      connection_details_to_active_metadata_map_.emplace(
          connection_details,
          std::make_tuple(ConnectionState::kActiveConnectionExists,
                          std::move(authenticated_channel),
                          std::move(initial_clients)));
  DCHECK(inserted);
}

void FakeActiveConnectionManager::PerformAddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    const ConnectionDetails& connection_details) {
  auto it = connection_details_to_active_metadata_map_.find(connection_details);
  DCHECK(it != connection_details_to_active_metadata_map_.end());
  std::get<2>(it->second).push_back(std::move(client_connection_parameters));
}

FakeActiveConnectionManagerDelegate::FakeActiveConnectionManagerDelegate() =
    default;

FakeActiveConnectionManagerDelegate::~FakeActiveConnectionManagerDelegate() =
    default;

void FakeActiveConnectionManagerDelegate::OnDisconnected(
    const ConnectionDetails& connection_details) {
  ++connection_details_to_num_disconnections_map_[connection_details];
}

}  // namespace ash::secure_channel
