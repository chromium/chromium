// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/fake_power_manager_client.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const double kInitialBatteryPercent = 85;
const double kUpdatedBatteryPercent = 70;
const power_manager::PowerSupplyProperties_BatteryState kInitialBatteryState =
    power_manager::PowerSupplyProperties_BatteryState_DISCHARGING;
const power_manager::PowerSupplyProperties_ExternalPower kInitialExternalPower =
    power_manager::PowerSupplyProperties_ExternalPower_USB;

class TestObserver : public PowerManagerClient::Observer {
 public:
  TestObserver() : num_power_changed_(0) {}
  ~TestObserver() override = default;

  const power_manager::PowerSupplyProperties& props() const { return props_; }
  int num_power_changed() const { return num_power_changed_; }

  void ClearProps() { props_.Clear(); }

  void PowerChanged(
      const power_manager::PowerSupplyProperties& proto) override {
    props_ = proto;
    ++num_power_changed_;
  }

 private:
  int num_power_changed_;
  power_manager::PowerSupplyProperties props_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

void SetTestProperties(power_manager::PowerSupplyProperties* props) {
  props->set_battery_percent(kInitialBatteryPercent);
  props->set_is_calculating_battery_time(true);
  props->set_battery_state(kInitialBatteryState);
  props->set_external_power(kInitialExternalPower);
}

}  // namespace

TEST(FakePowerManagerClientTest, UpdatePowerPropertiesTest) {
  // Checking to verify when UpdatePowerProperties is called,
  // |props_| values are updated.
  FakePowerManagerClient client;
  power_manager::PowerSupplyProperties props;

  SetTestProperties(&props);
  client.UpdatePowerProperties(props);

  EXPECT_EQ(kInitialBatteryPercent, client.GetLastStatus()->battery_percent());
  EXPECT_TRUE(client.GetLastStatus()->is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, client.GetLastStatus()->battery_state());
  EXPECT_EQ(kInitialExternalPower, client.GetLastStatus()->external_power());

  // Test if when the values are changed, the correct data is set in the
  // FakePowerManagerClient.
  props = *client.GetLastStatus();
  props.set_battery_percent(kUpdatedBatteryPercent);
  client.UpdatePowerProperties(props);

  EXPECT_EQ(kUpdatedBatteryPercent, client.GetLastStatus()->battery_percent());
  EXPECT_TRUE(client.GetLastStatus()->is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, client.GetLastStatus()->battery_state());
  EXPECT_EQ(kInitialExternalPower, client.GetLastStatus()->external_power());
}

TEST(FakePowerManagerClientTest, NotifyObserversTest) {
  FakePowerManagerClient client;
  TestObserver test_observer;

  // Test adding observer.
  client.AddObserver(&test_observer);
  EXPECT_TRUE(client.HasObserver(&test_observer));

  // Test if NotifyObservers() sends the correct values to |observer|.
  // Check number of times NotifyObservers() is called.
  power_manager::PowerSupplyProperties props;
  SetTestProperties(&props);
  client.UpdatePowerProperties(props);

  EXPECT_EQ(kInitialBatteryPercent, test_observer.props().battery_percent());
  EXPECT_TRUE(test_observer.props().is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, test_observer.props().battery_state());
  EXPECT_EQ(kInitialExternalPower, test_observer.props().external_power());
  EXPECT_EQ(1, test_observer.num_power_changed());

  // Test if RequestStatusUpdate() will propagate the data to the observer.
  // Check number of times NotifyObservers is called.
  // RequestStatusUpdate posts to the current message loop. This is
  // necessary because we want to make sure that NotifyObservers() is
  // called as a result of RequestStatusUpdate().
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  test_observer.ClearProps();
  client.RequestStatusUpdate();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kInitialBatteryPercent, test_observer.props().battery_percent());
  EXPECT_TRUE(test_observer.props().is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, test_observer.props().battery_state());
  EXPECT_EQ(kInitialExternalPower, test_observer.props().external_power());
  EXPECT_EQ(2, test_observer.num_power_changed());

  // Check when values are changed, the correct values are propagated to the
  // observer
  props = *client.GetLastStatus();
  props.set_battery_percent(kUpdatedBatteryPercent);
  props.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  client.UpdatePowerProperties(props);

  EXPECT_EQ(kUpdatedBatteryPercent, test_observer.props().battery_percent());
  EXPECT_TRUE(test_observer.props().is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, test_observer.props().battery_state());
  EXPECT_EQ(power_manager::PowerSupplyProperties_ExternalPower_AC,
            test_observer.props().external_power());
  EXPECT_EQ(3, test_observer.num_power_changed());

  // Test removing observer.
  client.RemoveObserver(&test_observer);
  EXPECT_FALSE(client.HasObserver(&test_observer));
}

TEST(FakePowerManagerClientTest, AmbientColorSupport) {
  FakePowerManagerClient client;
  EXPECT_FALSE(client.SupportsAmbientColor());
  client.set_supports_ambient_color(true);
  EXPECT_TRUE(client.SupportsAmbientColor());
  client.set_supports_ambient_color(false);
  EXPECT_FALSE(client.SupportsAmbientColor());
}

}  // namespace chromeos
