// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/fake_power_manager_client.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
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

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  const power_manager::PowerSupplyProperties& props() const { return props_; }
  int num_power_changed() const { return num_power_changed_; }
  const power_manager::BatterySaverModeState& battery_saver_state() const {
    return battery_saver_state_;
  }
  const power_manager::AmbientLightSensorChange&
  last_ambient_light_sensor_change() const {
    return last_ambient_light_sensor_change_;
  }
  const power_manager::AmbientLightSensorChange&
  last_keyboard_ambient_light_sensor_change() const {
    return last_keyboard_ambient_light_sensor_change_;
  }

  void ClearProps() { props_.Clear(); }

  void PowerChanged(
      const power_manager::PowerSupplyProperties& proto) override {
    props_ = proto;
    ++num_power_changed_;
  }

  void BatterySaverModeStateChanged(
      const power_manager::BatterySaverModeState& proto) override {
    battery_saver_state_ = proto;
  }

  void AmbientLightSensorEnabledChanged(
      const power_manager::AmbientLightSensorChange& change) override {
    last_ambient_light_sensor_change_ = change;
  }

  void KeyboardAmbientLightSensorEnabledChanged(
      const power_manager::AmbientLightSensorChange& change) override {
    last_keyboard_ambient_light_sensor_change_ = change;
  }

 private:
  int num_power_changed_;
  power_manager::PowerSupplyProperties props_;
  power_manager::BatterySaverModeState battery_saver_state_;
  power_manager::AmbientLightSensorChange last_ambient_light_sensor_change_;
  power_manager::AmbientLightSensorChange
      last_keyboard_ambient_light_sensor_change_;
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

TEST(FakePowerManagerClientTest, UpdatePowerPropertiesWithNullTest) {
  // Checking to verify when UpdatePowerProperties is called,
  // |props_| values are updated.
  FakePowerManagerClient client;
  power_manager::PowerSupplyProperties props;

  SetTestProperties(&props);
  client.UpdatePowerProperties(props);

  ASSERT_TRUE(client.GetLastStatus().has_value());
  EXPECT_EQ(kInitialBatteryPercent, client.GetLastStatus()->battery_percent());
  EXPECT_TRUE(client.GetLastStatus()->is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, client.GetLastStatus()->battery_state());
  EXPECT_EQ(kInitialExternalPower, client.GetLastStatus()->external_power());

  client.UpdatePowerProperties(std::nullopt);

  EXPECT_FALSE(client.GetLastStatus().has_value());
}

TEST(FakePowerManagerClientTest,
     UpdatePowerPropertiesWithNullWillNotNotifyObserversTest) {
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

  EXPECT_EQ(1, test_observer.num_power_changed());
  EXPECT_EQ(kInitialBatteryPercent, test_observer.props().battery_percent());
  EXPECT_TRUE(test_observer.props().is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, test_observer.props().battery_state());
  EXPECT_EQ(kInitialExternalPower, test_observer.props().external_power());

  // Check when update power properties with null,
  // the observer isn't notified and the props from observer doesn't change.
  client.UpdatePowerProperties(std::nullopt);

  EXPECT_EQ(1, test_observer.num_power_changed());
  EXPECT_EQ(kInitialBatteryPercent, test_observer.props().battery_percent());
  EXPECT_TRUE(test_observer.props().is_calculating_battery_time());
  EXPECT_EQ(kInitialBatteryState, test_observer.props().battery_state());
  EXPECT_EQ(kInitialExternalPower, test_observer.props().external_power());
}

