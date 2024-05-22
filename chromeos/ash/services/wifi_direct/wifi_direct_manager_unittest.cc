// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/wifi_direct/wifi_direct_manager.h"

#include "ash/constants/ash_features.h"
#include "base/sync_socket.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_metrics_logger.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom-test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::wifi_direct {

using mojom::WifiCredentialsPtr;
using mojom::WifiDirectOperationResult;
using mojom::WifiP2PCapabilitiesPtr;

namespace {

constexpr char kIpv4Address[] = "100.0.0.1";
constexpr char kDefaultSSID[] = "DIRECT-A0";
constexpr char kDefaultPassphrase[] = "direct-passphrase";
constexpr char kAssignedSSID[] = "DIRECT-1a";
constexpr char kAssignedPassphrase[] = "test_passphrase";
const int kTestShillId = 0;
const base::TimeDelta kDurationTime = base::Seconds(123);
constexpr char kWifiP2PConnectionDurationHistogram[] =
    "Network.Ash.WiFiDirect.Connection.Duration";
constexpr char kGroupOwnerDisconnectReasonHistogram[] =
    "Network.Ash.WiFiDirect.GroupOwner.DisconnectReason";
constexpr char kGroupClientDisconnectReasonHistogram[] =
    "Network.Ash.WiFiDirect.GroupClient.DisconnectReason";

}  // namespace

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
    PatchPanelClient::InitializeFake();
    feature_list_.InitAndEnableFeature(features::kWifiDirect);
    WifiP2PController::Initialize();
    wifi_direct_manager_ = std::make_unique<WifiDirectManager>();
  }

  void TearDown() override {
    wifi_direct_manager_.reset();
    WifiP2PController::Shutdown();
    PatchPanelClient::Shutdown();
    shill_clients::Shutdown();
  }

  WifiP2POperationTestResult CreateWifiDirectGroup(
      WifiCredentialsPtr credentials) {
    auto wifi_direct_manager_async_waiter =
        mojom::WifiDirectManagerAsyncWaiter(wifi_direct_manager_.get());
    WifiP2POperationTestResult test_result;
    wifi_direct_manager_async_waiter.CreateWifiDirectGroup(
        std::move(credentials), &test_result.result,
        &test_result.wifi_direct_connection);
    return test_result;
  }

  WifiP2POperationTestResult ConnectToWifiDirectGroup(
      WifiCredentialsPtr credentials,
      std::optional<uint32_t> frequency) {
    auto wifi_direct_manager_async_waiter =
        mojom::WifiDirectManagerAsyncWaiter(wifi_direct_manager_.get());
    WifiP2POperationTestResult test_result;
    wifi_direct_manager_async_waiter.ConnectToWifiDirectGroup(
        std::move(credentials), frequency, &test_result.result,
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

  mojom::WifiDirectConnectionPropertiesPtr GetProperties(
      const mojo::Remote<mojom::WifiDirectConnection>& wifi_direct_connection) {
    mojom::WifiDirectConnectionProperties properties;
    auto wifi_direct_connection_async_waiter =
        mojom::WifiDirectConnectionAsyncWaiter(wifi_direct_connection.get());
    return wifi_direct_connection_async_waiter.GetProperties();
  }

  bool AssociateSocket(
      const mojo::Remote<mojom::WifiDirectConnection>& wifi_direct_connection) {
    auto wifi_direct_connection_async_waiter =
        mojom::WifiDirectConnectionAsyncWaiter(wifi_direct_connection.get());
    bool success;
    base::SyncSocket socket1, socket2;
    base::SyncSocket::CreatePair(&socket1, &socket2);
    wifi_direct_connection_async_waiter.AssociateSocket(
        mojo::PlatformHandle(socket1.Take()), &success);
    return success;
  }

  void ExpectConnectionsCount(size_t expected_connections_count) {
    wifi_direct_manager_->FlushForTesting();
    EXPECT_EQ(expected_connections_count,
              wifi_direct_manager_->GetConnectionsCountForTesting());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WifiDirectManager> wifi_direct_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WifiDirectManagerTest, CreateWifiDirectGroupWithCredentials_Success) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  auto credentials = mojom::WifiCredentials::New();
  credentials->ssid = kAssignedSSID;
  credentials->passphrase = kAssignedPassphrase;
  WifiP2POperationTestResult result_arguments =
      CreateWifiDirectGroup(std::move(credentials));
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);
  auto properties = GetProperties(wifi_direct_connection);
  EXPECT_EQ(1000u, properties->frequency);
  EXPECT_EQ(kIpv4Address, properties->ipv4_address);
  EXPECT_EQ(kAssignedSSID, properties->credentials->ssid);
  EXPECT_EQ(kAssignedPassphrase, properties->credentials->passphrase);
  EXPECT_TRUE(AssociateSocket(wifi_direct_connection));

  FakePatchPanelClient::Get()->set_tag_socket_success_for_testing(
      /*success=*/false);
  EXPECT_FALSE(AssociateSocket(wifi_direct_connection));

  task_environment_.FastForwardBy(kDurationTime);
  // Request disconnection from client side.
  wifi_direct_connection.reset();
  ExpectConnectionsCount(0);
  EXPECT_EQ(kTestShillId, ShillManagerClient::Get()
                              ->GetTestInterface()
                              ->GetRecentlyDestroyedP2PGroupId());
  histogram_tester_.ExpectTotalCount(kGroupOwnerDisconnectReasonHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kGroupOwnerDisconnectReasonHistogram,
      WifiP2PMetricsLogger::DisconnectReason::kClientInitiated, 1);

  histogram_tester_.ExpectTotalCount(kWifiP2PConnectionDurationHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kWifiP2PConnectionDurationHistogram,
                                          kDurationTime, 1);
}

