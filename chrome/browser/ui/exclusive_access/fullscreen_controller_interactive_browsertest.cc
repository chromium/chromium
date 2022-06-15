// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) && defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // USE_AURA

using url::kAboutBlankURL;
using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;

namespace {

const base::FilePath::CharType* kSimpleFile = FILE_PATH_LITERAL("simple.html");

}  // namespace

class FullscreenControllerInteractiveTest : public ExclusiveAccessTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExclusiveAccessTest::SetUpCommandLine(command_line);
    // Slow bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  // Tests that actually make the browser fullscreen have been flaky when
  // run sharded, and so are restricted here to interactive ui tests.
  void ToggleTabFullscreen(bool enter_fullscreen);
  void ToggleTabFullscreenNoRetries(bool enter_fullscreen);
  void ToggleBrowserFullscreen(bool enter_fullscreen);

  // IsMouseLocked verifies that the FullscreenController state believes
  // the mouse is locked. This is possible only for tests that initiate
  // mouse lock from a renderer process, and uses logic that tests that the
  // browser has focus. Thus, this can only be used in interactive ui tests
  // and not on sharded tests.
  bool IsMouseLocked() {
    // Verify that IsMouseLocked is consistent between the
    // Fullscreen Controller and the Render View Host View.
    EXPECT_TRUE(browser()->IsMouseLocked() == browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveWebContents()
                                                  ->GetPrimaryMainFrame()
                                                  ->GetRenderViewHost()
                                                  ->GetWidget()
                                                  ->GetView()
                                                  ->IsMouseLocked());
    return browser()->IsMouseLocked();
  }

  void PressKeyAndWaitForMouseLockRequest(ui::KeyboardCode key_code) {
    base::RunLoop run_loop;
    browser()
        ->exclusive_access_manager()
        ->mouse_lock_controller()
        ->set_lock_state_callback_for_test(run_loop.QuitClosure());
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key_code, false,
                                                false, false, false));
    run_loop.Run();
  }

 private:
  void ToggleTabFullscreen_Internal(bool enter_fullscreen,
                                    bool retry_until_success);
};

void FullscreenControllerInteractiveTest::ToggleTabFullscreen(
    bool enter_fullscreen) {
  ToggleTabFullscreen_Internal(enter_fullscreen, true);
}

// |ToggleTabFullscreen| should not need to tolerate the transition failing.
// Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
// and some flakiness has occurred when calling |ToggleTabFullscreen|, so that
// method has been made robust by retrying if the transition fails.
// The root cause of that flakiness should still be tracked down, see
// http://crbug.com/133831. In the mean time, this method
// allows a fullscreen_controller_interactive_browsertest.cc test to verify
// that when running serially there is no flakiness in the transition.
void FullscreenControllerInteractiveTest::ToggleTabFullscreenNoRetries(
    bool enter_fullscreen) {
  ToggleTabFullscreen_Internal(enter_fullscreen, false);
}

void FullscreenControllerInteractiveTest::ToggleBrowserFullscreen(
    bool enter_fullscreen) {
  ASSERT_EQ(browser()->window()->IsFullscreen(), !enter_fullscreen);
  FullscreenNotificationObserver fullscreen_observer(browser());

  chrome::ToggleFullscreenMode(browser());

  fullscreen_observer.Wait();
  ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
  ASSERT_EQ(IsFullscreenForBrowser(), enter_fullscreen);
}

void FullscreenControllerInteractiveTest::ToggleTabFullscreen_Internal(
    bool enter_fullscreen, bool retry_until_success) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  do {
    FullscreenNotificationObserver fullscreen_observer(browser());
    if (enter_fullscreen)
      browser()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
    else
      browser()->ExitFullscreenModeForTab(tab);
    fullscreen_observer.Wait();
    // Repeat ToggleFullscreenModeForTab until the correct state is entered.
    // This addresses flakiness on test bots running many fullscreen
    // tests in parallel.
  } while (retry_until_success &&
           !IsFullscreenForBrowser() &&
           browser()->window()->IsFullscreen() != enter_fullscreen);
  ASSERT_EQ(IsWindowFullscreenForTabOrPending(), enter_fullscreen);
  if (!IsFullscreenForBrowser())
    ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
}

