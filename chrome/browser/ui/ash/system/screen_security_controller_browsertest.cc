// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy/screen_switch_check_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ScreenSecurityControllerBrowserTest : public LoginManagerTest {
 public:
  ScreenSecurityControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

    login_mixin_.AppendRegularUsers(2);
  }

  ~ScreenSecurityControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    Shell::Get()
        ->screen_switch_check_controller()
        ->set_skip_cancel_dialog_for_testing(true);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ScreenSecurityControllerBrowserTest,
                       ScreenShareStopsOnUserSwitchWithVcEnabled) {
  // Login as the first user.
  LoginUser(login_mixin_.users()[0].account_id);

  // Switch to the second user to add them to the session.
  UserAddingScreen::Get()->Start();
  AddUser(login_mixin_.users()[1].account_id);

  // Switch back to the first user.
  SessionControllerClientImpl::Get()->SwitchActiveUser(
      login_mixin_.users()[0].account_id);

  // Start screen sharing.
  bool stop_callback_called = false;
  auto stop_callback = base::BindRepeating([](bool* called) { *called = true; },
                                           &stop_callback_called);

  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStart(
      stop_callback, base::RepeatingClosure(), std::u16string());

  EXPECT_FALSE(stop_callback_called);

  // Switch to the second user.
  SessionControllerClientImpl::Get()->SwitchActiveUser(
      login_mixin_.users()[1].account_id);

  // The screen share should be stopped during the user switch.
  EXPECT_TRUE(stop_callback_called);
}

IN_PROC_BROWSER_TEST_F(ScreenSecurityControllerBrowserTest,
                       ScreenShareStopsOnNewUserLoginWithVcEnabled) {
  // Login as the first user.
  LoginUser(login_mixin_.users()[0].account_id);

  // Start screen sharing.
  bool stop_callback_called = false;
  auto stop_callback = base::BindRepeating([](bool* called) { *called = true; },
                                           &stop_callback_called);

  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStart(
      stop_callback, base::RepeatingClosure(), std::u16string());

  EXPECT_FALSE(stop_callback_called);

  // Switch to the second user to add them to the session.
  UserAddingScreen::Get()->Start();
  AddUser(login_mixin_.users()[1].account_id);

  // The screen share should be stopped when the new user signs in.
  EXPECT_TRUE(stop_callback_called);
}

}  // namespace ash
