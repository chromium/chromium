// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/network_provider_impl.h"

#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/test_support/fake_platform_delegate.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ConnectionStatus = NetworkProviderImpl::ConnectionStatus;

class AssistantNetworkProviderImplTest : public ::testing::Test {
 public:
  AssistantNetworkProviderImplTest() {
    network_provider_.Initialize(&platform_delegate_);
  }

  AssistantNetworkProviderImplTest(const AssistantNetworkProviderImplTest&) =
      delete;
  AssistantNetworkProviderImplTest& operator=(
      const AssistantNetworkProviderImplTest&) = delete;

  ~AssistantNetworkProviderImplTest() override = default;

  void PublishConnectionStateType(ConnectionStateType connection_type) {
    std::vector<NetworkStatePropertiesPtr> active_networks;
    active_networks.push_back(CreateNetworkState(connection_type));

    PublishActiveNetworks(std::move(active_networks));
  }

  NetworkStatePropertiesPtr CreateNetworkState(
      ConnectionStateType connection_type) const {
    NetworkStatePropertiesPtr network_state =
        chromeos::network_config::mojom::NetworkStateProperties::New();
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
  base::test::TaskEnvironment task_environment;
  assistant::FakePlatformDelegate platform_delegate_;
  NetworkProviderImpl network_provider_;
};

TEST_F(AssistantNetworkProviderImplTest, StartWithStatusUnknown) {
  EXPECT_EQ(ConnectionStatus::UNKNOWN, network_provider_.GetConnectionStatus());
}

TEST_F(AssistantNetworkProviderImplTest,
       ChangeStateBasedOnConnectionStateType) {
  for (const auto& test : GetStatusPairs()) {
    ConnectionStateType input = test.first;
    ConnectionStatus expected = test.second;

    PublishConnectionStateType(input);

    EXPECT_EQ(expected, network_provider_.GetConnectionStatus())
        << "Failure with input " << input;
  }
}

TEST_F(AssistantNetworkProviderImplTest,
       IsOnlineIfOneOfTheActiveNetworksIsOnline) {
  std::vector<NetworkStatePropertiesPtr> active_networks{};
  active_networks.push_back(
      CreateNetworkState(ConnectionStateType::kNotConnected));
  active_networks.push_back(CreateNetworkState(ConnectionStateType::kOnline));

  PublishActiveNetworks(std::move(active_networks));

  EXPECT_EQ(ConnectionStatus::CONNECTED,
            network_provider_.GetConnectionStatus());
}

TEST_F(AssistantNetworkProviderImplTest, IsOfflineIfThereAreNoNetworks) {
  PublishActiveNetworks({});

  EXPECT_EQ(ConnectionStatus::DISCONNECTED_FROM_INTERNET,
            network_provider_.GetConnectionStatus());
}

}  // namespace ash::libassistant
