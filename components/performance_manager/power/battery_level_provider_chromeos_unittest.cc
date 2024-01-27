// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/power/battery_level_provider_chromeos.h"

#include "base/power_monitor/battery_state_sampler.h"
#include "base/scoped_observation.h"
#include "base/test/gtest_util.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/performance_manager/power/dbus_power_manager_sampling_event_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::power {

namespace {

class TestBatteryStateSamplerObserver
    : public base::BatteryStateSampler::Observer {
 public:
  // base::BatteryStateSampler::Observer:
  void OnBatteryStateSampled(
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state) override {
    last_sampled_state_ = battery_state;
  }

  std::optional<base::BatteryLevelProvider::BatteryState> last_sampled_state_ =
      std::nullopt;
};

}  // namespace

class BatteryLevelProviderChromeOSTest : public ::testing::Test {
 protected:
  void InitProvider(chromeos::FakePowerManagerClient* power_manager_client) {
    battery_level_provider_ =
        std::make_unique<BatteryLevelProviderChromeOS>(power_manager_client);
  }

  std::optional<base::BatteryLevelProvider::BatteryState> GetBatteryState() {
    std::optional<base::BatteryLevelProvider::BatteryState> state;
    battery_level_provider_->GetBatteryState(base::BindOnce(
        [](std::optional<base::BatteryLevelProvider::BatteryState>* out,
           const std::optional<base::BatteryLevelProvider::BatteryState>&
               state) { *out = state; },
        &state));

    return state;
  }

  chromeos::FakePowerManagerClient fake_power_manager_client_;
  std::unique_ptr<BatteryLevelProviderChromeOS> battery_level_provider_;
};

TEST_F(BatteryLevelProviderChromeOSTest, NoPowerManager) {
  EXPECT_DCHECK_DEATH(InitProvider(nullptr));
}

TEST_F(BatteryLevelProviderChromeOSTest, NoLastStatus) {
  fake_power_manager_client_.UpdatePowerProperties(std::nullopt);
  InitProvider(&fake_power_manager_client_);

  EXPECT_FALSE(GetBatteryState());
}

TEST_F(BatteryLevelProviderChromeOSTest, NoBattery) {
  auto props = power_manager::PowerSupplyProperties();

  props.set_battery_state(power_manager::PowerSupplyProperties::NOT_PRESENT);
  fake_power_manager_client_.UpdatePowerProperties(props);
  InitProvider(&fake_power_manager_client_);

  std::optional<base::BatteryLevelProvider::BatteryState> state =
      GetBatteryState();
  EXPECT_TRUE(state);
  EXPECT_EQ(0, state->battery_count);
  EXPECT_TRUE(state->is_external_power_connected);
}

TEST_F(BatteryLevelProviderChromeOSTest, BatteryReportsMAh) {
  auto props = power_manager::PowerSupplyProperties();

  props.set_battery_state(power_manager::PowerSupplyProperties::DISCHARGING);
  props.set_external_power(power_manager::PowerSupplyProperties::DISCONNECTED);
  props.set_battery_charge(100.0);
  props.set_battery_charge_full(500.0);
  fake_power_manager_client_.UpdatePowerProperties(props);
  InitProvider(&fake_power_manager_client_);

  std::optional<base::BatteryLevelProvider::BatteryState> state =
      GetBatteryState();
  EXPECT_TRUE(state);
  EXPECT_EQ(1, state->battery_count);
  EXPECT_FALSE(state->is_external_power_connected);
  EXPECT_EQ(100000UL, state->current_capacity);
  EXPECT_EQ(500000UL, state->full_charged_capacity);
  EXPECT_EQ(base::BatteryLevelProvider::BatteryLevelUnit::kMAh,
            state->charge_unit);
}

