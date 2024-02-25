// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/adapters.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/settings/pages/power/device_power_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace ash::settings {

namespace {

using ::chromeos::PowerPolicyController;

PrefService* GetPrefs() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs();
}

}  // namespace

class TestPowerHandler : public PowerHandler {
 public:
  explicit TestPowerHandler(PrefService* prefs) : PowerHandler(prefs) {}

  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using PowerHandler::set_web_ui;
};

class PowerHandlerTest : public InProcessBrowserTest {
 protected:
  struct DevicePowerSettings {
    // Initialize with initial settings.
    DevicePowerSettings() {
      possible_ac_behaviors.insert(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP);
      possible_ac_behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_OFF);
      possible_ac_behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_ON);
      possible_battery_behaviors.insert(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP);
      possible_battery_behaviors.insert(
          PowerHandler::IdleBehavior::DISPLAY_OFF);
      possible_battery_behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_ON);
    }

    std::set<PowerHandler::IdleBehavior> possible_ac_behaviors;
    std::set<PowerHandler::IdleBehavior> possible_battery_behaviors;
    PowerHandler::IdleBehavior current_ac_behavior =
        PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP;
    PowerHandler::IdleBehavior current_battery_behavior =
        PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP;
    bool ac_idle_managed = false;
    bool battery_idle_managed = false;
    PowerPolicyController::Action lid_closed_behavior =
        PowerPolicyController::ACTION_SUSPEND;
    bool lid_closed_controlled = false;
    bool has_lid = true;
    bool adaptive_charging = true;
    bool adaptive_charging_managed = false;
    bool battery_saver_feature_enabled = true;
  };

  PowerHandlerTest() = default;

  PowerHandlerTest(const PowerHandlerTest&) = delete;
  PowerHandlerTest& operator=(const PowerHandlerTest&) = delete;

  ~PowerHandlerTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    // Initialize user policy.
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kBatterySaver);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestPowerHandler>(GetPrefs());
    test_api_ = std::make_unique<PowerHandler::TestAPI>(handler_.get());
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    test_api_.reset();
    handler_.reset();
  }

  // Returns a JSON representation of the contents of the last message sent to
  // WebUI about settings being changed.
  [[nodiscard]] std::string GetLastSettingsChangedMessage() {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_.call_data())) {
      const std::string* name = data->arg1()->GetIfString();
      if (data->function_name() != "cr.webUIListenerCallback" || !name ||
          *name != PowerHandler::kPowerManagementSettingsChangedName) {
        continue;
      }
      if (!data->arg2()->is_dict()) {
        ADD_FAILURE() << "Failed to get dict from " << *name << " message";
        continue;
      }
      const base::Value::Dict& dict = data->arg2()->GetDict();
      std::string out;
      EXPECT_TRUE(base::JSONWriter::Write(dict, &out));
      return out;
    }

    ADD_FAILURE() << PowerHandler::kPowerManagementSettingsChangedName
                  << " message was not sent";
    return std::string();
  }

  // Returns a string for the given |settings|. Used to verify expected
  // settings are sent to the UI.
  std::string ToString(const DevicePowerSettings& settings) {
    auto dict =
        base::Value::Dict()
            .Set(PowerHandler::kCurrentAcIdleBehaviorKey,
                 static_cast<int>(settings.current_ac_behavior))
            .Set(PowerHandler::kCurrentBatteryIdleBehaviorKey,
                 static_cast<int>(settings.current_battery_behavior))
            .Set(PowerHandler::kAcIdleManagedKey, settings.ac_idle_managed)
            .Set(PowerHandler::kBatteryIdleManagedKey,
                 settings.battery_idle_managed)
            .Set(PowerHandler::kLidClosedBehaviorKey,
                 settings.lid_closed_behavior)
            .Set(PowerHandler::kLidClosedControlledKey,
                 settings.lid_closed_controlled)
            .Set(PowerHandler::kHasLidKey, settings.has_lid)
            .Set(PowerHandler::kAdaptiveChargingKey, settings.adaptive_charging)
            .Set(PowerHandler::kAdaptiveChargingManagedKey,
                 settings.adaptive_charging_managed)
            .Set(PowerHandler::kBatterySaverFeatureEnabledKey,
                 settings.battery_saver_feature_enabled);

    base::Value::List* list =
        dict.EnsureList(PowerHandler::kPossibleAcIdleBehaviorsKey);
    for (auto idle_behavior : settings.possible_ac_behaviors) {
      list->Append(static_cast<int>(idle_behavior));
    }

    list = dict.EnsureList(PowerHandler::kPossibleBatteryIdleBehaviorsKey);
    for (auto idle_behavior : settings.possible_battery_behaviors) {
      list->Append(static_cast<int>(idle_behavior));
    }

    std::string out;
    EXPECT_TRUE(base::JSONWriter::Write(dict, &out));
    return out;
  }

  // Returns the user-set value of the integer pref identified by |name| or -1
  // if the pref is unset.
  int GetIntPref(const std::string& name) {
    if (!GetPrefs()->HasPrefPath(name)) {
      return -1;
    }
    return GetPrefs()->GetInteger(name);
  }

  // Trigger power pref managed change.
  void UpdateChromePolicy(policy::PolicyMap* policy_map) {
    provider_.UpdateChromePolicy(*policy_map);
    base::RunLoop().RunUntilIdle();
  }

  // Sets a policy update which will cause power pref managed change.
  void SetPolicyForPolicyKey(policy::PolicyMap* policy_map,
                             const std::string& policy_key,
                             base::Value value) {
    policy_map->Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    std::move(value), nullptr);
    UpdateChromePolicy(policy_map);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestPowerHandler> handler_;
  std::unique_ptr<TestPowerHandler::TestAPI> test_api_;

  content::TestWebUI web_ui_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

