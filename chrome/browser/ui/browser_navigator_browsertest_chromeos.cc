// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include "ash/constants/ash_switches.h"
#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_pin_util.h"
#include "base/command_line.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"

namespace {

using BrowserNavigatorTestChromeOS = BrowserNavigatorTest;

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

// Verifies that new browser is not opened for Signin profile.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTestChromeOS, RestrictSigninProfile) {
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);

  EXPECT_EQ(Browser::CreationStatus::kErrorProfileUnsuitable,
            Browser::GetCreationStatusForProfile(
                ash::ProfileHelper::GetSigninProfile()));
}

// Verify that page navigation is blocked in locked fullscreen mode.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTestChromeOS,
                       NavigationBlockedInLockedFullscreen) {
  // Set locked fullscreen state.
  aura::Window* window = browser()->window()->GetNativeWindow();
  ash::PinWindow(window, /*trusted=*/true);

  // Navigate to a page.
  auto url = GURL(chrome::kChromeUIVersionURL);
  NavigateParams params(MakeNavigateParams(browser()));
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.url = url;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&params);

  // The page should not be opened, and the browser should still sit at the
  // default about:blank page.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // As a sanity check unset the locked fullscreen state and make sure that the
  // navigation happens (the following EXPECTs fail if the next line isn't
  // executed).
  ash::UnpinWindow(window);

  Navigate(&params);

  // The original browser should still be at the same page, but the newly
  // opened browser should sit on the chrome:version page.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(
      1,
      params.browser->GetBrowserForMigrationOnly()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUIVersionURL),
            params.browser->GetBrowserForMigrationOnly()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetURL());
}

// Verify that page navigation is allowed in locked fullscreen mode when locked
// for OnTask. Only applicable for non-web browser scenarios.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTestChromeOS,
                       NavigationAllowedInLockedFullscreenWhenLockedForOnTask) {
  // Set locked fullscreen state.
  aura::Window* const window = browser()->window()->GetNativeWindow();
  ash::PinWindow(window, /*trusted=*/true);
  browser()->SetLockedForOnTask(true);

  // Navigate to a page.
  const GURL kUrl(chrome::kChromeUIVersionURL);
  NavigateParams params(MakeNavigateParams(browser()));
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.url = kUrl;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&params);

  // The original browser should still be at the same page, but the newly
  // opened browser should sit on the chrome:version page.
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(
      1,
      params.browser->GetBrowserForMigrationOnly()->tab_strip_model()->count());
  EXPECT_EQ(kUrl, params.browser->GetBrowserForMigrationOnly()
                      ->tab_strip_model()
                      ->GetActiveWebContents()
                      ->GetURL());
}

// Subclass that tests navigation while in the Guest session.
class BrowserGuestSessionNavigatorTest : public BrowserNavigatorTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine command_line_copy = *command_line;
    command_line_copy.AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line_copy.AppendSwitch(ash::switches::kGuestSession);
    ash::GetOffTheRecordCommandLine(GetGoogleURL(), command_line_copy,
                                    command_line);
  }
};

