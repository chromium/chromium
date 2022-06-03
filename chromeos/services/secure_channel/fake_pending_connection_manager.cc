// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_pending_connection_manager.h"

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "chromeos/services/secure_channel/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

FakePendingConnectionManager::FakePendingConnectionManager(Delegate* delegate)
    : PendingConnectionManager(delegate) {}

FakePendingConnectionManager::~FakePendingConnectionManager() = default;

std::vector<ClientConnectionParameters*>
FakePendingConnectionManager::NotifyConnectionForHandledRequests(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    const ConnectionDetails& connection_details) {
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_list;

  auto it = handled_requests_.begin();
  while (it != handled_requests_.end()) {
    ConnectionDetails details_for_handled_request =
        std::get<0>(*it).GetAssociatedConnectionDetails();
    if (details_for_handled_request != connection_details) {
      ++it;
      continue;
    }

    client_list.push_back(std::move(std::get<1>(*it)));
    it = handled_requests_.erase(it);
  }

  // There must be at least one client in the list.
  DCHECK_LT(0u, client_list.size());

  // Make a copy of the client list to pass as a return value for this function.
  std::vector<ClientConnectionParameters*> client_list_raw;
  std::transform(client_list.begin(), client_list.end(),
                 std::back_inserter(client_list_raw),
                 [](auto& client) { return client.get(); });

  NotifyOnConnection(std::move(authenticated_channel), std::move(client_list),
                     connection_details);

  return client_list_raw;
}

void FakePendingConnectionManager::HandleConnectionRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  handled_requests_.push_back(std::make_tuple(
      connection_attempt_details, std::move(client_connection_parameters),
      connection_priority));
}

FakePendingConnectionManagerDelegate::FakePendingConnectionManagerDelegate() =
    default;

FakePendingConnectionManagerDelegate::~FakePendingConnectionManagerDelegate() =
    default;

void FakePendingConnectionManagerDelegate::OnConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
    const ConnectionDetails& connection_details) {
  received_connections_list_.push_back(
      std::make_tuple(std::move(authenticated_channel), std::move(clients),
                      connection_details));
}

}  // namespace secure_channel

}  // namespace chromeos
