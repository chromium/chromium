// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_health/public/cpp/network_health_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_health/network_health_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos::network_health {

class NetworkHealthHelperTest : public ::testing::Test {
 public:
  NetworkHealthHelperTest() = default;
  NetworkHealthHelperTest(const NetworkHealthHelperTest&) = delete;
  NetworkHealthHelperTest& operator=(const NetworkHealthHelperTest&) = delete;
  ~NetworkHealthHelperTest() override = default;

  void SetUp() override {
    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();
    cros_network_config_test_helper_->network_state_helper()
        .ResetDevicesAndServices();

    network_health_service_ = std::make_unique<NetworkHealthService>();
    helper_ =
        NetworkHealthHelper::CreateForTesting(network_health_service_.get());
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    helper_.reset();
    network_health_service_.reset();
    cros_network_config_test_helper_.reset();
  }

  NetworkHealthHelper* helper() { return helper_.get(); }

  std::string SetupWiFiService(const std::string& state) {
    return cros_network_config_test_helper_->network_state_helper()
        .ConfigureWiFi(state);
  }

  void SetWiFiState(const std::string& path, const std::string& state) {
    return cros_network_config_test_helper_->network_state_helper()
        .SetServiceProperty(path, shill::kStateProperty, base::Value(state));
  }

  // private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  std::unique_ptr<NetworkHealthService> network_health_service_;
  std::unique_ptr<NetworkHealthHelper> helper_;
};

TEST_F(NetworkHealthHelperTest, RequestDefaultNetworkNone) {
  mojom::Network* default_network = helper()->default_network();
  // NetworkHealth provides state for every available technology type (WiFi is
  // enabled by default in tests).
  ASSERT_TRUE(default_network);
  // The default state should be not connected.
  EXPECT_EQ(default_network->state, mojom::NetworkState::kNotConnected);
}

TEST_F(NetworkHealthHelperTest, RequestDefaultNetworkOnline) {
  SetupWiFiService(shill::kStateOnline);
  mojom::Network* default_network = helper()->default_network();
  ASSERT_TRUE(default_network);
  EXPECT_EQ(default_network->state, mojom::NetworkState::kOnline);
}

TEST_F(NetworkHealthHelperTest, RequestIsPortalState) {
  EXPECT_FALSE(helper()->IsPortalState());

  std::string path = SetupWiFiService(shill::kStateOnline);
  EXPECT_FALSE(helper()->IsPortalState());

  SetWiFiState(path, shill::kStateRedirectFound);
  EXPECT_TRUE(helper()->IsPortalState());
}

}  // namespace chromeos::network_health
