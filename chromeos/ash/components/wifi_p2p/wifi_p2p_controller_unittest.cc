// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

class WifiP2PControllerTest : public ::testing::Test {
 public:
  void SetUp() override { shill_clients::InitializeFakes(); }

  void TearDown() override { shill_clients::Shutdown(); }

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

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WifiP2PControllerTest, FeatureEnabled) {
  feature_list_.InitAndEnableFeature(features::kWifiDirect);
  WifiP2PController::Initialize();
  base::RunLoop().RunUntilIdle();
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PControllerTest::OnGetManagerCallback,
                     base::Unretained(this), shill::kP2PAllowedProperty,
                     /*expected_value=*/true));
  WifiP2PController::Shutdown();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WifiP2PControllerTest, FeatureDisabled) {
  feature_list_.InitAndDisableFeature(features::kWifiDirect);
  WifiP2PController::Initialize();
  base::RunLoop().RunUntilIdle();
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PControllerTest::OnGetManagerCallback,
                     base::Unretained(this), shill::kP2PAllowedProperty,
                     /*expected_value=*/false));
  WifiP2PController::Shutdown();
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
