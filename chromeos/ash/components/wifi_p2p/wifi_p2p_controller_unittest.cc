// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

class WifiP2PControllerTest : public ::testing::Test {
 public:
  struct WifiP2POperationTestResult {
    WifiP2PController::OperationResult result;
    std::optional<WifiP2PController::WifiDirectConnectionMetadata> metadata;
  };

  void SetUp() override { shill_clients::InitializeFakes(); }

  void TearDown() override { shill_clients::Shutdown(); }

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

  WifiP2POperationTestResult CreateP2PGroup(const std::string& ssid,
                                            const std::string& passphrase) {
    WifiP2POperationTestResult test_result;
    base::RunLoop run_loop;
    WifiP2PController::Get()->CreateWifiP2PGroup(
        ssid, passphrase,
        base::BindLambdaForTesting(
            [&](WifiP2PController::OperationResult result,
                std::optional<WifiP2PController::WifiDirectConnectionMetadata>
                    metadata) {
              test_result.result = result;
              test_result.metadata = metadata;
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
            [&](WifiP2PController::OperationResult result,
                std::optional<WifiP2PController::WifiDirectConnectionMetadata>
                    metadata) {
              test_result.result = result;
              test_result.metadata = metadata;
              run_loop.Quit();
            }));
    base::RunLoop().RunUntilIdle();
    return test_result;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WifiP2PControllerTest, FeatureEnabled) {
  Init();
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PControllerTest::OnGetManagerCallback,
                     base::Unretained(this), shill::kP2PAllowedProperty,
                     /*expected_value=*/true));
  WifiP2PController::Shutdown();
}

TEST_F(WifiP2PControllerTest, FeatureDisabled) {
  Init(/*enable_flag=*/false);
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PControllerTest::OnGetManagerCallback,
                     base::Unretained(this), shill::kP2PAllowedProperty,
                     /*expected_value=*/false));
  WifiP2PController::Shutdown();
}

TEST_F(WifiP2PControllerTest, CreateP2PGroupSuccess) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kSuccess,
                                        shill::kCreateP2PGroupResultSuccess);
  const WifiP2POperationTestResult& result_arguments =
      CreateP2PGroup("ssid", "passphrase");
  EXPECT_EQ(result_arguments.result,
            WifiP2PController::OperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.metadata);
  EXPECT_EQ(result_arguments.metadata->shill_id, 0);
  EXPECT_EQ(result_arguments.metadata->frequency, 1000u);
  EXPECT_EQ(result_arguments.metadata->network_id, 1);

  WifiP2PController::Shutdown();
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
  EXPECT_EQ(result_arguments.result,
            WifiP2PController::OperationResult::kInvalidArguments);
  EXPECT_FALSE(result_arguments.metadata);

  WifiP2PController::Shutdown();
}

TEST_F(WifiP2PControllerTest, CreateP2PGroupFailure_DBusError) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateCreateP2PGroupResult(FakeShillSimulatedResult::kFailure,
                                        std::string());
  const WifiP2POperationTestResult& result_arguments =
      CreateP2PGroup("ssid", "passphrase");
  EXPECT_EQ(result_arguments.result,
            WifiP2PController::OperationResult::kDBusError);
  EXPECT_FALSE(result_arguments.metadata);

  WifiP2PController::Shutdown();
}

TEST_F(WifiP2PControllerTest, ConnectToP2PGroupSuccess) {
  Init();

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateConnectToP2PGroupResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kConnectToP2PGroupResultSuccess);
  const WifiP2POperationTestResult& result_arguments =
      ConnectP2PGroup("ssid", "passphrase", /*frequency=*/5200u);
  EXPECT_EQ(result_arguments.result,
            WifiP2PController::OperationResult::kSuccess);
  ASSERT_TRUE(result_arguments.metadata);
  EXPECT_EQ(result_arguments.metadata->shill_id, 0);
  EXPECT_EQ(result_arguments.metadata->frequency, 5200u);
  EXPECT_EQ(result_arguments.metadata->network_id, 1);

  WifiP2PController::Shutdown();
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
      ConnectP2PGroup("ssid", "passphrase", /*frequency=*/5200u);
  EXPECT_EQ(result_arguments.result,
            WifiP2PController::OperationResult::kConcurrencyNotSupported);
  EXPECT_FALSE(result_arguments.metadata);

  WifiP2PController::Shutdown();
}

}  // namespace ash
