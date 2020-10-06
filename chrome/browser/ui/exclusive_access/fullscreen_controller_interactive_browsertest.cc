// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
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
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"
#endif  // OS_CHROMEOS

using url::kAboutBlankURL;
using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;

namespace {

const base::FilePath::CharType* kSimpleFile = FILE_PATH_LITERAL("simple.html");

}  // namespace

class FullscreenControllerInteractiveTest : public ExclusiveAccessTest {
 protected:

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
    EXPECT_TRUE(browser()->IsMouseLocked() ==
                browser()
                    ->tab_strip_model()
                    ->GetActiveWebContents()
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
      browser()->EnterFullscreenModeForTab(tab->GetMainFrame(), {});
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
  ASSERT_TRUE(embedded_test_server()->Start());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    AddTabAtIndex(1, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

// Tests a tab exiting fullscreen will bring the browser out of fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests Fullscreen entered in Browser, then Tab mode, then exited via Browser.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       BrowserFullscreenExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter tab fullscreen.
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  // Exit browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests Browser Fullscreen remains active after Tab mode entered and exited.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       BrowserFullscreenAfterTabFSExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter and then exit tab fullscreen.
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));

  // Verify browser fullscreen still active.
  ASSERT_TRUE(IsFullscreenForBrowser());
}

// Tests fullscreen entered without permision prompt for file:// urls.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest, FullscreenFileURL) {
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kSimpleFile)));

  // Validate that going fullscreen for a file does not ask permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests fullscreen is exited on page navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsFullscreenOnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests fullscreen is exited when navigating back.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsFullscreenOnGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

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

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Tests tab fullscreen exits, but browser fullscreen remains, on navigation.
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerInteractiveTest,
    TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_TRUE(IsFullscreenForBrowser());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

#if defined(OS_MAC)
// http://crbug.com/100467
IN_PROC_BROWSER_TEST_F(ExclusiveAccessTest,
                       DISABLED_TabEntersPresentationModeFromWindowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    browser()->EnterFullscreenModeForTab(tab->GetMainFrame(), {});
    fullscreen_observer.Wait();
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
#endif

// Tests mouse lock can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest, EscapingMouseLock) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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

// Disabled due to flakiness.
// TODO(crbug.com/976883): Fix and re-enable this.
// Tests mouse lock and fullscreen modes can be escaped with ESC key.
#if defined(OS_WIN)
#define MAYBE_EscapingMouseLockAndFullscreen EscapingMouseLockAndFullscreen
#else
#define MAYBE_EscapingMouseLockAndFullscreen \
  DISABLED_EscapingMouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_EscapingMouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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
// TODO(crbug.com/913409): UAv2 seems to make this flaky.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_MouseLockThenFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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

// Times out sometimes on Linux. http://crbug.com/135115
// Mac: http://crbug.com/103912
// Tests mouse lock then fullscreen in same request.
#if defined(OS_WIN)
#define MAYBE_MouseLockAndFullscreen MouseLockAndFullscreen
#else
#define MAYBE_MouseLockAndFullscreen DISABLED_MouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_MouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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

// Tests mouse lock and fullscreen for the privileged fullscreen case (e.g.,
// embedded flash fullscreen, since the Flash plugin handles user permissions
// requests itself).
// Flaky on Linux: crbug.com/1066607
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC)
#define MAYBE_PrivilegedMouseLockAndFullscreen \
  DISABLED_PrivilegedMouseLockAndFullscreen
#else
#define MAYBE_PrivilegedMouseLockAndFullscreen PrivilegedMouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_PrivilegedMouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  SetPrivilegedFullscreen(true);

  // Request to lock the mouse and enter fullscreen.
  FullscreenNotificationObserver fullscreen_observer(browser());
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_B);
  fullscreen_observer.Wait();

  // Confirm they are enabled and there is no prompt.
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Flaky on Linux, CrOS: http://crbug.com/159000
// Flaky on Windows; see https://crbug.com/791539.
// Flaky on Mac: https://crbug.com/876617.

// Tests mouse lock can be exited and re-entered by an application silently
// with no UI distraction for users.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_MouseLockSilentAfterTargetUnlock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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

  // Lock from target, not user gesture, make sure it works.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_D);
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Unlock by escape.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_ESCAPE);
  ASSERT_FALSE(IsMouseLocked());

  // Lock the mouse with a user gesture, make sure we see bubble again.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// These are flaky on linux_aura.
// http://crbug.com/163931
#define MAYBE_TestTabExitsMouseLockOnNavigation \
    DISABLED_TestTabExitsMouseLockOnNavigation
#define MAYBE_TestTabExitsMouseLockOnGoBack \
    DISABLED_TestTabExitsMouseLockOnGoBack
#else
#define MAYBE_TestTabExitsMouseLockOnNavigation \
    TestTabExitsMouseLockOnNavigation
#define MAYBE_TestTabExitsMouseLockOnGoBack \
    TestTabExitsMouseLockOnGoBack
#endif

// Tests mouse lock is exited on page navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(IsMouseLocked());
}

// Tests mouse lock is exited when navigating back.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate twice to provide a place to go back to.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  GoBack();

  ASSERT_FALSE(IsMouseLocked());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA) || \
    defined(OS_WIN) && defined(NDEBUG)
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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create URLs for test page and test page with #fragment.
  GURL url(embedded_test_server()->GetURL(kFullscreenMouseLockHTML));
  GURL url_with_fragment(url.spec() + "#fragment");

  // Navigate to test page.
  ui_test_utils::NavigateToURL(browser(), url);

  // Lock the mouse with a user gesture.
  PressKeyAndWaitForMouseLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  // Navigate to url with fragment. Mouse lock should persist.
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsMouseLocked());
}