TEST_F(WifiDirectManagerTest, CreateWifiDirectGroupNoCredentials_Success) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  WifiP2POperationTestResult result_arguments = CreateWifiDirectGroup(nullptr);
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);
  auto properties = GetProperties(wifi_direct_connection);
  EXPECT_EQ(1000u, properties->frequency);
  EXPECT_EQ(kIpv4Address, properties->ipv4_address);
  EXPECT_EQ(kDefaultSSID, properties->credentials->ssid);
  EXPECT_EQ(kDefaultPassphrase, properties->credentials->passphrase);
  EXPECT_TRUE(AssociateSocket(wifi_direct_connection));

  task_environment_.FastForwardBy(kDurationTime);
  // Request disconnection from client side.
  wifi_direct_connection.reset();
  ExpectConnectionsCount(0);
  histogram_tester_.ExpectTotalCount(kGroupOwnerDisconnectReasonHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kGroupOwnerDisconnectReasonHistogram,
      WifiP2PMetricsLogger::DisconnectReason::kClientInitiated, 1);
  histogram_tester_.ExpectTotalCount(kWifiP2PConnectionDurationHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kWifiP2PConnectionDurationHistogram,
                                          kDurationTime, 1);
}

TEST_F(WifiDirectManagerTest, CreateWifiDirectGroupFailure_InvalidCredentials) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kCreateP2PGroupResultNotSupported);
  auto credentials = mojom::WifiCredentials::New();
  credentials->ssid = "invalid-ssid";
  credentials->passphrase = "test_passphrase";
  WifiP2POperationTestResult result_arguments =
      CreateWifiDirectGroup(std::move(credentials));
  EXPECT_EQ(result_arguments.result,
            WifiDirectOperationResult::kInvalidArguments);
  EXPECT_FALSE(result_arguments.wifi_direct_connection.is_valid());
}

TEST_F(WifiDirectManagerTest, CreateWifiDirectGroupFailure_NotSupported) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kCreateP2PGroupResultNotSupported);
  auto credentials = mojom::WifiCredentials::New();
  credentials->ssid = "DIRECT-1a";
  credentials->passphrase = "test_passphrase";
  WifiP2POperationTestResult result_arguments =
      CreateWifiDirectGroup(std::move(credentials));
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kNotSupported);
  EXPECT_FALSE(result_arguments.wifi_direct_connection.is_valid());
}

