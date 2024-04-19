// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/wifi_direct/wifi_direct_manager.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom-test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::wifi_direct {

using mojom::WifiDirectOperationResult;
using mojom::WifiP2PCapabilitiesPtr;

class WifiDirectManagerTest : public testing::Test {
 public:
  struct WifiP2POperationTestResult {
    WifiDirectOperationResult result;
    mojo::PendingRemote<mojom::WifiDirectConnection> wifi_direct_connection;
  };

  WifiDirectManagerTest() = default;
  WifiDirectManagerTest(const WifiDirectManagerTest&) = delete;
  WifiDirectManagerTest& operator=(const WifiDirectManagerTest&) = delete;
  ~WifiDirectManagerTest() override = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    feature_list_.InitAndEnableFeature(features::kWifiDirect);
    WifiP2PController::Initialize();
    wifi_direct_manager_ = std::make_unique<WifiDirectManager>();
  }

  void TearDown() override {
    wifi_direct_manager_.reset();
    WifiP2PController::Shutdown();
    shill_clients::Shutdown();
  }

  WifiP2POperationTestResult CreateWifiDirectGroup(
      const std::string& ssid,
      const std::string& passphrase) {
    auto wifi_direct_manager_async_waiter =
        mojom::WifiDirectManagerAsyncWaiter(wifi_direct_manager_.get());
    WifiP2POperationTestResult test_result;
    wifi_direct_manager_async_waiter.CreateWifiDirectGroup(
        ssid, passphrase, &test_result.result,
        &test_result.wifi_direct_connection);
    return test_result;
  }

  WifiP2POperationTestResult ConnectToWifiDirectGroup(
      const std::string& ssid,
      const std::string& passphrase,
      std::optional<uint32_t> frequency) {
    auto wifi_direct_manager_async_waiter =
        mojom::WifiDirectManagerAsyncWaiter(wifi_direct_manager_.get());
    WifiP2POperationTestResult test_result;
    wifi_direct_manager_async_waiter.ConnectToWifiDirectGroup(
        ssid, passphrase, frequency, &test_result.result,
        &test_result.wifi_direct_connection);
    return test_result;
  }

  WifiP2PCapabilitiesPtr GetWifiP2PCapabilities() {
    auto wifi_direct_manager_async_waiter =
        mojom::WifiDirectManagerAsyncWaiter(wifi_direct_manager_.get());
    WifiP2PCapabilitiesPtr result;
    wifi_direct_manager_async_waiter.GetWifiP2PCapabilities(&result);
    return result;
  }

  uint32_t GetFrequency(
      const mojo::Remote<mojom::WifiDirectConnection>& wifi_direct_connection) {
    uint32_t frequency;
    auto wifi_direct_connection_async_waiter =
        mojom::WifiDirectConnectionAsyncWaiter(wifi_direct_connection.get());
    wifi_direct_connection_async_waiter.GetFrequency(&frequency);
    return frequency;
  }

  void ExpectConnectionsCount(size_t expected_connections_count) {
    wifi_direct_manager_->FlushForTesting();
    EXPECT_EQ(expected_connections_count,
              wifi_direct_manager_->GetConnectionsCountForTesting());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WifiDirectManager> wifi_direct_manager_;
};

TEST_F(WifiDirectManagerTest, CreateWifiDirectGroupSuccess) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  WifiP2POperationTestResult result_arguments =
      CreateWifiDirectGroup("DIRECT-1a", "passphrase");
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);
  EXPECT_EQ(1000u, GetFrequency(wifi_direct_connection));
  // Request disconnection from client side.
  wifi_direct_connection.reset();
  ExpectConnectionsCount(0);
}

TEST_F(WifiDirectManagerTest, CreateWifiDirectGroupFailure_InvalidArguments) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kCreateP2PGroupResultNotSupported);
  WifiP2POperationTestResult result_arguments =
      CreateWifiDirectGroup("DIRECT-1a", "passphrase");
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kNotSupported);
  EXPECT_FALSE(result_arguments.wifi_direct_connection.is_valid());
}

TEST_F(WifiDirectManagerTest, ConnectToWifiDirectGroupSuccess) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kConnectToP2PGroupResultSuccess);
  WifiP2POperationTestResult result_arguments =
      ConnectToWifiDirectGroup("DIRECT-1a", "passphrase", 5200u);
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);
  EXPECT_EQ(5200u, GetFrequency(wifi_direct_connection));
  // Request disconnection from client side.
  wifi_direct_connection.reset();
  ExpectConnectionsCount(0);
}

TEST_F(WifiDirectManagerTest, ConnectToWifiDirectGroupFailure_InvalidResult) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                           "invalid_result");
  WifiP2POperationTestResult result_arguments =
      ConnectToWifiDirectGroup("DIRECT-1a", "passphrase", 5200u);
  EXPECT_EQ(result_arguments.result,
            WifiDirectOperationResult::kInvalidResultCode);
  EXPECT_FALSE(result_arguments.wifi_direct_connection.is_valid());
}

TEST_F(WifiDirectManagerTest, GetWifiP2PCapabilities) {
  auto capabilities_dict =
      base::Value::Dict().Set(shill::kP2PCapabilitiesGroupReadinessProperty,
                              shill::kP2PCapabilitiesGroupReadinessReady);
  capabilities_dict.Set(shill::kP2PCapabilitiesClientReadinessProperty,
                        shill::kP2PCapabilitiesClientReadinessReady);
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kP2PCapabilitiesProperty, base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetWifiP2PCapabilities()->is_client_ready);
  EXPECT_TRUE(GetWifiP2PCapabilities()->is_owner_ready);

  capabilities_dict.Set(shill::kP2PCapabilitiesClientReadinessProperty,
                        shill::kP2PCapabilitiesClientReadinessNotReady);
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kP2PCapabilitiesProperty, base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetWifiP2PCapabilities()->is_client_ready);
  EXPECT_TRUE(GetWifiP2PCapabilities()->is_owner_ready);
}

}  // namespace ash::wifi_direct