// Tests ///////////////////////////////////////////////////////////////////////

// Tests that while in fullscreen creating a new tab will exit fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestNewTabExitsFullscreen) {
#if BUILDFLAG(IS_LINUX) && defined(USE_OZONE)
  // Flaky in Linux interactive_ui_tests_wayland: crbug.com/1200036
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland")
    GTEST_SKIP();
#endif

  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    ASSERT_TRUE(
        AddTabAtIndex(1, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED));
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

// Tests a tab exiting fullscreen will bring the browser out of fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests that the closure provided to RunOrDeferUntilTransitionIsComplete is
// run. Some platforms may be synchronous (lambda is executed immediately) and
// others (e.g. Mac) will run it asynchronously (after the transition).
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       RunOrDeferClosureDuringTransition) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  GetFullscreenController()->EnterFullscreenModeForTab(
      tab->GetPrimaryMainFrame(), {});
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  base::RunLoop run_loop;
  bool lambda_called = false;
  ASSERT_NO_FATAL_FAILURE(
      GetFullscreenController()->RunOrDeferUntilTransitionIsComplete(
          base::BindLambdaForTesting([&lambda_called, &run_loop]() {
            lambda_called = true;
            run_loop.Quit();
          })));
  // Lambda may run synchronously on some platforms. If it did not already run,
  // block until it has.
  if (!lambda_called)
    run_loop.Run();
  EXPECT_TRUE(lambda_called);
}

// Test is flaky on Lacros: https://crbug.com/1250091
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_BrowserFullscreenExit DISABLED_BrowserFullscreenExit
#else
#define MAYBE_BrowserFullscreenExit BrowserFullscreenExit
#endif
// Tests Fullscreen entered in Browser, then Tab mode, then exited via Browser.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_BrowserFullscreenExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter tab fullscreen.
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  // Exit browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Test is flaky on Lacros: https://crbug.com/1250092
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_BrowserFullscreenAfterTabFSExit \
  DISABLED_BrowserFullscreenAfterTabFSExit
#else
#define MAYBE_BrowserFullscreenAfterTabFSExit BrowserFullscreenAfterTabFSExit
#endif
// Tests Browser Fullscreen remains active after Tab mode entered and exited.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_BrowserFullscreenAfterTabFSExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter and then exit tab fullscreen.
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));

  // Verify browser fullscreen still active.
  ASSERT_TRUE(IsFullscreenForBrowser());
}

// Tests fullscreen entered without permision prompt for file:// urls.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest, FullscreenFileURL) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kSimpleFile))));

  // Validate that going fullscreen for a file does not ask permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests fullscreen is exited on page navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsFullscreenOnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Test is flaky on all platforms: https://crbug.com/1234337
// Tests fullscreen is exited when navigating back.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_TestTabExitsFullscreenOnGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests fullscreen is not exited on sub frame navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabDoesntExitFullscreenOnSubFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kSimpleFile)));
  GURL url_with_fragment(url.spec() + "#fragment");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_fragment));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Test is flaky on all platforms: https://crbug.com/1234337
// Tests tab fullscreen exits, but browser fullscreen remains, on navigation.
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerInteractiveTest,
    DISABLED_TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_TRUE(IsFullscreenForBrowser());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// TODO(crbug.com/1230771) Flaky on Linux-ozone and Lacros
#if (BUILDFLAG(IS_LINUX) && defined(USE_OZONE)) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TabEntersPresentationModeFromWindowed \
  DISABLED_TabEntersPresentationModeFromWindowed
#else
#define MAYBE_TabEntersPresentationModeFromWindowed \
  TabEntersPresentationModeFromWindowed
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TabEntersPresentationModeFromWindowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED));

  {
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
    EXPECT_TRUE(browser()->window()->IsFullscreen());
  }

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
    EXPECT_FALSE(browser()->window()->IsFullscreen());
  }

  {
    // Test that tab fullscreen mode doesn't make presentation mode the default
    // on Lion.
    FullscreenNotificationObserver fullscreen_observer(browser());
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
    EXPECT_TRUE(browser()->window()->IsFullscreen());
  }
}

