// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/technology_state_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

class TechnologyStateControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    technology_state_controller_ =
        std::make_unique<TechnologyStateController>();
    technology_state_controller_->Init(
        network_state_test_helper_.network_state_handler());
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    technology_state_controller_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(TechnologyStateControllerTest, ChangeWifiTechnology) {
  // Disable Wifi technology. Will immediately set the state to DISABLING.
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Run the message loop. When Shill updates the enabled technologies since
  // the state should transition to AVAILABLE.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Enable Wifi technology. Will immediately set the state to ENABLING.
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Run the message loop. State should change to ENABLED.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
}

TEST_F(TechnologyStateControllerTest, ChangePhysicalTechnologies) {
  // Disable Physical technologies which includes WiFi(), Cellular() and
  // Ethernet()
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::Physical(), /*enabled=*/false,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));

  // Run the message loop. When Shill updates the enabled technologies since
  // the state should transition to AVAILABLE.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));

  // Enable Physical technologies. Will immediately set the state to ENABLING.
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::Physical(), /*enabled=*/true,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));

  // Run the message loop. State should change to ENABLED.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));
}

}  // namespace ash