// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_status_model.h"

#include "chromeos/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {

TEST(PhoneStatusModelTest, Initialization) {
  PhoneStatusModel success(PhoneStatusModel::MobileStatus::kSimWithReception,
                           CreateFakeMobileConnectionMetadata(),
                           PhoneStatusModel::ChargingState::kNotCharging,
                           PhoneStatusModel::BatterySaverState::kOff,
                           /*battery_percentage=*/100u);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            success.mobile_status());
  EXPECT_EQ(CreateFakeMobileConnectionMetadata(),
            *success.mobile_connection_metadata());
  EXPECT_EQ(PhoneStatusModel::ChargingState::kNotCharging,
            success.charging_state());
  EXPECT_EQ(PhoneStatusModel::BatterySaverState::kOff,
            success.battery_saver_state());
  EXPECT_EQ(100u, success.battery_percentage());

  // If battery is >100, it is set to 100.
  PhoneStatusModel high_battery(
      PhoneStatusModel::MobileStatus::kSimWithReception,
      CreateFakeMobileConnectionMetadata(),
      PhoneStatusModel::ChargingState::kNotCharging,
      PhoneStatusModel::BatterySaverState::kOff,
      /*battery_percentage=*/1000u);
  EXPECT_EQ(100u, high_battery.battery_percentage());

  // If the MobileStatus does not indicate reception, connection metadata should
  // be cleared.
  PhoneStatusModel no_sim(PhoneStatusModel::MobileStatus::kNoSim,
                          CreateFakeMobileConnectionMetadata(),
                          PhoneStatusModel::ChargingState::kNotCharging,
                          PhoneStatusModel::BatterySaverState::kOff,
                          /*battery_percentage=*/100u);
  EXPECT_FALSE(no_sim.mobile_connection_metadata().has_value());

  // If the MobileStatus does indicate reception but no connection metadata is
  // available, the status is set back to no reception.
  PhoneStatusModel no_connection_metadata(
      PhoneStatusModel::MobileStatus::kSimWithReception,
      /*mobile_connection_metadata=*/base::nullopt,
      PhoneStatusModel::ChargingState::kNotCharging,
      PhoneStatusModel::BatterySaverState::kOff,
      /*battery_percentage=*/100u);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimButNoReception,
            no_connection_metadata.mobile_status());
}

}  // namespace phonehub
}  // namespace chromeos
