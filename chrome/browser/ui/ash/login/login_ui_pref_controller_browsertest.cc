// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_ui_pref_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/system/fake_input_device_settings.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Gets current mouse setting primary button right value.
bool GetSystemMousePrimaryButtonRight() {
  return system::InputDeviceSettings::Get()
      ->GetFakeInterface()
      ->current_mouse_settings()
      .GetPrimaryButtonRight();
}

// Gets current pointing_stick primary button right value.
bool GetSystemPointingStickPrimaryButtonRight() {
  return system::InputDeviceSettings::Get()
      ->GetFakeInterface()
      ->current_pointing_stick_settings()
      .GetPrimaryButtonRight();
}

// Set "owner.pointing_stick.primary_right" preference.
void SetOwnerPointingStickPrimaryRight(bool val) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kOwnerPrimaryPointingStickButtonRight, val);
}

// Gets current pointing_stick primary button right value.
bool GetSystemTouchpadTapToClick() {
  return system::InputDeviceSettings::Get()
      ->GetFakeInterface()
      ->current_touchpad_settings()
      .GetTapToClick();
}

// Set "owner.touchpad.enable_tap_to_click" preference.
void SetOwnerTouchpadEnableTapToClick(bool val) {
  g_browser_process->local_state()->SetBoolean(prefs::kOwnerTapToClickEnabled,
                                               val);
}

}  // namespace

class LoginUIPrefControllerTest : public LoginManagerTest {
 public:
  LoginUIPrefControllerTest() = default;

  LoginUIPrefControllerTest(const LoginUIPrefControllerTest&) = delete;
  LoginUIPrefControllerTest& operator=(const LoginUIPrefControllerTest&) =
      delete;

  ~LoginUIPrefControllerTest() override = default;

  void RefreshDevicePolicy() { policy_helper_.RefreshDevicePolicy(); }

  // Sets DeviceLoginScreenPrimaryMouseButtonSwitch proto value.
  void SetDeviceLoginScreenPrimaryMouseButtonSwitch(bool val) {
    policy_helper_.device_policy()
        ->payload()
        .mutable_login_screen_primary_mouse_button_switch()
        ->set_value(val);
  }

 private:
  LoginManagerMixin login_mixin_{&mixin_host_};
  policy::DevicePolicyCrosTestHelper policy_helper_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(LoginUIPrefControllerTest,
                       DeviceLoginScreenPrimaryMouseButtonSwitch) {
  EXPECT_FALSE(GetSystemMousePrimaryButtonRight());

  SetDeviceLoginScreenPrimaryMouseButtonSwitch(true);
  RefreshDevicePolicy();
  WaitForPrefValue(g_browser_process->local_state(),
                   prefs::kOwnerPrimaryMouseButtonRight, base::Value(true));
  EXPECT_TRUE(GetSystemMousePrimaryButtonRight());

  SetDeviceLoginScreenPrimaryMouseButtonSwitch(false);
  RefreshDevicePolicy();
  WaitForPrefValue(g_browser_process->local_state(),
                   prefs::kOwnerPrimaryMouseButtonRight, base::Value(false));
  EXPECT_FALSE(GetSystemMousePrimaryButtonRight());
}

IN_PROC_BROWSER_TEST_F(LoginUIPrefControllerTest,
                       OwnerPrimaryPointingStickButtonRight) {
  EXPECT_FALSE(GetSystemPointingStickPrimaryButtonRight());
  SetOwnerPointingStickPrimaryRight(true);
  EXPECT_TRUE(GetSystemPointingStickPrimaryButtonRight());
  SetOwnerPointingStickPrimaryRight(false);
  EXPECT_FALSE(GetSystemPointingStickPrimaryButtonRight());
}

IN_PROC_BROWSER_TEST_F(LoginUIPrefControllerTest, OwnerTapToClickEnabled) {
  EXPECT_TRUE(GetSystemTouchpadTapToClick());
  SetOwnerTouchpadEnableTapToClick(false);
  EXPECT_FALSE(GetSystemTouchpadTapToClick());
  SetOwnerTouchpadEnableTapToClick(true);
  EXPECT_TRUE(GetSystemTouchpadTapToClick());
}

}  // namespace ash
