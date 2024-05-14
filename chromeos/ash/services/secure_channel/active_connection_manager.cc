// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/active_connection_manager.h"

#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"

namespace ash::secure_channel {

ActiveConnectionManager::ActiveConnectionManager(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

ActiveConnectionManager::~ActiveConnectionManager() = default;

void ActiveConnectionManager::AddActiveConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
    const ConnectionDetails& connection_details) {
  DCHECK(authenticated_channel);
  DCHECK(!authenticated_channel->is_disconnected());

  auto connection_state = GetConnectionState(connection_details);
  if (connection_state != ConnectionState::kNoConnectionExists) {
    PA_LOG(ERROR) << "ActiveConnectionManager::AddActiveConnection(): Tried to "
                  << "add new active channel, but the connection state was "
                  << connection_state
                  << ". Connection details: " << connection_details;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  PerformAddActiveConnection(std::move(authenticated_channel),
                             std::move(initial_clients), connection_details);
}

void ActiveConnectionManager::AddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    const ConnectionDetails& connection_details) {
  auto connection_state = GetConnectionState(connection_details);
  if (connection_state != ConnectionState::kActiveConnectionExists) {
    PA_LOG(ERROR) << "ActiveConnectionManager::AddClientToChannel(): Tried to "
                  << "add new client to active channel, but the connection "
                  << "state was " << connection_state << ". "
                  << "Connection details: " << connection_details;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  PerformAddClientToChannel(std::move(client_connection_parameters),
                            connection_details);
}

void ActiveConnectionManager::OnChannelDisconnected(
    const ConnectionDetails& connection_details) {
  delegate_->OnDisconnected(connection_details);
}

std::ostream& operator<<(
    std::ostream& stream,
    const ActiveConnectionManager::ConnectionState& connection_state) {
  switch (connection_state) {
    case ActiveConnectionManager::ConnectionState::kActiveConnectionExists:
      stream << "[active connection exists]";
      break;
    case ActiveConnectionManager::ConnectionState::kNoConnectionExists:
      stream << "[no connection exists]";
      break;
    case ActiveConnectionManager::ConnectionState::
        kDisconnectingConnectionExists:
      stream << "[disconnection connection exists]";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
