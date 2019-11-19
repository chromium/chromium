// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"

using chromeos::ProfileHelper;
using user_manager::UserManager;

using SystemTrayClientEnterpriseTest = policy::DevicePolicyCrosBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseTest, TrayEnterprise) {
  auto test_api = ash::SystemTrayTestApi::Create();

  // Managed devices show an item in the menu.
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_TRAY_ENTERPRISE,
                                            true /* open_tray */));
}

class SystemTrayClientClockTest : public chromeos::LoginManagerTest {
 public:
  SystemTrayClientClockTest()
      : LoginManagerTest(false /* should_launch_browser */,
                         true /* should_initialize_webui */),
        // Use consumer emails to avoid having to fake a policy fetch.
        account_id1_(
            AccountId::FromUserEmailGaiaId("user1@gmail.com", "1111111111")),
        account_id2_(
            AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222")) {}

  ~SystemTrayClientClockTest() override = default;

  void SetupUserProfile(const AccountId& account_id, bool use_24_hour_clock) {
    const user_manager::User* user = UserManager::Get()->FindUser(account_id);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetBoolean(prefs::kUse24HourClock, use_24_hour_clock);
    // Allow clock setting to be sent to ash over mojo.
    content::RunAllPendingInMessageLoop();
  }

 protected:
  const AccountId account_id1_;
  const AccountId account_id2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayClientClockTest);
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest,
                       PRE_TestMultiProfile24HourClock) {
  RegisterUser(account_id1_);
  RegisterUser(account_id2_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

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
