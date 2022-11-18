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
  mojom::NetworkPtr result_ptr;
  base::RunLoop run_loop;
  helper()->RequestDefaultNetwork(
      base::BindLambdaForTesting([&](mojom::NetworkPtr& default_network) {
        result_ptr = default_network.Clone();
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_FALSE(result_ptr);
}

TEST_F(NetworkHealthHelperTest, RequestDefaultNetworkOnline) {
  SetupWiFiService(shill::kStateOnline);

  mojom::NetworkPtr result_ptr;
  base::RunLoop run_loop;
  helper()->RequestDefaultNetwork(
      base::BindLambdaForTesting([&](mojom::NetworkPtr& default_network) {
        result_ptr = default_network.Clone();
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(result_ptr);
  EXPECT_EQ(result_ptr->state, mojom::NetworkState::kOnline);
}

TEST_F(NetworkHealthHelperTest, RequestIsPortalState) {
  std::string path = SetupWiFiService(shill::kStateOnline);
  {
    bool result;
    base::RunLoop run_loop;
    helper()->RequestIsPortalState(
        base::BindLambdaForTesting([&](bool is_portal_state) {
          result = is_portal_state;
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_FALSE(result);
  }

  SetWiFiState(path, shill::kStateRedirectFound);
  {
    bool result;
    base::RunLoop run_loop;
    helper()->RequestIsPortalState(
        base::BindLambdaForTesting([&](bool is_portal_state) {
          result = is_portal_state;
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_TRUE(result);
  }
}

}  // namespace chromeos::network_health