// Tests Mouse Lock and Fullscreen are exited upon reload.
// http://crbug.com/137486
// mac: http://crbug.com/103912
#if defined(OS_WIN)
#define MAYBE_ReloadExitsMouseLockAndFullscreen \
  ReloadExitsMouseLockAndFullscreen
#else
#define MAYBE_ReloadExitsMouseLockAndFullscreen \
  DISABLED_ReloadExitsMouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_ReloadExitsMouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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
  AddTabAtIndex(0, url, PAGE_TRANSITION_TYPED);

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(false));
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests FullscreenController with experimental flags enabled.
class ExperimentalFullscreenControllerInteractiveTest
    : public FullscreenControllerInteractiveTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
    FullscreenControllerInteractiveTest::SetUpCommandLine(command_line);
  }
};

// TODO(crbug.com/1034772): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !defined(OS_CHROMEOS)
#define MAYBE_FullscreenOnSecondDisplay DISABLED_FullscreenOnSecondDisplay
#else
#define MAYBE_FullscreenOnSecondDisplay FullscreenOnSecondDisplay
#endif
// An end-to-end test that mocks a dual-screen configuration and executes
// javascript to request and exit fullscreen on the second display.
IN_PROC_BROWSER_TEST_F(ExperimentalFullscreenControllerInteractiveTest,
                       MAYBE_FullscreenOnSecondDisplay) {
  // Updates the display configuration to add a secondary display.
#if defined(OS_CHROMEOS)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-801x802");
#else
  display::Screen* original_screen = display::Screen::GetScreen();
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                   display::DisplayList::Type::NOT_PRIMARY);
  display::Screen::SetScreenInstance(&screen);
#endif  // OS_CHROMEOS
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  // Move the window to the first display (on the left).
  const gfx::Rect original_bounds(150, 150, 600, 500);
  browser()->window()->SetBounds(original_bounds);
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Auto-accept the Window Placement permission request.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Execute JS to request fullscreen on the second display (on the right).
  FullscreenNotificationObserver enter_fullscreen_observer(browser());
  const std::string request_fullscreen_script = R"(
      (async () => {
          const screens = await self.getScreens();
          let options = { screen: screens[1] };
          await document.body.requestFullscreen(options);
          return !!document.fullscreenElement;
      })();
  )";
  EXPECT_EQ(true, EvalJs(tab, request_fullscreen_script));
  enter_fullscreen_observer.Wait();
  EXPECT_TRUE(browser()->window()->IsFullscreen());
#if defined(OS_CHROMEOS)
  EXPECT_EQ(gfx::Rect(801, 0, 801, 802), browser()->window()->GetBounds());
#else
  EXPECT_EQ(gfx::Rect(901, 100, 801, 802), browser()->window()->GetBounds());
#endif  // OS_CHROMEOS

  // Execute JS to exit fullscreen.
  FullscreenNotificationObserver exit_fullscreen_observer(browser());
  const std::string exit_fullscreen_script = R"(
      (async () => {
          await document.exitFullscreen();
          return !!document.fullscreenElement;
      })();
  )";
  EXPECT_EQ(false, EvalJs(tab, exit_fullscreen_script));
  exit_fullscreen_observer.Wait();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());

#if !defined(OS_CHROMEOS)
  display::Screen::SetScreenInstance(original_screen);
#endif  // !OS_CHROMEOS
}

// Tests async fullscreen requests on screenschange event.
// TODO(crbug.com/1134731): Disabled on Windows, where RenderWidgetHostViewAura
// blindly casts display::Screen::GetScreen() to display::win::ScreenWin*.
#if defined(OS_WIN)
#define MAYBE_FullscreenOnScreensChange DISABLED_FullscreenOnScreensChange
#else
#define MAYBE_FullscreenOnScreensChange FullscreenOnScreensChange
#endif
IN_PROC_BROWSER_TEST_F(ExperimentalFullscreenControllerInteractiveTest,
                       MAYBE_FullscreenOnScreensChange) {
#if !defined(OS_CHROMEOS)
  // Install a mock screen object to be monitored by a new web contents.
  display::Screen* original_screen = display::Screen::GetScreen();
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  display::Screen::SetScreenInstance(&screen);
#endif  // OS_CHROMEOS

  // Open a new foreground tab that will observe the mock screen object.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  AddTabAtIndex(1, url, PAGE_TRANSITION_TYPED);
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Auto-accept the Window Placement permission request.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Add a screenschange handler to requestFullscreen after awaiting getScreens.
  const std::string request_fullscreen_script = R"(
      window.onscreenschange = async () => {
        const screens = await self.getScreens();
        await document.body.requestFullscreen();
      };
  )";
  EXPECT_TRUE(EvalJs(tab, request_fullscreen_script).error.empty());
  EXPECT_FALSE(browser()->window()->IsFullscreen());

  FullscreenNotificationObserver fullscreen_observer(browser());

  // Update the display configuration to trigger window.onscreenschange.
#if defined(OS_CHROMEOS)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-801x802");
#else
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                   display::DisplayList::Type::NOT_PRIMARY);
#endif  // OS_CHROMEOS

  fullscreen_observer.Wait();
  EXPECT_TRUE(browser()->window()->IsFullscreen());

#if !defined(OS_CHROMEOS)
  display::Screen::SetScreenInstance(original_screen);
#endif  // !OS_CHROMEOS
}