// Tests mouse lock can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest, EscapingMouseLock) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Request to lock the mouse.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);

  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());

  // Escape, confirm we are out of mouse lock with no prompts.
  SendEscapeToExclusiveAccessManager();
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests mouse lock and fullscreen modes can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       EscapingMouseLockAndFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    PressKeyAndWaitForMouseLockRequest(ui::VKEY_B);
    fullscreen_observer.Wait();
  }

  // Escape, no prompts should remain.
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    SendEscapeToExclusiveAccessManager();
    fullscreen_observer.Wait();
  }
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests mouse lock then fullscreen.
// TODO(crbug.com/1318638): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_MouseLockThenFullscreen DISABLED_MouseLockThenFullscreen
#else
#define MAYBE_MouseLockThenFullscreen MouseLockThenFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_MouseLockThenFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock the mouse without a user gesture, expect no response.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_D);
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_FALSE(IsMouseLocked());

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());

  // Enter fullscreen mode, mouse should remain locked.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Disabled on all due to issue with code under test: http://crbug.com/1255610.
//
// Was also disabled on platforms before:
// Times out sometimes on Linux. http://crbug.com/135115
// Mac: http://crbug.com/103912
// Tests mouse lock then fullscreen in same request.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_MouseLockAndFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    PressKeyAndWaitForMouseLockRequest(ui::VKEY_B);
    fullscreen_observer.Wait();
  }
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Tests mouse lock can be exited and re-entered by an application silently
// with no UI distraction for users.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MouseLockSilentAfterTargetUnlock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  // Unlock the mouse from target, make sure it's unlocked.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_U);
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock mouse again, make sure it works with no bubble.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Unlock the mouse again by target.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_U);
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());
}

// Tests mouse lock is exited on page navigation.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && defined(USE_AURA)
// https://crbug.com/1191964
#define MAYBE_TestTabExitsMouseLockOnNavigation \
    DISABLED_TestTabExitsMouseLockOnNavigation
#else
#define MAYBE_TestTabExitsMouseLockOnNavigation \
    TestTabExitsMouseLockOnNavigation
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnNavigation) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  ASSERT_FALSE(IsMouseLocked());
}

// Tests mouse lock is exited when navigating back.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && defined(USE_AURA)
// https://crbug.com/1192097
#define MAYBE_TestTabExitsMouseLockOnGoBack \
  DISABLED_TestTabExitsMouseLockOnGoBack
#else
#define MAYBE_TestTabExitsMouseLockOnGoBack TestTabExitsMouseLockOnGoBack
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnGoBack) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // Navigate twice to provide a place to go back to.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  GoBack();

  ASSERT_FALSE(IsMouseLocked());
}

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
        defined(USE_AURA) ||                                  \
    BUILDFLAG(IS_WIN) && defined(NDEBUG)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
// Test is flaky on Windows: https://crbug.com/1124492
#define MAYBE_TestTabDoesntExitMouseLockOnSubFrameNavigation \
  DISABLED_TestTabDoesntExitMouseLockOnSubFrameNavigation
#else
#define MAYBE_TestTabDoesntExitMouseLockOnSubFrameNavigation \
  TestTabDoesntExitMouseLockOnSubFrameNavigation
#endif

// Tests mouse lock is not exited on sub frame navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabDoesntExitMouseLockOnSubFrameNavigation) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // Create URLs for test page and test page with #fragment.
  GURL url(embedded_test_server()->GetURL(kFullscreenMouseLockHTML));
  GURL url_with_fragment(url.spec() + "#fragment");

  // Navigate to test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  // Navigate to url with fragment. Mouse lock should persist.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_fragment));
  ASSERT_TRUE(IsMouseLocked());
}

// Tests Mouse Lock and Fullscreen are exited upon reload.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       ReloadExitsMouseLockAndFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML)));

  // Request mouse lock.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);

  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  // Reload. Mouse lock request should be cleared.
  {
    base::RunLoop run_loop;
    browser()
        ->exclusive_access_manager()
        ->mouse_lock_controller()
        ->set_lock_state_callback_for_test(run_loop.QuitClosure());
    Reload();
    run_loop.Run();
  }

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    PressKeyAndWaitForMouseLockRequest(ui::VKEY_B);
    fullscreen_observer.Wait();
  }

  // We are fullscreen.
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  // Reload. Mouse should be unlocked and fullscreen exited.
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    Reload();
    fullscreen_observer.Wait();
    ASSERT_FALSE(IsMouseLocked());
    ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
  }
}