// This test verifies that the settings page is opened in the incognito window
// in Guest Session (as well as all other windows in Guest session).
IN_PROC_BROWSER_TEST_F(BrowserGuestSessionNavigatorTest,
                       Disposition_Settings_UseIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());

  // Navigate to the settings page.
  NavigateParams params(MakeNavigateParams(incognito_browser));
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GURL("chrome://settings");
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // Settings page should be opened in incognito window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(incognito_browser, params.browser);
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(
      GURL("chrome://settings"),
      incognito_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

class BrowserNavigatorMultiUserTestChromeOS
    : public InProcessBrowserTestMixinHostSupport<
          BrowserNavigatorTestChromeOS> {
 public:
  static constexpr inline auto kPrimaryAccountId =
      AccountId::Literal::FromUserEmailGaiaId("primary@test",
                                              GaiaId::Literal("12345"));
  static constexpr inline auto kSecondaryAccountId =
      AccountId::Literal::FromUserEmailGaiaId("secondary@test",
                                              GaiaId::Literal("67890"));

  BrowserNavigatorMultiUserTestChromeOS() = default;
  ~BrowserNavigatorMultiUserTestChromeOS() override = default;

  void SetUp() override {
    set_exit_when_last_browser_closes(false);
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        "test");
    InProcessBrowserTestMixinHostSupport<BrowserNavigatorTestChromeOS>::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTestMixinHostSupport<
        BrowserNavigatorTestChromeOS>::TearDown();
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        nullptr);
  }

  void LogIn(const AccountId& account_id) {
    // If there's already a session, i.e. if this is multi user sign in,
    // first, launch user selection flow.
    if (auto* primary_session =
            session_manager::SessionManager::Get()->GetPrimarySession()) {
      // Mark the acknowledge for the multi-user sign-in.
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

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};
  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_,
      {ash::LoginManagerMixin::TestUserInfo(kPrimaryAccountId),
       ash::LoginManagerMixin::TestUserInfo(kSecondaryAccountId)}};
};

// Test that in multi user environments a newly created browser gets created
// on the same desktop as the browser is shown on.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorMultiUserTestChromeOS,
                       Browser_Gets_Created_On_Visiting_Desktop) {
  // Set up primary user.
  LogIn(kPrimaryAccountId);
  Profile* primary_user_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          kPrimaryAccountId));
  ASSERT_TRUE(primary_user_profile);

  // Create a browser window with the primary user.
  ash::NewWindowDelegate::GetInstance()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false);

  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());
  Browser* browser = chrome::FindBrowserWithProfile(primary_user_profile);
  ASSERT_TRUE(browser);

  // Start multi-user sign-in.
  LogIn(kSecondaryAccountId);

  auto* window_manager = ash::Shell::Get()->multi_user_window_manager();

  // Test 1: Test that a browser created from a visiting browser will be on the
  // same visiting desktop.
  {
    // Make sure the current active user is the secondary.
    ASSERT_EQ(session_manager::SessionManager::Get()
                  ->GetActiveSession()
                  ->account_id(),
              kSecondaryAccountId);

    // Teleport the primary user's browser window to the secondary user's
    // desktop.
    window_manager->ShowWindowForUser(
        browser->window()->GetNativeWindow()->GetToplevelWindow(),
        kSecondaryAccountId);
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

    // Navigate to the settings page from the primary user's browser.
    NavigateParams params(MakeNavigateParams(browser));
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.url = GURL("chrome://settings");
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
    params.browser = browser;
    auto navigated = Navigate(&params);
    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

    // Verify the created window is shown on the secondary user's desktop.
    aura::Window* created_window =
        navigated->GetWebContents()->GetTopLevelNativeWindow();
    ASSERT_TRUE(created_window);
    EXPECT_TRUE(window_manager->IsWindowOnDesktopOfUser(created_window,
                                                        kSecondaryAccountId));
  }

  // Test 2: Test that a window which is not visiting does not cause an owner
  // assignment of a newly created browser.
  {
    // Move the browser window back to the primary user's desktop.
    window_manager->ShowWindowForUser(
        browser->window()->GetNativeWindow()->GetToplevelWindow(),
        kPrimaryAccountId);
    // Teleporting the window also triggers to switch the active session.
    ASSERT_EQ(session_manager::SessionManager::Get()
                  ->GetActiveSession()
                  ->account_id(),
              kPrimaryAccountId);

    // Navigate to the settings page.
    NavigateParams params(MakeNavigateParams(browser));
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.url = GURL("chrome://settings");
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
    params.browser = browser;
    auto navigated = Navigate(&params);
    EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

    // The created window should be at the primary user's desktop now.
    aura::Window* created_window =
        navigated->GetWebContents()->GetTopLevelNativeWindow();
    ASSERT_TRUE(created_window);
    EXPECT_TRUE(window_manager->IsWindowOnDesktopOfUser(created_window,
                                                        kPrimaryAccountId));
  }
}

}  // namespace
