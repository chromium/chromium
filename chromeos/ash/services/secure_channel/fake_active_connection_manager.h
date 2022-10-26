// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_ACTIVE_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_ACTIVE_CONNECTION_MANAGER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "chromeos/ash/services/secure_channel/active_connection_manager.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"

namespace ash::secure_channel {

class AuthenticatedChannel;

// Test ActiveConnectionManager implementation.
class FakeActiveConnectionManager : public ActiveConnectionManager {
 public:
  explicit FakeActiveConnectionManager(
      ActiveConnectionManager::Delegate* delegate);

  FakeActiveConnectionManager(const FakeActiveConnectionManager&) = delete;
  FakeActiveConnectionManager& operator=(const FakeActiveConnectionManager&) =
      delete;

  ~FakeActiveConnectionManager() override;

  using DetailsToMetadataMap = base::flat_map<
      ConnectionDetails,
      std::tuple<ConnectionState,
                 std::unique_ptr<AuthenticatedChannel>,
                 std::vector<std::unique_ptr<ClientConnectionParameters>>>>;

  DetailsToMetadataMap& connection_details_to_active_metadata_map() {
    return connection_details_to_active_metadata_map_;
  }

  void SetDisconnecting(const ConnectionDetails& connection_details);
  void SetDisconnected(const ConnectionDetails& connection_details);

 private:
  // ActiveConnectionManager:
  ConnectionState GetConnectionState(
      const ConnectionDetails& connection_details) const override;
  void PerformAddActiveConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
      const ConnectionDetails& connection_details) override;
  void PerformAddClientToChannel(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      const ConnectionDetails& connection_details) override;

  DetailsToMetadataMap connection_details_to_active_metadata_map_;
};

// Test ActiveConnectionManager::Delegate implementation.
class FakeActiveConnectionManagerDelegate
    : public ActiveConnectionManager::Delegate {
 public:
  FakeActiveConnectionManagerDelegate();

  FakeActiveConnectionManagerDelegate(
      const FakeActiveConnectionManagerDelegate&) = delete;
  FakeActiveConnectionManagerDelegate& operator=(
      const FakeActiveConnectionManagerDelegate&) = delete;

  ~FakeActiveConnectionManagerDelegate() override;

  const base::flat_map<ConnectionDetails, size_t>&
  connection_details_to_num_disconnections_map() const {
    return connection_details_to_num_disconnections_map_;
  }

 private:
  void OnDisconnected(const ConnectionDetails& connection_details) override;

  base::flat_map<ConnectionDetails, size_t>
      connection_details_to_num_disconnections_map_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_ACTIVE_CONNECTION_MANAGER_H_
