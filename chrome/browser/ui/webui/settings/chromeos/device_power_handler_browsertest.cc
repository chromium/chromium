// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::_;

namespace chromeos {
namespace settings {

namespace {

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
  PowerHandlerTest() = default;
  ~PowerHandlerTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    // Initialize user policy.
    ON_CALL(provider_, IsInitializationComplete(_)).WillByDefault(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
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
  std::string GetLastSettingsChangedMessage() WARN_UNUSED_RESULT {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string name;
      const base::DictionaryValue* dict = nullptr;
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->GetAsString(&name) ||
          name != PowerHandler::kPowerManagementSettingsChangedName) {
        continue;
      }
      if (!data->arg2()->GetAsDictionary(&dict)) {
        ADD_FAILURE() << "Failed to get dict from " << name << " message";
        continue;
      }
      std::string out;
      EXPECT_TRUE(base::JSONWriter::Write(*dict, &out));
      return out;
    }

    ADD_FAILURE() << PowerHandler::kPowerManagementSettingsChangedName
                  << " message was not sent";
    return std::string();
  }

  // Returns a string for the given settings that can be compared against the
  // output of GetLastSettingsChangedMessage().
  std::string CreateSettingsChangedString(
      PowerHandler::IdleBehavior idle_behavior,
      bool idle_controlled,
      PowerPolicyController::Action lid_closed_behavior,
      bool lid_closed_controlled,
      bool has_lid) {
    base::DictionaryValue dict;
    dict.SetInteger(PowerHandler::kIdleBehaviorKey,
                    static_cast<int>(idle_behavior));
    dict.SetBoolean(PowerHandler::kIdleControlledKey, idle_controlled);
    dict.SetInteger(PowerHandler::kLidClosedBehaviorKey, lid_closed_behavior);
    dict.SetBoolean(PowerHandler::kLidClosedControlledKey,
                    lid_closed_controlled);
    dict.SetBoolean(PowerHandler::kHasLidKey, has_lid);

    std::string out;
    EXPECT_TRUE(base::JSONWriter::Write(dict, &out));
    return out;
  }

  // Returns the user-set value of the integer pref identified by |name| or -1
  // if the pref is unset.
  int GetIntPref(const std::string& name) {
    if (!GetPrefs()->HasPrefPath(name))
      return -1;
    return GetPrefs()->GetInteger(name);
  }

  // Sets a policy update which will cause power pref managed change.
  void SetPolicyForPolicyKey(policy::PolicyMap* policy_map,
                             const std::string& policy_key,
                             std::unique_ptr<base::Value> value) {
    policy_map->Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    std::move(value), nullptr);
    provider_.UpdateChromePolicy(*policy_map);
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<TestPowerHandler> handler_;
  std::unique_ptr<TestPowerHandler::TestAPI> test_api_;

  content::TestWebUI web_ui_;

  policy::MockConfigurationPolicyProvider provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerHandlerTest);
};

// Verifies that settings are sent to WebUI when requested.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendInitialSettings) {
  test_api_->RequestPowerManagementSettings();
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());
}

// Verifies that WebUI receives updated settings when the lid state changes.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendSettingsForLidStateChanges) {
  chromeos::FakePowerManagerClient::Get()->SetLidState(
      PowerManagerClient::LidState::NOT_PRESENT, base::TimeTicks());
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, false /* has_lid */),
      GetLastSettingsChangedMessage());

  chromeos::FakePowerManagerClient::Get()->SetLidState(
      PowerManagerClient::LidState::OPEN, base::TimeTicks());
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());
}

// Verifies that when various prefs are controlled, the corresponding settings
// are reported as controlled to WebUI.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendSettingsForControlledPrefs) {
  policy::PolicyMap policy_map;
  // Making an arbitrary delay pref managed should result in the idle setting
  // being reported as controlled.
  SetPolicyForPolicyKey(&policy_map, policy::key::kScreenDimDelayAC,
                        std::make_unique<base::Value>(10000));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          true /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());

  // Ditto for making the lid action pref managed.
  SetPolicyForPolicyKey(
      &policy_map, policy::key::kLidCloseAction,
      std::make_unique<base::Value>(PowerPolicyController::ACTION_SUSPEND));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          true /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          true /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());
}

// Verifies that idle-related prefs are distilled into the proper WebUI
// settings.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendIdleSettingForPrefChanges) {
  // Set a do-nothing idle action and a nonzero screen-off delay.
  GetPrefs()->Set(ash::prefs::kPowerAcIdleAction,
                  base::Value(PowerPolicyController::ACTION_DO_NOTHING));
  GetPrefs()->Set(ash::prefs::kPowerAcScreenOffDelayMs, base::Value(10000));
  EXPECT_EQ(CreateSettingsChangedString(PowerHandler::IdleBehavior::DISPLAY_OFF,
                                        false /* idle_controlled */,
                                        PowerPolicyController::ACTION_SUSPEND,
                                        false /* lid_closed_controlled */,
                                        true /* has_lid */),
            GetLastSettingsChangedMessage());

  // Now set the delay to zero and check that the setting goes to "display on".
  GetPrefs()->Set(ash::prefs::kPowerAcScreenOffDelayMs, base::Value(0));
  EXPECT_EQ(CreateSettingsChangedString(PowerHandler::IdleBehavior::DISPLAY_ON,
                                        false /* idle_controlled */,
                                        PowerPolicyController::ACTION_SUSPEND,
                                        false /* lid_closed_controlled */,
                                        true /* has_lid */),
            GetLastSettingsChangedMessage());

  // Other idle actions should result in an "other" setting.
  GetPrefs()->Set(ash::prefs::kPowerAcIdleAction,
                  base::Value(PowerPolicyController::ACTION_STOP_SESSION));
  EXPECT_EQ(CreateSettingsChangedString(
                PowerHandler::IdleBehavior::OTHER, false /* idle_controlled */,
                PowerPolicyController::ACTION_SUSPEND,
                false /* lid_closed_controlled */, true /* has_lid */),
            GetLastSettingsChangedMessage());
}

// Verifies that the lid-closed pref's value is sent directly to WebUI.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SendLidSettingForPrefChanges) {
  GetPrefs()->Set(ash::prefs::kPowerLidClosedAction,
                  base::Value(PowerPolicyController::ACTION_SHUT_DOWN));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SHUT_DOWN,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());

  GetPrefs()->Set(ash::prefs::kPowerLidClosedAction,
                  base::Value(PowerPolicyController::ACTION_STOP_SESSION));
  EXPECT_EQ(CreateSettingsChangedString(
                PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
                false /* idle_controlled */,
                PowerPolicyController::ACTION_STOP_SESSION,
                false /* lid_closed_controlled */, true /* has_lid */),
            GetLastSettingsChangedMessage());
}

// Verifies that requests from WebUI to update the idle behavior update prefs
// appropriately.
IN_PROC_BROWSER_TEST_F(PowerHandlerTest, SetIdleBehavior) {
  // Request the "Keep display on" setting and check that prefs are set
  // appropriately.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_ON);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(ash::prefs::kPowerBatteryScreenLockDelayMs));

  // "Turn off display" should set the idle prefs but clear the screen
  // delays.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_OFF);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(ash::prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerBatteryScreenLockDelayMs));

  // Now switch to the "Keep display on" setting (to set the prefs again) and
  // check that the "Turn off display and sleep" setting clears all the prefs.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_ON);
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP);
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(ash::prefs::kPowerAcScreenLockDelayMs));
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

}  // namespace settings
}  // namespace chromeos