// Verifies that settings are sent to WebUI when requested.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendInitialSettings) {
  test_api_->RequestPowerManagementSettings();
  // Initialized to initial settings.
  DevicePowerSettings settings;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that WebUI receives updated settings when the lid state changes.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendSettingsForLidStateChanges) {
  chromeos::FakePowerManagerClient::Get()->SetLidState(
      chromeos::PowerManagerClient::LidState::NOT_PRESENT, base::TimeTicks());

  DevicePowerSettings settings;
  settings.has_lid = false;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  chromeos::FakePowerManagerClient::Get()->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, base::TimeTicks());
  settings.has_lid = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that when various prefs are controlled, the corresponding
// settings are reported as controlled/managed to WebUI.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendSettingsForControlledPrefs) {
  policy::PolicyMap policy_map;
  // Making an arbitrary AC delay pref managed should result in the AC idle
  // setting being reported as managed.
  SetPolicyForPolicyKey(&policy_map, policy::key::kScreenDimDelayAC,
                        base::Value(10000));
  DevicePowerSettings settings;
  settings.ac_idle_managed = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Ditto for battery delay pref managed.
  SetPolicyForPolicyKey(&policy_map, policy::key::kScreenDimDelayBattery,
                        base::Value(10000));
  settings.battery_idle_managed = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Ditto for making the lid action pref managed.
  SetPolicyForPolicyKey(&policy_map, policy::key::kLidCloseAction,
                        base::Value(PowerPolicyController::ACTION_SUSPEND));
  settings.lid_closed_controlled = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Ditto for making the adaptive charging pref managed.
  SetPolicyForPolicyKey(&policy_map,
                        policy::key::kDevicePowerAdaptiveChargingEnabled,
                        base::Value(true));
  settings.adaptive_charging_managed = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that idle-related prefs (when not managed by enterpise policy)
// are distilled into the proper WebUI settings.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendIdleSettingForPrefChanges) {
  // Initial power settings.
  DevicePowerSettings settings;
  // Set a AC do-nothing idle action and a AC nonzero screen-off delay. User
  // should see all three options (DISPLAY_ON, DISPLAY_OFF and
  // DISPLAY_OFF_SLEEP) and the selected setting when on AC should be set to
  // DISPLAY_OFF.
  GetPrefs()->Set(ash::prefs::kPowerAcIdleAction,
                  base::Value(PowerPolicyController::ACTION_DO_NOTHING));
  GetPrefs()->Set(ash::prefs::kPowerAcScreenOffDelayMs, base::Value(10000));

  // Current AC idle behavior should be DISPLAY_OFF.
  settings.current_ac_behavior = PowerHandler::IdleBehavior::DISPLAY_OFF;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Now set the battery screen off delay to zero along with battery do-nothing
  // idle action and check that the selected setting goes to "display on" when
  // on battery.
  GetPrefs()->Set(ash::prefs::kPowerBatteryIdleAction,
                  base::Value(PowerPolicyController::ACTION_DO_NOTHING));
  GetPrefs()->Set(ash::prefs::kPowerBatteryScreenOffDelayMs, base::Value(0));

  // Current battery idle behavior should be DISPLAY_ON.
  settings.current_battery_behavior = PowerHandler::IdleBehavior::DISPLAY_ON;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that idle-related prefs when managed by enterpise policy are
