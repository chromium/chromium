// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/new_window/chrome_new_window_client.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUserName1[] = "test1@test.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUserName2[] = "test2@test.com";
constexpr char kTestUser2GaiaId[] = "2222222222";

void CreateAndStartUserSession(const AccountId& account_id) {
  using ::ash::ProfileHelper;
  using session_manager::SessionManager;

  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetProfileRequiresPolicy(
      account_id, user_manager::ProfileRequiresPolicy::kNoPolicyRequired);
  const std::string user_id_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  SessionManager::Get()->CreateSession(account_id, user_id_hash, false);
  profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));
  SessionManager::Get()->SessionStarted();
}

// Give the underlying function a clearer name.
Browser* GetLastActiveBrowser() {
  return chrome::FindLastActive();
}

}  // namespace

using ChromeNewWindowClientBrowserTest = InProcessBrowserTest;

// Tests that when we open a new window by pressing 'Ctrl-N', we should use the
// current active window's profile to determine on which profile's desktop we
// should open a new window.
//
// Test is flaky. See https://crbug.com/884118
IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest,
                       DISABLED_NewWindowForActiveWindowProfileTest) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser1GaiaId));
  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  Browser* browser1 = CreateBrowser(profile1);
  // The newly created window should be created for the current active profile.
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/false,
      /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Login another user and make sure the current active user changes.
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName2, kTestUser2GaiaId));
  Profile* profile2 = ProfileManager::GetActiveUserProfile();
  EXPECT_NE(profile1, profile2);

  Browser* browser2 = CreateBrowser(profile2);
  // The newly created window should be created for the current active window's
  // profile, which is |profile2|.
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/false,
      /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile2);

  // After activating |browser1|, the newly created window should be created
  // against |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/false,
      /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Test for incognito windows.
  // The newly created incognito window should be created against the current
  // active |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile1);

  // The newly created incognito window should be created against the current
  // active |browser2|'s profile.
  browser2->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile2);
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, IncognitoDisabled) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser2GaiaId));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Disabling incognito mode disables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Enabling incognito mode enables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), policy::IncognitoModeAvailability::kEnabled);
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(GetLastActiveBrowser()->profile()->IsIncognitoProfile());
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, IncognitoForced) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Forcing Incognito mode, opens any consequent window & tabs in Incognito.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), policy::IncognitoModeAvailability::kForced);

  // Deactivating the current normal profile browser
  Browser* regular_browser = GetLastActiveBrowser();
  regular_browser->window()->Deactivate();

  // NewTab should open a new browser window in Incognito
  ChromeNewWindowClient::Get()->NewTab();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  Browser* incognito_browser = GetLastActiveBrowser();
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());

  // After deactivating browsers, NewTab should open a new Incognito Tab only
  incognito_browser->window()->Deactivate();
  regular_browser->window()->Deactivate();
  ChromeNewWindowClient::Get()->NewTab();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
}
