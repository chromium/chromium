// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"

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

class HotspotAllowedFlagHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    shill_clients::InitializeFakes();

    hotspot_allowed_flag_handler_ =
        std::make_unique<HotspotAllowedFlagHandler>();
  }

  void TearDown() override {
    hotspot_allowed_flag_handler_.reset();

    shill_clients::Shutdown();
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

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
};

TEST_F(HotspotAllowedFlagHandlerTest, ExperimentalCarriersEnabled) {
  feature_list_.InitAndEnableFeature(
      features::kTetheringExperimentalFunctionality);
  hotspot_allowed_flag_handler_->Init();
  base::RunLoop().RunUntilIdle();
  ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &HotspotAllowedFlagHandlerTest::OnGetManagerCallback,
      base::Unretained(this), shill::kExperimentalTetheringFunctionality,
      /*expected_value=*/true));
  base::RunLoop().RunUntilIdle();
}

TEST_F(HotspotAllowedFlagHandlerTest, ExperimentalCarriersDisabled) {
  feature_list_.InitAndDisableFeature(
      features::kTetheringExperimentalFunctionality);
  hotspot_allowed_flag_handler_->Init();
  base::RunLoop().RunUntilIdle();
  ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &HotspotAllowedFlagHandlerTest::OnGetManagerCallback,
      base::Unretained(this), shill::kExperimentalTetheringFunctionality,
      /*expected_value=*/false));
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