// Tests ToggleFullscreenModeForTab always causes window to change.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       ToggleFullscreenModeForTab) {
  // Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
  // but flakiness required a while loop in
  // ExclusiveAccessTest::ToggleTabFullscreen. This test verifies that
  // when running serially there is no flakiness.

  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(AddTabAtIndex(0, url, PAGE_TRANSITION_TYPED));

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(false));
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests FullscreenController support of Multi-Screen Window Placement features.
// Sites with the Window Placement permission can request fullscreen on a
// specific screen, move fullscreen windows to different displays, and more.
class MultiScreenFullscreenControllerInteractiveTest
    : public FullscreenControllerInteractiveTest {
 public:
  void SetUp() override {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    screen_.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::PRIMARY);
    display::Screen::SetScreenInstance(&screen_);
#endif
    FullscreenControllerInteractiveTest::SetUp();
  }

  void TearDown() override {
    FullscreenControllerInteractiveTest::TearDown();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    display::Screen::SetScreenInstance(nullptr);
#endif
  }

  // Perform common setup operations for multi-screen fullscreen testing:
  // Mock a screen with two displays, move the browser onto the first display,
  // and auto-grant the Window Placement permission on its active tab.
  content::WebContents* SetUpTestScreenAndWindowPlacementTab() {
    // Set a test Screen environment with two displays.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay("0+0-800x800,800+0-800x800");
#else
    screen_.display_list().AddDisplay({2, gfx::Rect(800, 0, 800, 800)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

    // Move the window to the first display (on the left).
    browser()->window()->SetBounds({150, 150, 600, 500});

    // Open a new tab that observes the test screen environment.
    EXPECT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL("/simple.html"));
    EXPECT_TRUE(AddTabAtIndex(1, url, PAGE_TRANSITION_TYPED));

    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

    // Auto-accept Window Placement permission prompts.
    permissions::PermissionRequestManager* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(tab);
    permission_request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);

    return tab;
  }

  // Wait for a JS content fullscreen change with the given script and options.
  void RequestContentFullscreenFromScript(
      const std::string& eval_js_script,
      int eval_js_options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
      bool expect_content_fullscreen = true,
      bool expect_window_fullscreen = true) {
    FullscreenNotificationObserver fullscreen_observer(browser());
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(expect_content_fullscreen,
              EvalJs(tab, eval_js_script, eval_js_options));
    fullscreen_observer.Wait();
    EXPECT_EQ(expect_window_fullscreen, browser()->window()->IsFullscreen());
  }

  // Execute JS to request content fullscreen on the current screen.
  void RequestContentFullscreen() {
    const std::string request_fullscreen_script = R"(
      (async () => {
        await document.body.requestFullscreen();
        return !!document.fullscreenElement;
      })();
    )";
    RequestContentFullscreenFromScript(request_fullscreen_script);
  }

  // Execute JS to request content fullscreen on a screen with the given index.
  void RequestContentFullscreenOnScreen(int screen_index) {
    const std::string request_fullscreen_script =
        base::StringPrintf(R"(
      (async () => {
        if (!window.screenDetails)
          window.screenDetails = await window.getScreenDetails();
        const options = { screen: window.screenDetails.screens[%d] };
        await document.body.requestFullscreen(options);
        return !!document.fullscreenElement;
      })();
    )",
                           screen_index);
    RequestContentFullscreenFromScript(request_fullscreen_script);
  }

  // Execute JS to exit content fullscreen.
  void ExitContentFullscreen(bool expect_window_fullscreen = false) {
    const std::string exit_fullscreen_script = R"(
      (async () => {
        await document.exitFullscreen();
        return !!document.fullscreenElement;
      })();
    )";
    // Exiting fullscreen does not require a user gesture; do not supply one.
    RequestContentFullscreenFromScript(
        exit_fullscreen_script, content::EXECUTE_SCRIPT_NO_USER_GESTURE,
        /*expect_content_fullscreen=*/false, expect_window_fullscreen);
  }

  // Awaits expiry of the navigator.userActivation signal on the active tab.
  void WaitForUserActivationExpiry() {
    const std::string await_activation_expiry_script = R"(
      (async () => {
        while (navigator.userActivation.isActive)
          await new Promise(resolve => setTimeout(resolve, 1000));
        return navigator.userActivation.isActive;
      })();
    )";
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(false, EvalJs(tab, await_activation_expiry_script,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    EXPECT_FALSE(tab->HasRecentInteractiveInputEvent());
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  display::DisplayList& display_list() { return screen_.display_list(); }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kWindowPlacement};
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  display::ScreenBase screen_;
#endif
};

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SeparateDisplay DISABLED_SeparateDisplay
#else
#define MAYBE_SeparateDisplay SeparateDisplay
#endif
// Test requesting fullscreen on a separate display.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SeparateDisplay) {
  SetUpTestScreenAndWindowPlacementTab();
  const gfx::Rect original_bounds = browser()->window()->GetBounds();

  // Execute JS to request fullscreen on the second display (on the right).
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());
  const display::Screen* screen = display::Screen::GetScreen();
  EXPECT_NE(screen->GetDisplayMatching(original_bounds),
            screen->GetDisplayMatching(browser()->window()->GetBounds()));

  ExitContentFullscreen();
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
}

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SeparateDisplayMaximized DISABLED_SeparateDisplayMaximized
#else
#define MAYBE_SeparateDisplayMaximized SeparateDisplayMaximized
#endif
// Test requesting fullscreen on a separate display from a maximized window.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SeparateDisplayMaximized) {
  SetUpTestScreenAndWindowPlacementTab();
  const gfx::Rect original_bounds = browser()->window()->GetBounds();

  browser()->window()->Maximize();
  EXPECT_TRUE(browser()->window()->IsMaximized());
  const gfx::Rect maximized_bounds = browser()->window()->GetBounds();

  // Execute JS to request fullscreen on the second display (on the right).
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());

  ExitContentFullscreen();
  EXPECT_EQ(maximized_bounds, browser()->window()->GetBounds());
  EXPECT_TRUE(browser()->window()->IsMaximized());

  // Unmaximize the window and check that the original bounds are restored.
  browser()->window()->Restore();
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
}

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SameDisplayAndSwap DISABLED_SameDisplayAndSwap
#else
#define MAYBE_SameDisplayAndSwap SameDisplayAndSwap
#endif
// Test requesting fullscreen on the current display and then swapping displays.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SameDisplayAndSwap) {
  SetUpTestScreenAndWindowPlacementTab();
  const gfx::Rect original_bounds = browser()->window()->GetBounds();

  // Execute JS to request fullscreen on the current display (on the left).
  RequestContentFullscreen();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 800), browser()->window()->GetBounds());

  // Execute JS to request fullscreen on the other display (on the right).
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());

  ExitContentFullscreen();
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
}

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SameDisplayAndSwapMaximized DISABLED_SameDisplayAndSwapMaximized
#else
#define MAYBE_SameDisplayAndSwapMaximized SameDisplayAndSwapMaximized
#endif
// Test requesting fullscreen on the current display and then swapping displays
// from a maximized window.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SameDisplayAndSwapMaximized) {
  SetUpTestScreenAndWindowPlacementTab();
  const gfx::Rect original_bounds = browser()->window()->GetBounds();

  browser()->window()->Maximize();
  EXPECT_TRUE(browser()->window()->IsMaximized());
  const gfx::Rect maximized_bounds = browser()->window()->GetBounds();

  // Execute JS to request fullscreen on the current display (on the left).
  RequestContentFullscreen();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 800), browser()->window()->GetBounds());

  // Execute JS to request fullscreen on the other display (on the right).
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());

  ExitContentFullscreen();
  EXPECT_EQ(maximized_bounds, browser()->window()->GetBounds());
  EXPECT_TRUE(browser()->window()->IsMaximized());

  // Unmaximize the window and check that the original bounds are restored.
  browser()->window()->Restore();
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
}

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_BrowserFullscreenContentFullscreenSwapDisplay \
  DISABLED_BrowserFullscreenContentFullscreenSwapDisplay