// distilled into the proper WebUI settings.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendManagedIdleSettingForPrefChanges) {
  policy::PolicyMap policy_map;
  // Set Enterpise policy that forces AC idle action to suspend. Only possible
  // AC idle option visible to the user should be DISPLAY_OFF_SLEEP and the
  // current should also be set to same.
  SetPolicyForPolicyKey(
      &policy_map, policy::key::kIdleActionAC,
      base::Value(chromeos::PowerPolicyController::ACTION_SUSPEND));
  DevicePowerSettings settings;
  std::set<PowerHandler::IdleBehavior> behaviors;
  behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP);
  settings.possible_ac_behaviors = behaviors;
  settings.ac_idle_managed = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Set Enterpise policy that forces battery idle action to Shutdown. Only
  // possible battery idle option visible to the user then should be SHUT_DOWN
  // and the default should also be set to same.
  SetPolicyForPolicyKey(
      &policy_map, policy::key::kIdleActionBattery,
      base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN));
  behaviors.clear();
  behaviors.insert(PowerHandler::IdleBehavior::SHUT_DOWN);
  settings.possible_battery_behaviors = behaviors;
  settings.current_battery_behavior = PowerHandler::IdleBehavior::SHUT_DOWN;
  settings.battery_idle_managed = true;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
  // Erase battery idle action.
  policy_map.Erase(policy::key::kIdleActionBattery);

  // Set battery idle action to DO_NOTHING in Enterpise policy. The user then
  // should not see DISPLAY_OFF_SLEEP in available options.
  SetPolicyForPolicyKey(
      &policy_map, policy::key::kIdleActionBattery,
      base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING));
  behaviors.clear();
  behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_OFF);
  behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_ON);
  settings.possible_battery_behaviors = behaviors;
  settings.current_battery_behavior = PowerHandler::IdleBehavior::DISPLAY_OFF;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Set battery screen delay in Enterprise policy on top of DO_NOTHING idle
  // action. The user should see only see DISPLAY_OFF as the possible battery
  // idle action.
  SetPolicyForPolicyKey(&policy_map, policy::key::kScreenOffDelayBattery,
                        base::Value(10000));
  behaviors.clear();
  behaviors.insert(PowerHandler::IdleBehavior::DISPLAY_OFF);
  settings.possible_battery_behaviors = behaviors;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  // Now stop enforcing battery idle action (to DO_NOTHING) in enterprise
  // policy. The user should see DISPLAY_OFF and DISPLAY_OFF_SLEEP as
  // the possible battery idle actions.
  policy_map.Erase(policy::key::kIdleActionBattery);
  UpdateChromePolicy(&policy_map);
  settings.possible_battery_behaviors.insert(
      PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP);
  settings.current_battery_behavior =
      PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that the lid-closed pref's value is sent directly to WebUI.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendLidSettingForPrefChanges) {
  GetPrefs()->Set(ash::prefs::kPowerLidClosedAction,
                  base::Value(PowerPolicyController::ACTION_SHUT_DOWN));
  DevicePowerSettings settings;
  settings.lid_closed_behavior = PowerPolicyController::ACTION_SHUT_DOWN;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());

  GetPrefs()->Set(ash::prefs::kPowerLidClosedAction,
                  base::Value(PowerPolicyController::ACTION_STOP_SESSION));
  settings.lid_closed_behavior = PowerPolicyController::ACTION_STOP_SESSION;
  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that the adaptive charging pref's value is sent directly to WebUI.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendAdaptiveCharging) {
  GetPrefs()->Set(ash::prefs::kPowerAdaptiveChargingEnabled, base::Value(true));

  // Current AC idle behavior should be DISPLAY_OFF.
  DevicePowerSettings settings;
  settings.adaptive_charging = true;

  EXPECT_EQ(ToString(settings), GetLastSettingsChangedMessage());
}

// Verifies that requests from WebUI to update the idle behavior update prefs
// appropriately.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SetIdleBehavior) {
  // Request the "Keep display on" AC setting and check that prefs are set
  // appropriately.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_ON,
                             true /* is_ac */);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenLockDelayMs));

  // "Turn off display" battery setting should set the battery idle pref but
  // clear the battery screen delays.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_OFF,
                             false /* is_battery */);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenLockDelayMs));

  // Now switch to the "Keep display on" battery setting (to set the prefs
  // again) and check that the "Turn off display and sleep" battery setting
  // clears all the battery prefs.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_ON,
                             false /* is_battery */);
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
                             false /* is_battery */);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenLockDelayMs));
}

// Verifies that requests from WebUI to change the lid behavior update the pref.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SetLidBehavior) {
  // The "do nothing" setting should update the pref.
  test_api_->SetLidClosedBehavior(PowerPolicyController::ACTION_DO_NOTHING);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerLidClosedAction));

  // Selecting the "suspend" setting should just clear the pref.
  test_api_->SetLidClosedBehavior(PowerPolicyController::ACTION_SUSPEND);
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerLidClosedAction));
}

// Verifies that requests from WebUI to enable / disable the adaptive charging
// feature updates prefs appropriately.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SetAdaptiveCharging) {
  const PrefService::Preference* pref =
      GetPrefs()->FindPreference(ash::prefs::kPowerAdaptiveChargingEnabled);
  ASSERT_NE(nullptr, pref);

  EXPECT_EQ(true, pref->GetValue()->GetBool());
  test_api_->SetAdaptiveCharging(false);
  EXPECT_EQ(false, pref->GetValue()->GetBool());
}

}  // namespace ash::settings
