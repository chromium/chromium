// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_MANAGER_H_

#include <tuple>
#include <vector>

#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_details.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/pending_connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace ash::secure_channel {

// Test PendingConnectionManager implementation.
class FakePendingConnectionManager : public PendingConnectionManager {
 public:
  FakePendingConnectionManager(Delegate* delegate);

  FakePendingConnectionManager(const FakePendingConnectionManager&) = delete;
  FakePendingConnectionManager& operator=(const FakePendingConnectionManager&) =
      delete;

  ~FakePendingConnectionManager() override;

  using HandledRequestsList =
      std::vector<std::tuple<ConnectionAttemptDetails,
                             std::unique_ptr<ClientConnectionParameters>,
                             ConnectionPriority>>;
  HandledRequestsList& handled_requests() { return handled_requests_; }

  // Notifies the delegate that the a connection was successful for the attempt
  // associated with |connection_details|. Before this call can complete, there
  // must be at least one handled request with those details. This call removes
  // the relevant handled requests from the list returned by handled_requests().
  //
  // The return value of this function is the raw pointers of all
  // ClientConnectionParameters that were passed to the delegate function.
  std::vector<ClientConnectionParameters*> NotifyConnectionForHandledRequests(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      const ConnectionDetails& connection_details);

 private:
  void HandleConnectionRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority) override;

  HandledRequestsList handled_requests_;
};

// Test PendingConnectionManager::Delegate implementation.
class FakePendingConnectionManagerDelegate
    : public PendingConnectionManager::Delegate {
 public:
  FakePendingConnectionManagerDelegate();

  FakePendingConnectionManagerDelegate(
      const FakePendingConnectionManagerDelegate&) = delete;
  FakePendingConnectionManagerDelegate& operator=(
      const FakePendingConnectionManagerDelegate&) = delete;

  ~FakePendingConnectionManagerDelegate() override;

  using ReceivedConnectionsList = std::vector<
      std::tuple<std::unique_ptr<AuthenticatedChannel>,
                 std::vector<std::unique_ptr<ClientConnectionParameters>>,
                 ConnectionDetails>>;
  ReceivedConnectionsList& received_connections_list() {
    return received_connections_list_;
  }

 private:
  void OnConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
      const ConnectionDetails& connection_details) override;

  ReceivedConnectionsList received_connections_list_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_MANAGER_H_