// Test that observers are notified asynchronously of battery saver state
// updates.
TEST(FakePowerManagerClientTest, BatterySaverState) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  FakePowerManagerClient client;
  TestObserver test_observer;

  client.AddObserver(&test_observer);
  EXPECT_FALSE(test_observer.battery_saver_state().has_enabled());

  // Turn battery saver on, and check that observers are notified
  // asynchronously.
  {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(true);
    client.SetBatterySaverModeState(request);

    // Battery saver should not be enabled synchronously, since the real client
    // waits for a response from Power Manager.
    EXPECT_FALSE(test_observer.battery_saver_state().has_enabled());
    base::RunLoop().RunUntilIdle();

    // Should be on now.
    EXPECT_TRUE(test_observer.battery_saver_state().has_enabled());
    EXPECT_TRUE(test_observer.battery_saver_state().enabled());
  }

  // Verify GetBatterySaverModeState is on and asynchronous.
  {
    bool called = false;
    client.GetBatterySaverModeState(base::BindOnce(
        [](std::reference_wrapper<bool> called_ref,
           std::optional<power_manager::BatterySaverModeState> state) {
          EXPECT_TRUE(state.has_value());
          EXPECT_TRUE(state->has_enabled());
          EXPECT_TRUE(state->enabled());
          called_ref.get() = true;
        },
        std::ref(called)));

    // Result should be asynchronous.
    EXPECT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }

  // Turn battery saver off, and check that observers are notified
  // asynchronously.
  {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(false);
    client.SetBatterySaverModeState(request);

    // Again, make sure response is async by checking for stale values.
    EXPECT_TRUE(test_observer.battery_saver_state().has_enabled());
    EXPECT_TRUE(test_observer.battery_saver_state().enabled());

    base::RunLoop().RunUntilIdle();

    // Should be off now.
    EXPECT_TRUE(test_observer.battery_saver_state().has_enabled());
    EXPECT_FALSE(test_observer.battery_saver_state().enabled());
  }

  // Verify GetBatterySaverModeState is off and asynchronous.
  {
    bool called = false;
    client.GetBatterySaverModeState(base::BindOnce(
        [](std::reference_wrapper<bool> called_ref,
           std::optional<power_manager::BatterySaverModeState> state) {
          EXPECT_TRUE(state.has_value());
          EXPECT_TRUE(state->has_enabled());
          EXPECT_FALSE(state->enabled());
          called_ref.get() = true;
        },
        std::ref(called)));

    // Result should be asynchronous.
    EXPECT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }
}

