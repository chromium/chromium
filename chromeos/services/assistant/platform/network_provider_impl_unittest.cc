// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/network_provider_impl.h"

#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

using network_config::mojom::ConnectionStateType;
using network_config::mojom::NetworkStatePropertiesPtr;
using ConnectionStatus = NetworkProviderImpl::ConnectionStatus;

class NetworkProviderImplTest : public ::testing::Test {
 public:
  NetworkProviderImplTest() : network_provider_(nullptr) {}
  ~NetworkProviderImplTest() override {}

  void PublishConnectionStateType(ConnectionStateType connection_type) {
    std::vector<NetworkStatePropertiesPtr> active_networks;
    active_networks.push_back(CreateNetworkState(connection_type));

    PublishActiveNetworks(std::move(active_networks));
  }

  NetworkStatePropertiesPtr CreateNetworkState(
      ConnectionStateType connection_type) const {
    NetworkStatePropertiesPtr network_state =
        network_config::mojom::NetworkStateProperties::New();
    network_state->connection_state = connection_type;
    return network_state;
  }

  void PublishActiveNetworks(
      std::vector<NetworkStatePropertiesPtr> active_networks) {
    network_provider_.OnActiveNetworksChanged(std::move(active_networks));
  }

  std::vector<std::pair<ConnectionStateType, ConnectionStatus>>
  GetStatusPairs() {
    return {
        {ConnectionStateType::kOnline, ConnectionStatus::CONNECTED},
        {ConnectionStateType::kConnected,
         ConnectionStatus::DISCONNECTED_FROM_INTERNET},
        {ConnectionStateType::kPortal,
         ConnectionStatus::DISCONNECTED_FROM_INTERNET},
        {ConnectionStateType::kNotConnected,
         ConnectionStatus::DISCONNECTED_FROM_INTERNET},
    };
  }

 protected:
  NetworkProviderImpl network_provider_;

  DISALLOW_COPY_AND_ASSIGN(NetworkProviderImplTest);
};

TEST_F(NetworkProviderImplTest, StartWithStatusUnknown) {
  EXPECT_EQ(ConnectionStatus::UNKNOWN, network_provider_.GetConnectionStatus());
}

TEST_F(NetworkProviderImplTest, ChangeStateBasedOnConnectionStateType) {
  for (const auto& test : GetStatusPairs()) {
    ConnectionStateType input = test.first;
    ConnectionStatus expected = test.second;

    PublishConnectionStateType(input);

    EXPECT_EQ(expected, network_provider_.GetConnectionStatus())
        << "Failure with input " << input;
  }
}

TEST_F(NetworkProviderImplTest, IsOnlineIfOneOfTheActiveNetworksIsOnline) {
  std::vector<NetworkStatePropertiesPtr> active_networks{};
  active_networks.push_back(
      CreateNetworkState(ConnectionStateType::kNotConnected));
  active_networks.push_back(CreateNetworkState(ConnectionStateType::kOnline));

  PublishActiveNetworks(std::move(active_networks));

  EXPECT_EQ(ConnectionStatus::CONNECTED,
            network_provider_.GetConnectionStatus());
}

TEST_F(NetworkProviderImplTest, IsOfflineIfThereAreNoNetworks) {
  PublishActiveNetworks({});

  EXPECT_EQ(ConnectionStatus::DISCONNECTED_FROM_INTERNET,
            network_provider_.GetConnectionStatus());
}

}  // namespace assistant
}  // namespace chromeos
