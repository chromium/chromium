// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/local_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/gurl.h"

using chromeos::ProfileHelper;
using user_manager::UserManager;

using SystemTrayClientEnterpriseTest = policy::DevicePolicyCrosBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseTest, TrayEnterprise) {
  auto test_api = ash::SystemTrayTestApi::Create();

  // Managed devices show an item in the menu.
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_TRAY_ENTERPRISE,
                                            true /* open_tray */));

  if (ash::features::IsManagedDeviceUIRedesignEnabled()) {
    // The text shows the domain.
    EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
                                         ui::GetChromeOSDeviceName(),
                                         base::UTF8ToUTF16("example.com")),
              test_api->GetBubbleViewText(ash::VIEW_ID_TRAY_ENTERPRISE_LABEL));
  } else {
    // The tooltip shows the domain.
    EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
                                         ui::GetChromeOSDeviceName(),
                                         base::UTF8ToUTF16("example.com")),
              test_api->GetBubbleViewTooltip(ash::VIEW_ID_TRAY_ENTERPRISE));
  }

  // Clicking the item opens the management page.
  test_api->ClickBubbleView(ash::VIEW_ID_TRAY_ENTERPRISE);
  EXPECT_EQ(
      GURL(chrome::kChromeUIManagementURL),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

class SystemTrayClientClockTest : public chromeos::LoginManagerTest {
 public:
  SystemTrayClientClockTest() : LoginManagerTest() {
    // Use consumer emails to avoid having to fake a policy fetch.
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }

  ~SystemTrayClientClockTest() override = default;

  void SetupUserProfile(const AccountId& account_id, bool use_24_hour_clock) {
    const user_manager::User* user = UserManager::Get()->FindUser(account_id);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetBoolean(prefs::kUse24HourClock, use_24_hour_clock);
    // Allow clock setting to be sent to ash over mojo.
    content::RunAllPendingInMessageLoop();
  }

 protected:
  AccountId account_id1_;
  AccountId account_id2_;
  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayClientClockTest);
};

// Test that clock type is taken from user profile for current active user.
IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, TestMultiProfile24HourClock) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  // Login a user with a 24-hour clock.
  LoginUser(account_id1_);
  SetupUserProfile(account_id1_, true /* use_24_hour_clock */);
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  // Add a user with a 12-hour clock.
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(account_id2_);
  SetupUserProfile(account_id2_, false /* use_24_hour_clock */);
  EXPECT_FALSE(tray_test_api->Is24HourClock());

  // Switch back to the user with the 24-hour clock.
  UserManager::Get()->SwitchActiveUser(account_id1_);
  // Allow clock setting to be sent to ash over mojo.
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tray_test_api->Is24HourClock());
}

// Test that on the login and lock screen clock type is taken from user profile
// of the focused pod.
IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, PRE_FocusedPod24HourClock) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  // Login a user with a 24-hour clock.
  LoginUser(account_id1_);
  SetupUserProfile(account_id1_, true /* use_24_hour_clock */);
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  // Add a user with a 12-hour clock.
  chromeos::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  SetupUserProfile(account_id2_, false /* use_24_hour_clock */);
  EXPECT_FALSE(tray_test_api->Is24HourClock());

  // Test lock screen.
  chromeos::ScreenLockerTester locker;
  locker.Lock();

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id1_));
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id2_));
  EXPECT_FALSE(tray_test_api->Is24HourClock());
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, FocusedPod24HourClock) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();
  // Test login screen.
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id1_));
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id2_));
  EXPECT_FALSE(tray_test_api->Is24HourClock());
}

class SystemTrayClientClockUnknownPrefTest
    : public SystemTrayClientClockTest,
      public chromeos::LocalStateMixin::Delegate {
 public:
  SystemTrayClientClockUnknownPrefTest() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kSystemUse24HourClock, true);
  }
  // chromeos::localStateMixin::Delegate:
  void SetUpLocalState() override {
    // First user does not have a preference.
    ASSERT_FALSE(user_manager::known_user::GetBooleanPref(
        account_id1_, ::prefs::kUse24HourClock, nullptr));

    // Set preference for the second user only.
    user_manager::known_user::SetBooleanPref(account_id2_,
                                             ::prefs::kUse24HourClock, false);
  }

 protected:
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::LocalStateMixin local_state_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientClockUnknownPrefTest, SwitchToDefault) {
  // Check default value.
  ASSERT_EQ(base::GetHourClockType(), base::k12HourClock);

  auto tray_test_api = ash::SystemTrayTestApi::Create();
  EXPECT_EQ(ash::LoginScreenTestApi::GetFocusedUser(), account_id1_);
  // Should be system setting because the first user does not have a preference.
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  // Check user with the set preference.
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id2_));
  EXPECT_FALSE(tray_test_api->Is24HourClock());

  // Should get back to the system settings.
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id1_));
  EXPECT_TRUE(tray_test_api->Is24HourClock());
}
