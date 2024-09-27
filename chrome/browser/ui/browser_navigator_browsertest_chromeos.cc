// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_browsertest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/multi_user/test_multi_user_window_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using BrowserNavigatorTestChromeOS = BrowserNavigatorTest;

#if BUILDFLAG(IS_CHROMEOS_ASH)

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
  PinWindow(window, /*trusted=*/true);

  // Navigate to a page.
  auto url = GURL(chrome::kChromeUIVersionURL);
  NavigateParams params(MakeNavigateParams(browser()));
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.url = url;
  params.window_action = NavigateParams::SHOW_WINDOW;
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
  UnpinWindow(window);

  Navigate(&params);

  // The original browser should still be at the same page, but the newly
  // opened browser should sit on the chrome:version page.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
  EXPECT_EQ(
      GURL(chrome::kChromeUIVersionURL),
      params.browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verify that page navigation is allowed in locked fullscreen mode when locked
// for OnTask. Only applicable for non-web browser scenarios.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTestChromeOS,
                       NavigationAllowedInLockedFullscreenWhenLockedForOnTask) {
  // Set locked fullscreen state.
  aura::Window* const window = browser()->window()->GetNativeWindow();
  PinWindow(window, /*trusted=*/true);
  browser()->SetLockedForOnTask(true);

  // Navigate to a page.
  const GURL kUrl(chrome::kChromeUIVersionURL);
  NavigateParams params(MakeNavigateParams(browser()));
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.url = kUrl;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);

  // The original browser should still be at the same page, but the newly
  // opened browser should sit on the chrome:version page.
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(1, params.browser->tab_strip_model()->count());
  EXPECT_EQ(
      kUrl,
      params.browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  params.window_action = NavigateParams::SHOW_WINDOW;
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

// Test that in multi user environments a newly created browser gets created
// on the same desktop as the browser is shown on.
IN_PROC_BROWSER_TEST_F(BrowserGuestSessionNavigatorTest,
                       Browser_Gets_Created_On_Visiting_Desktop) {
  // Test 1: Test that a browser created from a visiting browser will be on the
  // same visiting desktop.
  {
    const AccountId desktop_account_id(
        AccountId::FromUserEmail("desktop_user_id@fake.com"));
    TestMultiUserWindowManager* window_manager =
        TestMultiUserWindowManager::Create(browser(), desktop_account_id);

    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

    // Navigate to the settings page.
    NavigateParams params(MakeNavigateParams(browser()));
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.url = GURL("chrome://settings");
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
    params.browser = browser();
    Navigate(&params);

    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

    aura::Window* created_window = window_manager->created_window();
    ASSERT_TRUE(created_window);
    EXPECT_TRUE(
        MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
            created_window, desktop_account_id));
  }
  // Test 2: Test that a window which is not visiting does not cause an owner
  // assignment of a newly created browser.
  {
    const AccountId browser_owner =
        multi_user_util::GetAccountIdFromProfile(browser()->profile());
    TestMultiUserWindowManager* window_manager =
        TestMultiUserWindowManager::Create(browser(), browser_owner);

    // Navigate to the settings page.
    NavigateParams params(MakeNavigateParams(browser()));
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.url = GURL("chrome://settings");
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
    params.browser = browser();
    Navigate(&params);

    EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

    // The ShowWindowForUser should not have been called since the window is
    // already on the correct desktop.
    ASSERT_FALSE(window_manager->created_window());
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Verifies that the navigation is trying to open the os:// scheme page in
// Ash, will fail and then open it as chrome:// in Lacros to show a 404 error.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTestChromeOS, OsSchemeRedirectFail) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Navigate to an unknown page with an os:// scheme.
  NavigateParams params(MakeNavigateParams(browser()));
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GURL("os://foobar");
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // A new blocked page should be shown in the browser.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(content::kBlockedURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Verifies that the navigation of an os:// scheme page is opening an app on
// the ash side and does not produce a navigation on the Lacros side.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTestChromeOS, OsSchemeRedirectSucceed) {
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::TestController>() <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kGetOpenAshBrowserWindowsMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();

  // Ash shouldn't have a browser window open by now.
  base::test::TestFuture<uint32_t> window_count_future;
  test_controller->GetOpenAshBrowserWindows(window_count_future.GetCallback());
  EXPECT_EQ(0u, window_count_future.Take());

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  GURL url_before_navigation =
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL();

  // Navigate to a known Ash page.
  NavigateParams params(MakeNavigateParams(browser()));
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GURL(chrome::kOsUIFlagsURL);
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  params.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  Navigate(&params);

  // No change should have happened on the Lacros side.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(url_before_navigation,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Clean up the window we have created.

  // Wait until we have the app running.
  ASSERT_TRUE(base::test::RunUntil([&] {
    test_controller->GetOpenAshBrowserWindows(
        window_count_future.GetCallback());
    return window_count_future.Take() > 0;
  }));

  // Close it.
  base::test::TestFuture<bool> success_future;
  test_controller->CloseAllBrowserWindows(success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  // Wait until all are gone.
  ASSERT_TRUE(base::test::RunUntil([&] {
    test_controller->GetOpenAshBrowserWindows(
        window_count_future.GetCallback());
    return window_count_future.Take() == 0;
  }));
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