#else
#define MAYBE_BrowserFullscreenContentFullscreenSwapDisplay \
  BrowserFullscreenContentFullscreenSwapDisplay
#endif
// Test requesting browser fullscreen on current display, launching
// tab-fullscreen on a different display, and then closing tab-fullscreen to
// restore browser-fullscreen on the original display.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_BrowserFullscreenContentFullscreenSwapDisplay) {
  SetUpTestScreenAndWindowPlacementTab();

  ToggleBrowserFullscreen(true);
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());

  const gfx::Rect fullscreen_bounds = browser()->window()->GetBounds();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 800), fullscreen_bounds);

  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());
  // Fullscreen was originally initiated by browser, this should still be true.
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_TRUE(IsWindowFullscreenForTabOrPending());

  ExitContentFullscreen(/*expect_window_fullscreen=*/true);
  EXPECT_EQ(fullscreen_bounds, browser()->window()->GetBounds());
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());
}

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SeparateDisplayAndSwap DISABLED_SeparateDisplayAndSwap
#else
#define MAYBE_SeparateDisplayAndSwap SeparateDisplayAndSwap
#endif
// Test requesting fullscreen on a separate display and then swapping displays.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SeparateDisplayAndSwap) {
  SetUpTestScreenAndWindowPlacementTab();
  const gfx::Rect original_bounds = browser()->window()->GetBounds();

  // Execute JS to request fullscreen on the second display (on the right).
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());

  // Execute JS to change fullscreen screens back to the original.
  RequestContentFullscreenOnScreen(0);
  EXPECT_EQ(gfx::Rect(0, 0, 800, 800), browser()->window()->GetBounds());

  // Go back to the second display, just for good measure.
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());

  ExitContentFullscreen();
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
}

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SwapShowsBubble DISABLED_SwapShowsBubble
#else
#define MAYBE_SwapShowsBubble SwapShowsBubble
#endif
// Test requesting fullscreen on the current display and then swapping displays.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SwapShowsBubble) {
  SetUpTestScreenAndWindowPlacementTab();

  // Execute JS to request fullscreen on the current display (on the left).
  RequestContentFullscreen();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 800), browser()->window()->GetBounds());

  // Explicitly check for, and destroy, the exclusive access bubble.
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
  base::RunLoop run_loop;
  ExclusiveAccessBubbleHideCallback callback = base::BindLambdaForTesting(
      [&run_loop](ExclusiveAccessBubbleHideReason) { run_loop.Quit(); });
  browser()
      ->exclusive_access_manager()
      ->context()
      ->UpdateExclusiveAccessExitBubbleContent(
          browser()->exclusive_access_manager()->GetExclusiveAccessBubbleURL(),
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, std::move(callback),
          /*force_update=*/false);
  run_loop.Run();
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Execute JS to request fullscreen on the other display (on the right).
  RequestContentFullscreenOnScreen(1);
  EXPECT_EQ(gfx::Rect(800, 0, 800, 800), browser()->window()->GetBounds());

  // Ensure the exclusive access bubble is re-shown on fullscreen display swap.
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