// Test that observers are notified asynchronously when the Ambient Light Sensor
// status changes.
TEST(FakePowerManagerClientTest, AmbientLightSensorEnabled) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  FakePowerManagerClient client;
  TestObserver test_observer;

  client.AddObserver(&test_observer);

  // The Ambient Light Sensor is enabled by default.
  EXPECT_FALSE(
      test_observer.last_ambient_light_sensor_change().has_sensor_enabled());
  EXPECT_FALSE(test_observer.last_ambient_light_sensor_change().has_cause());

  // Turn the Ambient Light Sensor off, and check that observers are notified
  // asynchronously.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  client.SetAmbientLightSensorEnabled(set_als_request);

  // The Ambient Light Sensor should not be disabled synchronously, since the
  // real client waits for a response from Power Manager.
  EXPECT_FALSE(
      test_observer.last_ambient_light_sensor_change().has_sensor_enabled());
  EXPECT_FALSE(test_observer.last_ambient_light_sensor_change().has_cause());
  base::RunLoop().RunUntilIdle();

  // The Ambient Light Sensor should be disabled now.
  EXPECT_TRUE(
      test_observer.last_ambient_light_sensor_change().has_sensor_enabled());
  EXPECT_FALSE(
      test_observer.last_ambient_light_sensor_change().sensor_enabled());
  // The change cause should be USER_REQUEST_SETTINGS_APP because the change was
  // triggered via the PowerManagerClient function.
  EXPECT_TRUE(test_observer.last_ambient_light_sensor_change().has_cause());
  EXPECT_EQ(
      test_observer.last_ambient_light_sensor_change().cause(),
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // Turn the Ambient Light Sensor on, and check that observers are notified
  // asynchronously.
  set_als_request.set_sensor_enabled(true);
  client.SetAmbientLightSensorEnabled(set_als_request);

  // The Ambient Light Sensor should not be enabled synchronously, since the
  // real client waits for a response from Power Manager.
  EXPECT_TRUE(
      test_observer.last_ambient_light_sensor_change().has_sensor_enabled());
  EXPECT_FALSE(
      test_observer.last_ambient_light_sensor_change().sensor_enabled());
  base::RunLoop().RunUntilIdle();

  // The Ambient Light Sensor should be enabled now.
  EXPECT_TRUE(
      test_observer.last_ambient_light_sensor_change().sensor_enabled());
  // The cause should be the same as before.
  EXPECT_TRUE(test_observer.last_ambient_light_sensor_change().has_cause());
  EXPECT_EQ(
      test_observer.last_ambient_light_sensor_change().cause(),
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
}

// Test that observers are notified asynchronously when the Keyboard Ambient
// Light Sensor status changes.
TEST(FakePowerManagerClientTest, KeyboardAmbientLightSensorEnabled) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  FakePowerManagerClient client;
  TestObserver test_observer;

  client.AddObserver(&test_observer);

  // The Keyboard Ambient Light Sensor is enabled by default.
  EXPECT_FALSE(test_observer.last_keyboard_ambient_light_sensor_change()
                   .has_sensor_enabled());
  EXPECT_FALSE(
      test_observer.last_keyboard_ambient_light_sensor_change().has_cause());

  // Turn the Keyboard Ambient Light Sensor off, and check that observers are
  // notified asynchronously.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  client.SetKeyboardAmbientLightSensorEnabled(set_als_request);

  // The Keyboard Ambient Light Sensor should not be disabled synchronously,
  // since the real client waits for a response from Power Manager.
  EXPECT_FALSE(test_observer.last_keyboard_ambient_light_sensor_change()
                   .has_sensor_enabled());
  EXPECT_FALSE(
      test_observer.last_keyboard_ambient_light_sensor_change().has_cause());
  base::RunLoop().RunUntilIdle();

  // The Keyboard Ambient Light Sensor should be disabled now.
  EXPECT_TRUE(test_observer.last_keyboard_ambient_light_sensor_change()
                  .has_sensor_enabled());
  EXPECT_FALSE(test_observer.last_keyboard_ambient_light_sensor_change()
                   .sensor_enabled());
  // The change cause should be USER_REQUEST_SETTINGS_APP because the change was
  // triggered via the PowerManagerClient function.
  EXPECT_TRUE(
      test_observer.last_keyboard_ambient_light_sensor_change().has_cause());
  EXPECT_EQ(
      test_observer.last_keyboard_ambient_light_sensor_change().cause(),
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // Turn the Keyboard Ambient Light Sensor on, and check that observers are
  // notified asynchronously.
  set_als_request.set_sensor_enabled(true);
  client.SetKeyboardAmbientLightSensorEnabled(set_als_request);

  // The Keyboard Ambient Light Sensor should not be enabled synchronously,
  // since the real client waits for a response from Power Manager.
  EXPECT_TRUE(test_observer.last_keyboard_ambient_light_sensor_change()
                  .has_sensor_enabled());
  EXPECT_FALSE(test_observer.last_keyboard_ambient_light_sensor_change()
                   .sensor_enabled());
  base::RunLoop().RunUntilIdle();

  // The Keyboard Ambient Light Sensor should be enabled now.
  EXPECT_TRUE(test_observer.last_keyboard_ambient_light_sensor_change()
                  .sensor_enabled());
  // The cause should be the same as before.
  EXPECT_TRUE(
      test_observer.last_keyboard_ambient_light_sensor_change().has_cause());
  EXPECT_EQ(
      test_observer.last_keyboard_ambient_light_sensor_change().cause(),
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
}

// Test that GetAmbientLightSensorEnabled works correctly.
TEST(FakePowerManagerClientTest, GetAmbientLightSensorEnabled) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  FakePowerManagerClient client;

  {
    power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
    set_als_request.set_sensor_enabled(true);
    client.SetAmbientLightSensorEnabled(set_als_request);

    bool called = false;
    client.GetAmbientLightSensorEnabled(base::BindOnce(
        [](std::reference_wrapper<bool> called_ref,
           std::optional<bool> is_ambient_light_sensor_enabled) {
          // The callback should be called with a value indicating that the
          // ambient light sensor is enabled.
          EXPECT_TRUE(is_ambient_light_sensor_enabled.value());

          // Indicate that this function was called.
          called_ref.get() = true;
        },
        std::ref(called)));

    // Result should be asynchronous.
    EXPECT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }

  {
    power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
    set_als_request.set_sensor_enabled(false);
    client.SetAmbientLightSensorEnabled(set_als_request);

    bool called = false;
    client.GetAmbientLightSensorEnabled(base::BindOnce(
        [](std::reference_wrapper<bool> called_ref,
           std::optional<bool> is_ambient_light_sensor_enabled) {
          // The callback should be called with a value indicating that the
          // ambient light sensor is not enabled.
          EXPECT_FALSE(is_ambient_light_sensor_enabled.value());

          // Indicate that this function was called.
          called_ref.get() = true;
        },
        std::ref(called)));

    // Result should be asynchronous.
    EXPECT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }
}

}  // namespace chromeos
