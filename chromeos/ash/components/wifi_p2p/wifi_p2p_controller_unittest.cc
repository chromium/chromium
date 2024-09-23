// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_group.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_metrics_logger.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

constexpr char kDefaultIpv4Address[] = "100.0.0.1";
constexpr char kDefaultSSID[] = "DIRECT-A0";
constexpr char kDefaultPassphrase[] = "direct-passphrase";
constexpr char kAssignedSSID[] = "DIRECT-A0";
constexpr char kAssignedPassphrase[] = "assigned-passphrase";

}  // namespace

using OperationResult = WifiP2PController::OperationResult;

class WifiP2PControllerTest : public ::testing::Test {
 public:
  struct WifiP2POperationTestResult {
    OperationResult result;
    std::optional<WifiP2PGroup> group_metadata;
  };

  void SetUp() override {
    shill_clients::InitializeFakes();
    PatchPanelClient::InitializeFake();
  }

  void TearDown() override {
    if (WifiP2PController::IsInitialized()) {
      WifiP2PController::Shutdown();
    }
    PatchPanelClient::Shutdown();
    shill_clients::Shutdown();
  }

  void Init(bool enable_flag = true) {
    if (enable_flag) {
      feature_list_.InitAndEnableFeature(features::kWifiDirect);
    } else {
      feature_list_.InitAndDisableFeature(features::kWifiDirect);
    }
    WifiP2PController::Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void OnGetManagerCallback(const std::string& property_name,
                            bool expected_value,
                            std::optional<base::Value::Dict> result) {
    if (!result) {
      ADD_FAILURE() << "Error getting Shill manager properties";
      return;
    }
    std::optional<bool> actual_value = result->FindBool(property_name);
    if (!actual_value) {
      ADD_FAILURE()
          << "Error getting TetheringAllowed in Shill manager properties";
      return;
    }
    EXPECT_EQ(expected_value, *actual_value);
  }

  WifiP2POperationTestResult CreateP2PGroup(
      std::optional<std::string> ssid,
      std::optional<std::string> passphrase) {
    WifiP2POperationTestResult test_result;
    base::RunLoop run_loop;
    WifiP2PController::Get()->CreateWifiP2PGroup(
        ssid, passphrase,
        base::BindLambdaForTesting(
            [&](OperationResult result,
                std::optional<WifiP2PGroup> group_metadata) {
              test_result.result = result;
              test_result.group_metadata = group_metadata;
              run_loop.Quit();
            }));
    base::RunLoop().RunUntilIdle();
    return test_result;
  }

  OperationResult DestroyP2PGroup(const int shill_id) {
    OperationResult test_result;
    base::RunLoop run_loop;
    WifiP2PController::Get()->DestroyWifiP2PGroup(
        shill_id, base::BindLambdaForTesting([&](OperationResult result) {
          test_result = result;
          run_loop.Quit();
        }));
    base::RunLoop().RunUntilIdle();
    return test_result;
  }

  WifiP2POperationTestResult ConnectP2PGroup(const std::string& ssid,
                                             const std::string& passphrase,
                                             uint32_t frequency) {
    WifiP2POperationTestResult test_result;
    base::RunLoop run_loop;
    WifiP2PController::Get()->ConnectToWifiP2PGroup(
        ssid, passphrase, frequency,
        base::BindLambdaForTesting(
            [&](OperationResult result,
                std::optional<WifiP2PGroup> group_metadata) {
              test_result.result = result;
              test_result.group_metadata = group_metadata;
              run_loop.Quit();
            }));
    base::RunLoop().RunUntilIdle();
    return test_result;
  }

  OperationResult DisconnectP2PGroup(const int shill_id) {
    OperationResult test_result;
    base::RunLoop run_loop;
    WifiP2PController::Get()->DisconnectFromWifiP2PGroup(
        shill_id, base::BindLambdaForTesting([&](OperationResult result) {
          test_result = result;
          run_loop.Quit();
        }));
    base::RunLoop().RunUntilIdle();
    return test_result;
  }

  bool TagSocket(int network_id, base::ScopedFD socket_fd) {
    bool result;
    base::RunLoop run_loop;
    WifiP2PController::Get()->TagSocket(
        network_id, std::move(socket_fd),
        base::BindLambdaForTesting([&](bool success) {
          result = success;
          run_loop.Quit();
        }));
    base::RunLoop().RunUntilIdle();
    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WifiP2PControllerTest, FeatureEnabled) {
  Init();
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PControllerTest::OnGetManagerCallback,
                     base::Unretained(this), shill::kP2PAllowedProperty,
                     /*expected_value=*/true));
}