// TODO(crbug.com/1134731): Disabled on Windows, where RenderWidgetHostViewAura
// blindly casts display::Screen::GetScreen() to display::win::ScreenWin*.
#if BUILDFLAG(IS_WIN)
#define MAYBE_FullscreenOnPermissionGrant DISABLED_FullscreenOnPermissionGrant
#else
#define MAYBE_FullscreenOnPermissionGrant FullscreenOnPermissionGrant
#endif
// Test requesting fullscreen using the permission grant's transient activation.
IN_PROC_BROWSER_TEST_F(MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_FullscreenOnPermissionGrant) {
  EXPECT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url, PAGE_TRANSITION_TYPED));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Request the Window Placement permission and accept the prompt after user
  // activation expires; accepting should grant a new transient activation
  // signal that can be used to request fullscreen, without another gesture.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  const std::string request_fullscreen_from_prompt_script = R"(
    (async () => {
      await document.body.requestFullscreen();
      return !!document.fullscreenElement;
    })();
  )";
  RequestContentFullscreenFromScript(request_fullscreen_from_prompt_script,
                                     content::EXECUTE_SCRIPT_NO_USER_GESTURE);
}

// Tests FullscreenController support for fullscreen companion windows.
class FullscreenCompanionWindowFullscreenControllerInteractiveTest
    : public MultiScreenFullscreenControllerInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  FullscreenCompanionWindowFullscreenControllerInteractiveTest() {
    feature_list_.InitWithFeatureState(
        blink::features::kWindowPlacementFullscreenCompanionWindow, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
// TODO(crbug.com/1310416): Disabled on Chrome OS for flakiness.
//
// Test requesting fullscreen on a specific screen and opening a cross-screen
// popup window from one gesture. Check the expected window activation pattern.
IN_PROC_BROWSER_TEST_P(
    FullscreenCompanionWindowFullscreenControllerInteractiveTest,
    DISABLED_FullscreenCompanionWindow) {
  SetUpTestScreenAndWindowPlacementTab();

  // Execute JS to request fullscreen and open a popup on separate screens.
  const std::string fullscreen_and_popup_script = R"(
    (async () => {
      if (!window.screenDetails)
          window.screenDetails = await window.getScreenDetails();
      const options = { screen: window.screenDetails.screens[0] };
      await document.body.requestFullscreen(options);
      if (navigator.userActivation.isActive)
        return false;
      const s = window.screenDetails.screens[1];
      const f = `left=${s.availLeft},top=${s.availTop},width=300,height=200`;
      window.open('.', '', f);
      return !!document.fullscreenElement;
    })();
  )";
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  RequestContentFullscreenFromScript(fullscreen_and_popup_script);
  EXPECT_EQ(gfx::Rect(0, 0, 800, 800), browser()->window()->GetBounds());
  // The popup opens iff kWindowPlacementFullscreenCompanionWindow is enabled.
  EXPECT_EQ(GetParam() ? 2u : 1u, browser_list->size());
  // Popup window activation is delayed until its opener exits fullscreen.
  EXPECT_EQ(browser(), browser_list->GetLastActive());
  ToggleTabFullscreen(/*enter_fullscreen=*/false);
  EXPECT_EQ(GetParam(), browser() != browser_list->GetLastActive());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FullscreenCompanionWindowFullscreenControllerInteractiveTest,
    ::testing::Bool());

// Tests FullscreenController support for fullscreen on screenschange events.
class FullscreenOnScreensChangeFullscreenControllerInteractiveTest
    : public MultiScreenFullscreenControllerInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  FullscreenOnScreensChangeFullscreenControllerInteractiveTest() {
    feature_list_.InitWithFeatureState(
        blink::features::kWindowPlacementFullscreenOnScreensChange, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1134731): Disabled on Windows, where RenderWidgetHostViewAura
// blindly casts display::Screen::GetScreen() to display::win::ScreenWin*.
// TODO(crbug.com/1183791): Disabled on Mac due to flaky ObserverList crashes.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_FullscreenOnScreensChange DISABLED_FullscreenOnScreensChange
#else
#define MAYBE_FullscreenOnScreensChange FullscreenOnScreensChange
#endif
// Tests async fullscreen requests on screenschange event.
IN_PROC_BROWSER_TEST_P(
    FullscreenOnScreensChangeFullscreenControllerInteractiveTest,
    MAYBE_FullscreenOnScreensChange) {
  content::WebContents* tab = SetUpTestScreenAndWindowPlacementTab();

  // Add a screenschange handler to requestFullscreen using the transient
  // affordance granted on screen change events, after user activation expiry.
  const std::string request_fullscreen_script = R"(
    (async () => {
      const screenDetails = await window.getScreenDetails();
      screenDetails.onscreenschange = async () => {
        if (!navigator.userActivation.isActive)
          await document.body.requestFullscreen();
      };
    })();
  )";
  EXPECT_TRUE(ExecJs(tab, request_fullscreen_script));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  WaitForUserActivationExpiry();

  // Update the display configuration to trigger window.onscreenschange.
  FullscreenNotificationObserver fullscreen_observer(browser());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("0+0-800x800");
#else
  display_list().RemoveDisplay(2);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetParam())  // The request will only be honored with the flag enabled.
    fullscreen_observer.Wait();
  EXPECT_EQ(GetParam(), browser()->window()->IsFullscreen());

  // Close all tabs to avoid assertions failing when their cached screen info
  // differs from the restored original Screen instance.
  browser()->tab_strip_model()->CloseAllTabs();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FullscreenOnScreensChangeFullscreenControllerInteractiveTest,
    ::testing::Bool());
