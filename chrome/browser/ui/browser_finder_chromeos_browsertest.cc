// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

class BrowserFinderWithDesksTest : public InProcessBrowserTest {
 public:
  BrowserFinderWithDesksTest() = default;

  BrowserFinderWithDesksTest(const BrowserFinderWithDesksTest&) = delete;
  BrowserFinderWithDesksTest& operator=(const BrowserFinderWithDesksTest&) =
      delete;

  ~BrowserFinderWithDesksTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Create three desks (two other than the default).
    auto* desks_controller = ash::DesksController::Get();
    desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);
    desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);
  }

  void ActivateBrowser(BrowserWindowInterface* browser) {
    browser->GetWindow()->Activate();
  }

  Browser* CreateTestBrowser() {
    Browser* new_browser = CreateBrowser(browser()->profile());
    new_browser->window()->Show();
    ActivateBrowser(new_browser);
    return new_browser;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserFinderWithDesksTest, FindAnyBrowser) {
  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(3u, desks_controller->desks().size());
  auto* desk_1 = desks_controller->desks()[0].get();
  auto* desk_2 = desks_controller->desks()[1].get();
  auto* desk_3 = desks_controller->desks()[2].get();

  BrowserWindowInterface* const browser_1 = CreateTestBrowser();
  CloseBrowserSynchronously(browser());
  SetBrowser(browser_1);
  auto* window_1 = browser_1->GetWindow()->GetNativeWindow();
  EXPECT_EQ(
      1u,
      ProfileBrowserCollection::GetForProfile(browser()->profile())->GetSize());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_EQ(browser_1, ui_test_utils::FindAnyBrowser(browser()->profile()));

  // Switch to desk_2 and create a browser there.
  ash::ActivateDesk(desk_2);
  EXPECT_TRUE(desk_2->is_active());
  Browser* browser_2 = CreateTestBrowser();
  auto* window_2 = browser_2->GetWindow()->GetNativeWindow();
  EXPECT_EQ(
      2u,
      ProfileBrowserCollection::GetForProfile(browser()->profile())->GetSize());
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_2));

  // FindAnyBrowser should return the MRU browser, which is browser_2 in this
  // case.
  EXPECT_EQ(browser_2, ui_test_utils::FindAnyBrowser(browser()->profile()));

  // Switch to desk_3, no browsers on this desk, however, FindAnyBrowser
  // should still return browser_2.
  ash::ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(window_2));
  EXPECT_EQ(browser_2, ui_test_utils::FindAnyBrowser(browser()->profile()));

  // Switch to desk_1 by activating browser_1. When we switch back to desk_3,
  // FindAnyBrowser() will return browser_1 as the MRU browser.
  ash::DeskSwitchAnimationWaiter waiter;
  ActivateBrowser(browser_1);
  waiter.Wait();

  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_EQ(browser_1, ui_test_utils::FindAnyBrowser(browser()->profile()));

  ash::ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_EQ(browser_1, ui_test_utils::FindAnyBrowser(browser()->profile()));
}

class BrowserFinderChromeOSBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  static constexpr inline auto kPrimaryAccountId =
      AccountId::Literal::FromUserEmailGaiaId("primary@test",
                                              GaiaId::Literal("12345"));

  static constexpr inline auto kSecondaryAccountId =
      AccountId::Literal::FromUserEmailGaiaId("secondary@test",
                                              GaiaId::Literal("67890"));

  BrowserFinderChromeOSBrowserTest() {
    set_exit_when_last_browser_closes(false);
  }

  ~BrowserFinderChromeOSBrowserTest() override = default;

  void SetUp() override {
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        "test");
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    MixinBasedInProcessBrowserTest::TearDown();
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        nullptr);
  }

  void LogIn(const AccountId& account_id) {
    if (auto* primary_session =
            session_manager::SessionManager::Get()->GetPrimarySession()) {
      user_manager::User* primary_user =
          user_manager::UserManager::Get()->FindUserAndModify(
              primary_session->account_id());
      primary_user->GetProfilePrefs()->SetBoolean(
          user_manager::prefs::kMultiProfileNeverShowIntro, true);
      SessionControllerClientImpl::Get()->ShowMultiProfileLogin();
    }
    login_manager_mixin_.LoginWithDefaultContext(
        ash::LoginManagerMixin::TestUserInfo(account_id));
  }

 protected:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};

  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_,
      {ash::LoginManagerMixin::TestUserInfo(kPrimaryAccountId),
       ash::LoginManagerMixin::TestUserInfo(kSecondaryAccountId)}};
};

IN_PROC_BROWSER_TEST_F(BrowserFinderChromeOSBrowserTest,
                       IncognitoBrowserMatchTest) {
  LogIn(kPrimaryAccountId);
  Profile* primary_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          kPrimaryAccountId));

  Browser* primary_browser = CreateBrowser(primary_profile);
  EXPECT_EQ(primary_browser, ui_test_utils::FindAnyBrowser(primary_profile));
  EXPECT_EQ(primary_browser,
            ui_test_utils::FindAnyBrowser(primary_profile,
                                          /*match_original_profiles=*/false));

  // Close the primary browser.
  CloseBrowserSynchronously(primary_browser);
  EXPECT_FALSE(ui_test_utils::FindAnyBrowser(primary_profile));

  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(primary_profile);

  // Exact profile match returns nothing (only the incognito browser exists,
  // and it belongs to the OTR profile).
  EXPECT_FALSE(ui_test_utils::FindAnyBrowser(
      primary_profile, /*match_original_profiles=*/false));

  // But searching by original profile finds the incognito browser.
  EXPECT_EQ(incognito_browser, ui_test_utils::FindAnyBrowser(primary_profile));

  CloseBrowserSynchronously(incognito_browser);
}

IN_PROC_BROWSER_TEST_F(BrowserFinderChromeOSBrowserTest,
                       FindBrowserOwnedByAnotherProfile) {
  LogIn(kPrimaryAccountId);
  Profile* primary_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          kPrimaryAccountId));

  LogIn(kSecondaryAccountId);

  SessionControllerClientImpl::Get()->SwitchActiveUser(kPrimaryAccountId);
  ASSERT_EQ(
      session_manager::SessionManager::Get()->GetActiveSession()->account_id(),
      kPrimaryAccountId);

  Browser* primary_browser = CreateBrowser(primary_profile);
  auto* window_manager = ash::Shell::Get()->multi_user_window_manager();

  // The browser is shown for the owning user, so FindAnyBrowser finds it
  // regardless of match_original_profiles setting.
  EXPECT_EQ(primary_browser,
            ui_test_utils::FindAnyBrowser(primary_profile,
                                          /*match_original_profiles=*/false));
  EXPECT_EQ(primary_browser, ui_test_utils::FindAnyBrowser(primary_profile));

  // Move the browser window to another user's desktop.
  window_manager->ShowWindowForUser(
      primary_browser->GetWindow()->GetNativeWindow()->GetToplevelWindow(),
      kSecondaryAccountId);

  // ShowWindowForUser() notifies chrome async.
  // FindAnyBrowser filters by multi-user window visibility on ChromeOS.
  // A window shown on another user's desktop is excluded regardless of
  // match_original_profiles setting.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !ui_test_utils::FindAnyBrowser(primary_profile,
                                          /*match_original_profiles=*/false);
  }));
  EXPECT_FALSE(ui_test_utils::FindAnyBrowser(primary_profile));

  CloseBrowserSynchronously(primary_browser);
}