TEST_F(WifiP2PControllerTest, FeatureDisabled) {
  Init(/*enable_flag=*/false);
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PControllerTest::OnGetManagerCallback,
                     base::Unretained(this), shill::kP2PAllowedProperty,
                     /*expected_value=*/false));
}

TEST_F(WifiP2PControllerTest, CreateP2PGroupWithCredentials_Success) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  const WifiP2POperationTestResult& result_arguments =
      CreateP2PGroup(kAssignedSSID, kAssignedPassphrase);
  EXPECT_EQ(result_arguments.result, OperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.group_metadata);
  EXPECT_EQ(result_arguments.group_metadata->shill_id(), 0);
  EXPECT_EQ(result_arguments.group_metadata->frequency(), 1000u);
  EXPECT_EQ(result_arguments.group_metadata->network_id(), 1);
  EXPECT_EQ(result_arguments.group_metadata->ipv4_address(),
            kDefaultIpv4Address);
  EXPECT_EQ(result_arguments.group_metadata->ssid(), kAssignedSSID);
  EXPECT_EQ(result_arguments.group_metadata->passphrase(), kAssignedPassphrase);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram, OperationResult::kSuccess,
      1);
}

TEST_F(WifiP2PControllerTest, CreateP2PGroupWithoutCredentials_Success) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  const WifiP2POperationTestResult& result_arguments =
      CreateP2PGroup(/*ssid=*/std::nullopt, /*passphrase=*/std::nullopt);
  EXPECT_EQ(result_arguments.result, OperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.group_metadata);
  EXPECT_EQ(result_arguments.group_metadata->shill_id(), 0);
  EXPECT_EQ(result_arguments.group_metadata->frequency(), 1000u);
  EXPECT_EQ(result_arguments.group_metadata->network_id(), 1);
  EXPECT_EQ(result_arguments.group_metadata->ipv4_address(),
            kDefaultIpv4Address);
  EXPECT_EQ(result_arguments.group_metadata->ssid(), kDefaultSSID);
  EXPECT_EQ(result_arguments.group_metadata->passphrase(), kDefaultPassphrase);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram, OperationResult::kSuccess,
      1);
}

TEST_F(WifiP2PControllerTest, CreateP2PGroupFailure_InvalidArguments) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kCreateP2PGroupResultInvalidArguments);
  const WifiP2POperationTestResult& result_arguments =
      CreateP2PGroup("ssid", "passphrase");
  EXPECT_EQ(result_arguments.result, OperationResult::kInvalidArguments);
  EXPECT_FALSE(result_arguments.group_metadata);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram,
      OperationResult::kInvalidArguments, 1);
}

TEST_F(WifiP2PControllerTest, CreateP2PGroupFailure_DBusError) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kFailure,
                                        std::string());
  const WifiP2POperationTestResult& result_arguments =
      CreateP2PGroup("DIRECT-1a", "passphrase");
  EXPECT_EQ(result_arguments.result, OperationResult::kDBusError);
  EXPECT_FALSE(result_arguments.group_metadata);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kCreateP2PGroupHistogram,
      OperationResult::kDBusError, 1);
}

TEST_F(WifiP2PControllerTest, DestroyP2PGroupSuccess) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateDestroyP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                         shill::kDestroyP2PGroupResultSuccess);
  const OperationResult& result = DestroyP2PGroup(/*shill_id=*/0);
  EXPECT_EQ(result, OperationResult::kSuccess);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kDestroyP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kDestroyP2PGroupHistogram,
      OperationResult::kSuccess, 1);
}

TEST_F(WifiP2PControllerTest, DestroyP2PGroupSuccess_GroupNotFound) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateDestroyP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                         shill::kDestroyP2PGroupResultNoGroup);
  const OperationResult& result = DestroyP2PGroup(/*shill_id=*/0);
  EXPECT_EQ(result, OperationResult::kGroupNotFound);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kDestroyP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kDestroyP2PGroupHistogram,
      OperationResult::kGroupNotFound, 1);
}

TEST_F(WifiP2PControllerTest, ConnectToP2PGroupSuccess) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kConnectToP2PGroupResultSuccess);
  const WifiP2POperationTestResult& result_arguments =
      ConnectP2PGroup(kAssignedSSID, kAssignedPassphrase, /*frequency=*/5200u);
  EXPECT_EQ(result_arguments.result, OperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.group_metadata);
  EXPECT_EQ(result_arguments.group_metadata->shill_id(), 0);
  EXPECT_EQ(result_arguments.group_metadata->frequency(), 5200u);
  EXPECT_EQ(result_arguments.group_metadata->network_id(), 1);
  EXPECT_EQ(result_arguments.group_metadata->ipv4_address(),
            kDefaultIpv4Address);
  EXPECT_EQ(result_arguments.group_metadata->ssid(), kAssignedSSID);
  EXPECT_EQ(result_arguments.group_metadata->passphrase(), kAssignedPassphrase);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kConnectP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kConnectP2PGroupHistogram,
      OperationResult::kSuccess, 1);
}

