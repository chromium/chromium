// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/sensor/arc_iio_sensor_bridge.h"

#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/test/connection_holder_util.h"
#include "chromeos/ash/experiences/arc/test/fake_iio_sensor_instance.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class TestPowerManagerClient : public chromeos::FakePowerManagerClient {
 public:
  void GetSwitchStates(
      chromeos::DBusMethodCallback<SwitchStates> callback) override {
    chromeos::FakePowerManagerClient::GetSwitchStates(
        std::move(callback).Then(switch_states_processed_.GetCallback()));
  }

  bool WaitForSwitchStates() { return switch_states_processed_.Wait(); }

 private:
  base::test::TestFuture<void> switch_states_processed_;
};

}  // namespace

class ArcIioSensorBridgeTest : public testing::Test {
 public:
  ArcIioSensorBridgeTest() = default;
  ~ArcIioSensorBridgeTest() override = default;
  ArcIioSensorBridgeTest(const ArcIioSensorBridgeTest&) = delete;
  ArcIioSensorBridgeTest& operator=(const ArcIioSensorBridgeTest&) = delete;

  void SetUp() override {
    auto* power_manager_client = new TestPowerManagerClient();
    bridge_ = std::make_unique<ArcIioSensorBridge>(nullptr /* context */,
                                                   &bridge_service_);
    // Finish the initial tablet-mode query before tests send newer mode events,
    // so the queued snapshot cannot overwrite them.
    ASSERT_TRUE(power_manager_client->WaitForSwitchStates());
  }

  void TearDown() override {
    bridge_ = nullptr;
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  void InitializeInstance() {
    bridge_service_.iio_sensor()->SetInstance(&fake_instance_);
    WaitForInstanceReady(bridge_service_.iio_sensor());
  }

  base::test::TaskEnvironment task_environment_;

  ArcBridgeService bridge_service_;
  FakeIioSensorInstance fake_instance_;
  std::unique_ptr<ArcIioSensorBridge> bridge_;
};

TEST_F(ArcIioSensorBridgeTest, TabletMode) {
  // Verify the fake's initial state.
  EXPECT_FALSE(fake_instance_.is_tablet_mode_on());

  // ARC receives the tablet mode update just after initialization.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  InitializeInstance();
  EXPECT_TRUE(fake_instance_.is_tablet_mode_on());

  // ARC continues receiving updates after initialization.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  EXPECT_FALSE(fake_instance_.is_tablet_mode_on());

  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_TRUE(fake_instance_.is_tablet_mode_on());
}

}  // namespace arc