TEST_F(BatteryLevelProviderChromeOSTest, BatteryReportsMWh) {
  auto props = power_manager::PowerSupplyProperties();

  props.set_battery_state(power_manager::PowerSupplyProperties::DISCHARGING);
  props.set_external_power(power_manager::PowerSupplyProperties::DISCONNECTED);
  props.set_battery_charge(100.0);
  props.set_battery_charge_full(500.0);
  props.set_battery_voltage(12.0);
  fake_power_manager_client_.UpdatePowerProperties(props);
  InitProvider(&fake_power_manager_client_);

  std::optional<base::BatteryLevelProvider::BatteryState> state =
      GetBatteryState();
  EXPECT_TRUE(state);
  EXPECT_EQ(1, state->battery_count);
  EXPECT_FALSE(state->is_external_power_connected);
  // Capacity in mWh is charge in mAh * voltage
  EXPECT_EQ(1200000UL, state->current_capacity);
  EXPECT_EQ(6000000UL, state->full_charged_capacity);
  EXPECT_EQ(base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
            state->charge_unit);
}

TEST_F(BatteryLevelProviderChromeOSTest, BatteryReportsRelative) {
  auto props = power_manager::PowerSupplyProperties();

  props.set_battery_state(power_manager::PowerSupplyProperties::CHARGING);
  props.set_external_power(power_manager::PowerSupplyProperties::AC);
  props.set_battery_percent(20.0);
  fake_power_manager_client_.UpdatePowerProperties(props);
  InitProvider(&fake_power_manager_client_);

  std::optional<base::BatteryLevelProvider::BatteryState> state =
      GetBatteryState();
  EXPECT_TRUE(state);
  EXPECT_EQ(1, state->battery_count);
  EXPECT_TRUE(state->is_external_power_connected);
  EXPECT_EQ(20UL, state->current_capacity);
  EXPECT_EQ(100UL, state->full_charged_capacity);
  EXPECT_EQ(base::BatteryLevelProvider::BatteryLevelUnit::kRelative,
            state->charge_unit);
}

TEST_F(BatteryLevelProviderChromeOSTest, WorksWithBatteryStateSampler) {
  auto props = power_manager::PowerSupplyProperties();
  props.set_battery_state(power_manager::PowerSupplyProperties::DISCHARGING);
  props.set_external_power(power_manager::PowerSupplyProperties::DISCONNECTED);
  props.set_battery_charge(100.0);
  props.set_battery_charge_full(500.0);
  props.set_battery_voltage(12.0);
  fake_power_manager_client_.UpdatePowerProperties(props);

  base::BatteryStateSampler sampler(
      std::make_unique<DbusPowerManagerSamplingEventSource>(
          &fake_power_manager_client_),
      std::make_unique<BatteryLevelProviderChromeOS>(
          &fake_power_manager_client_));

  TestBatteryStateSamplerObserver obs;
  EXPECT_FALSE(obs.last_sampled_state_);
  base::ScopedObservation<base::BatteryStateSampler,
                          base::BatteryStateSampler::Observer>
      observation{&obs};
  observation.Observe(&sampler);

  // Observers are notified right away when the battery state changes.
  EXPECT_TRUE(obs.last_sampled_state_);
  EXPECT_EQ(1, obs.last_sampled_state_->battery_count);
  EXPECT_FALSE(obs.last_sampled_state_->is_external_power_connected);
  // Capacity in mWh is charge in mAh * voltage
  EXPECT_EQ(1200000UL, obs.last_sampled_state_->current_capacity);
  EXPECT_EQ(6000000UL, obs.last_sampled_state_->full_charged_capacity);
  EXPECT_EQ(base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
            obs.last_sampled_state_->charge_unit);

  // Changing the state will trigger the sampling event source, which will cause
  // the BatteryStateSampler to get the updated battery state.
  auto other_props = power_manager::PowerSupplyProperties();
  other_props.set_battery_state(power_manager::PowerSupplyProperties::CHARGING);
  other_props.set_external_power(power_manager::PowerSupplyProperties::AC);
  other_props.set_battery_percent(20.0);
  fake_power_manager_client_.UpdatePowerProperties(other_props);

  EXPECT_TRUE(obs.last_sampled_state_);
  EXPECT_EQ(1, obs.last_sampled_state_->battery_count);
  EXPECT_TRUE(obs.last_sampled_state_->is_external_power_connected);
  EXPECT_EQ(20UL, obs.last_sampled_state_->current_capacity);
  EXPECT_EQ(100UL, obs.last_sampled_state_->full_charged_capacity);
  EXPECT_EQ(base::BatteryLevelProvider::BatteryLevelUnit::kRelative,
            obs.last_sampled_state_->charge_unit);
}

}  // namespace performance_manager::power