TEST_F(WifiP2PControllerTest,
       ConnectToP2PGroupFailure_ConcurrencyNotSupported) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kConnectToP2PGroupResultConcurrencyNotSupported);
  const WifiP2POperationTestResult& result_arguments =
      ConnectP2PGroup("DIRECT-1a", "passphrase", /*frequency=*/5200u);
  EXPECT_EQ(result_arguments.result, OperationResult::kConcurrencyNotSupported);
  EXPECT_FALSE(result_arguments.group_metadata);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kConnectP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kConnectP2PGroupHistogram,
      OperationResult::kConcurrencyNotSupported, 1);
}

TEST_F(WifiP2PControllerTest, DisconnectFromP2PGroupSuccess) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateDisconnectFromP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kDisconnectFromP2PGroupResultSuccess);
  const OperationResult& result = DisconnectP2PGroup(/*shill_id=*/0);
  EXPECT_EQ(result, OperationResult::kSuccess);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kDisconnectP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kDisconnectP2PGroupHistogram,
      OperationResult::kSuccess, 1);
}

TEST_F(WifiP2PControllerTest, DisconnectFromP2PGroupFailure_NotConnected) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateDisconnectFromP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kDisconnectFromP2PGroupResultNotConnected);
  const OperationResult& result = DisconnectP2PGroup(/*shill_id=*/0);
  EXPECT_EQ(result, OperationResult::kNotConnected);

  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kDisconnectP2PGroupHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kDisconnectP2PGroupHistogram,
      OperationResult::kNotConnected, 1);
}

TEST_F(WifiP2PControllerTest, GetP2PCapabilities) {
  auto capabilities_dict =
      base::Value::Dict().Set(shill::kP2PCapabilitiesGroupReadinessProperty,
                              shill::kP2PCapabilitiesGroupReadinessReady);
  capabilities_dict.Set(shill::kP2PCapabilitiesClientReadinessProperty,
                        shill::kP2PCapabilitiesClientReadinessReady);
  capabilities_dict.Set(shill::kP2PCapabilitiesP2PSupportedProperty,
                        /*is_p2p_supported=*/true);
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kP2PCapabilitiesProperty, base::Value(capabilities_dict.Clone()));

  Init();

  WifiP2PController::WifiP2PCapabilities result =
      WifiP2PController::Get()->GetP2PCapabilities();
  EXPECT_TRUE(result.is_owner_ready);
  EXPECT_TRUE(result.is_client_ready);
  EXPECT_TRUE(result.is_p2p_supported);
  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kWifiP2PCapabilitiesHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kWifiP2PCapabilitiesHistogram,
      WifiP2PMetricsLogger::WifiP2PMetricsCapabilities::
          kBothClientAndOwnerReady,
      1);

  capabilities_dict.Set(shill::kP2PCapabilitiesClientReadinessProperty,
                        shill::kP2PCapabilitiesClientReadinessNotReady);
  capabilities_dict.Set(shill::kP2PCapabilitiesP2PSupportedProperty,
                        /*is_p2p_supported=*/false);
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kP2PCapabilitiesProperty, base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  result = WifiP2PController::Get()->GetP2PCapabilities();
  EXPECT_TRUE(result.is_owner_ready);
  EXPECT_FALSE(result.is_client_ready);
  EXPECT_FALSE(result.is_p2p_supported);
  histogram_tester_.ExpectTotalCount(
      WifiP2PMetricsLogger::kWifiP2PCapabilitiesHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      WifiP2PMetricsLogger::kWifiP2PCapabilitiesHistogram,
      WifiP2PMetricsLogger::WifiP2PMetricsCapabilities::kOnlyOwnerReady, 1);
}

TEST_F(WifiP2PControllerTest, TagSocketSuccess) {
  Init();

  EXPECT_TRUE(TagSocket(123, base::ScopedFD()));
  histogram_tester_.ExpectTotalCount(WifiP2PMetricsLogger::kTagSocketHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(WifiP2PMetricsLogger::kTagSocketHistogram,
                                      true, 1);
}

TEST_F(WifiP2PControllerTest, TagSocketFailure) {
  Init();
  FakePatchPanelClient::Get()->set_tag_socket_success_for_testing(
      /*success=*/false);
  EXPECT_FALSE(TagSocket(123, base::ScopedFD()));
  histogram_tester_.ExpectTotalCount(WifiP2PMetricsLogger::kTagSocketHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(WifiP2PMetricsLogger::kTagSocketHistogram,
                                      false, 1);
}

}  // namespace ash