TEST_F(WifiDirectManagerTest, ConnectToWifiDirectGroupSuccess) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kConnectToP2PGroupResultSuccess);
  auto credentials = mojom::WifiCredentials::New();
  credentials->ssid = kAssignedSSID;
  credentials->passphrase = kAssignedPassphrase;
  WifiP2POperationTestResult result_arguments =
      ConnectToWifiDirectGroup(std::move(credentials), 5200u);
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);
  auto properties = GetProperties(wifi_direct_connection);
  EXPECT_EQ(5200u, properties->frequency);
  EXPECT_EQ(kIpv4Address, properties->ipv4_address);
  EXPECT_EQ(kAssignedSSID, properties->credentials->ssid);
  EXPECT_EQ(kAssignedPassphrase, properties->credentials->passphrase);
  EXPECT_TRUE(AssociateSocket(wifi_direct_connection));

  FakePatchPanelClient::Get()->set_tag_socket_success_for_testing(
      /*success=*/false);
  EXPECT_FALSE(AssociateSocket(wifi_direct_connection));

  task_environment_.FastForwardBy(kDurationTime);
  // Request disconnection from client side.
  wifi_direct_connection.reset();
  ExpectConnectionsCount(0);
  EXPECT_EQ(kTestShillId, ShillManagerClient::Get()
                              ->GetTestInterface()
                              ->GetRecentlyDisconnectedP2PGroupId());
  histogram_tester_.ExpectTotalCount(kGroupClientDisconnectReasonHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kGroupClientDisconnectReasonHistogram,
      WifiP2PMetricsLogger::DisconnectReason::kClientInitiated, 1);
  histogram_tester_.ExpectTotalCount(kWifiP2PConnectionDurationHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kWifiP2PConnectionDurationHistogram,
                                          kDurationTime, 1);
}

TEST_F(WifiDirectManagerTest, GroupClientEvents) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kConnectToP2PGroupResultSuccess);
  auto credentials = mojom::WifiCredentials::New();
  credentials->ssid = kAssignedSSID;
  credentials->passphrase = kAssignedPassphrase;
  WifiP2POperationTestResult result_arguments =
      ConnectToWifiDirectGroup(std::move(credentials), 5200u);
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);

  task_environment_.FastForwardBy(kDurationTime);
  auto p2pclient_dict =
      base::Value::Dict().Set(shill::kP2PClientInfoShillIDProperty, 0);
  p2pclient_dict.Set(shill::kP2PClientInfoStateProperty,
                     shill::kP2PClientInfoStateIdle);

  base::Value::List p2pclient_list;
  p2pclient_list.Append(std::move(p2pclient_dict));

  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kP2PClientInfosProperty, base::Value(p2pclient_list.Clone()));

  ExpectConnectionsCount(0);
  histogram_tester_.ExpectTotalCount(kGroupClientDisconnectReasonHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kGroupClientDisconnectReasonHistogram,
      WifiP2PMetricsLogger::DisconnectReason::kInternalError, 1);
  histogram_tester_.ExpectTotalCount(kWifiP2PConnectionDurationHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kWifiP2PConnectionDurationHistogram,
                                          kDurationTime, 1);
}

TEST_F(WifiDirectManagerTest, ConnectToWifiDirectGroupFailure_InvalidResult) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                           "invalid_result");
  auto credentials = mojom::WifiCredentials::New();
  WifiP2POperationTestResult result_arguments =
      ConnectToWifiDirectGroup(std::move(credentials), 5200u);
  EXPECT_EQ(result_arguments.result,
            WifiDirectOperationResult::kInvalidResultCode);
  EXPECT_FALSE(result_arguments.wifi_direct_connection.is_valid());
}

TEST_F(WifiDirectManagerTest, GroupOwnerEvents) {
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  auto credentials = mojom::WifiCredentials::New();
  credentials->ssid = kAssignedSSID;
  credentials->passphrase = kAssignedPassphrase;
  WifiP2POperationTestResult result_arguments =
      CreateWifiDirectGroup(std::move(credentials));
  EXPECT_EQ(result_arguments.result, WifiDirectOperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.wifi_direct_connection.is_valid());

  mojo::Remote<mojom::WifiDirectConnection> wifi_direct_connection(
      std::move(result_arguments.wifi_direct_connection));
  ExpectConnectionsCount(1);

  task_environment_.FastForwardBy(kDurationTime);
  auto p2pgroup_dict =
      base::Value::Dict().Set(shill::kP2PGroupInfoShillIDProperty, 0);
  p2pgroup_dict.Set(shill::kP2PGroupInfoStateProperty,
                    shill::kP2PGroupInfoStateIdle);

  base::Value::List p2pgroup_list;
  p2pgroup_list.Append(std::move(p2pgroup_dict));

  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kP2PGroupInfosProperty, base::Value(p2pgroup_list.Clone()));

  ExpectConnectionsCount(0);
  histogram_tester_.ExpectTotalCount(kGroupOwnerDisconnectReasonHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kGroupOwnerDisconnectReasonHistogram,
      WifiP2PMetricsLogger::DisconnectReason::kInternalError, 1);
  histogram_tester_.ExpectTotalCount(kWifiP2PConnectionDurationHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kWifiP2PConnectionDurationHistogram,
                                          kDurationTime, 1);
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
