// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/display/screen.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/nswindow_test_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // USE_AURA

using content::WebContents;

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

  // IsPointerLocked verifies that the FullscreenController state believes
  // the pointer is locked. This is possible only for tests that initiate
  // pointer lock from a renderer process, and uses logic that tests that the
  // browser has focus. Thus, this can only be used in interactive ui tests
  // and not on sharded tests.
  bool IsPointerLocked() {
    // Verify that IsPointerLocked is consistent between the
    // Fullscreen Controller and the Render View Host View.
    EXPECT_TRUE(browser()->IsPointerLocked() == browser()
                                                    ->tab_strip_model()
                                                    ->GetActiveWebContents()
                                                    ->GetPrimaryMainFrame()
                                                    ->GetRenderViewHost()
                                                    ->GetWidget()
                                                    ->GetView()
                                                    ->IsPointerLocked());
    return browser()->IsPointerLocked();
  }

  void PressKeyAndWaitForPointerLockRequest(ui::KeyboardCode key_code) {
    base::RunLoop run_loop;
    browser()
        ->exclusive_access_manager()
        ->pointer_lock_controller()
        ->set_lock_state_callback_for_test(run_loop.QuitClosure());
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key_code, false,
                                                false, false, false));
    run_loop.Run();
  }

  void WaitForPointerLockBubbleToHide() {
    PointerLockController* pointer_lock_controller =
        browser()->exclusive_access_manager()->pointer_lock_controller();
    base::RunLoop run_loop;
    pointer_lock_controller->set_bubble_hide_callback_for_test(
        base::BindRepeating(
            [](base::RunLoop* run_loop,
               ExclusiveAccessBubbleHideReason reason) {
              ASSERT_EQ(reason, ExclusiveAccessBubbleHideReason::kTimeout);
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
    pointer_lock_controller->set_bubble_hide_callback_for_test(
        base::NullCallback());
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

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
  ASSERT_EQ(IsFullscreenForBrowser(), enter_fullscreen);
}

void FullscreenControllerInteractiveTest::ToggleTabFullscreen_Internal(
    bool enter_fullscreen, bool retry_until_success) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  do {
    ui_test_utils::FullscreenWaiter waiter(
        browser(), {.tab_fullscreen = enter_fullscreen});
    if (enter_fullscreen) {
      browser()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
    } else {
      browser()->ExitFullscreenModeForTab(tab);
    }
    waiter.Wait();
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
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)
  // Flaky in Linux interactive_ui_tests_wayland: crbug.com/1200036
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland")
    GTEST_SKIP();
#endif

  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  {
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    ASSERT_TRUE(
        AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
    waiter.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

// Tests a tab exiting fullscreen will bring the browser out of fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

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

// Tests Fullscreen entered in Browser, then Tab mode, then exited via Browser.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       BrowserFullscreenExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter tab fullscreen.
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
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
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
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

// TODO(crbug.com/40779265) Flaky on Linux-ozone, Lacros and MacOS.
#if (BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
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
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  {
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
    EXPECT_TRUE(browser()->window()->IsFullscreen());
  }

  {
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
    EXPECT_FALSE(browser()->window()->IsFullscreen());
  }

  {
    // Test that tab fullscreen mode doesn't make presentation mode the default
    // on Lion.
    ui_test_utils::ToggleFullscreenModeAndWait(browser());
    EXPECT_TRUE(browser()->window()->IsFullscreen());
  }
}

// Tests pointer lock can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       EscapingPointerLock) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Request to lock the pointer.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);

  ASSERT_TRUE(IsPointerLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());

  // Escape, confirm we are out of pointer lock with no prompts.
  SendEscapeToExclusiveAccessManager();
  ASSERT_FALSE(IsPointerLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests pointer lock and fullscreen modes can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       EscapingPointerLockAndFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Request to lock the pointer and enter fullscreen.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    PressKeyAndWaitForPointerLockRequest(ui::VKEY_B);
    waiter.Wait();
  }

  // Escape, no prompts should remain.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    SendEscapeToExclusiveAccessManager();
    waiter.Wait();
  }
  ASSERT_FALSE(IsPointerLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests pointer lock then fullscreen.
// TODO(crbug.com/40835508): Re-enable this test
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PointerLockThenFullscreen DISABLED_PointerLockThenFullscreen
#else
#define MAYBE_PointerLockThenFullscreen PointerLockThenFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_PointerLockThenFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

#if !defined(MEMORY_SANITIZER)
  // Lock the pointer without a user gesture, expect no response.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_D);
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_FALSE(IsPointerLocked());
#else
  // MSan builds change the timing of user gestures, which this part of the test
  // depends upon.  See `fullscreen_pointerlock.html` for more details, but the
  // main idea is that it waits ~5 seconds after the keypress and assumes that
  // the user gesture has expired.
#endif

  // Lock the pointer with a user gesture.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsPointerLocked());

  // Enter fullscreen mode, pointer should remain locked.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsPointerLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Disabled on all due to issue with code under test: http://crbug.com/1255610.
//
// Was also disabled on platforms before:
// Times out sometimes on Linux. http://crbug.com/135115
// Mac: http://crbug.com/103912
// Tests pointer lock then fullscreen in same request.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_PointerLockAndFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Request to lock the pointer and enter fullscreen.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    PressKeyAndWaitForPointerLockRequest(ui::VKEY_B);
    waiter.Wait();
  }
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsPointerLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Tests pointer lock can be exited and re-entered by an application silently
// with no UI distraction for users.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       PointerLockSilentAfterTargetUnlock) {
  SetWebContentsGrantedSilentPointerLockPermission();
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock the pointer with a user gesture.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsPointerLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  // Wait for the bubble to be shown for its full duration. This allows
  // the page to lock the pointer without showing the bubble later.
  WaitForPointerLockBubbleToHide();

  // Unlock the pointer from target, make sure it's unlocked.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_U);
  ASSERT_FALSE(IsPointerLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock pointer again, make sure it works with no bubble.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsPointerLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Unlock the pointer again by target.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_U);
  ASSERT_FALSE(IsPointerLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());
}

// TODO: crbug.com/371511161 - Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SecondPointerLockShowsBubble DISABLED_SecondPointerLockShowsBubble
#else
#define MAYBE_SecondPointerLockShowsBubble SecondPointerLockShowsBubble
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_SecondPointerLockShowsBubble) {
  SetWebContentsGrantedSilentPointerLockPermission();
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock the pointer with a user gesture.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(IsPointerLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  // Unlock the pointer from target, make sure it's unlocked.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_U);
  ASSERT_FALSE(IsPointerLocked());
  ASSERT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Lock the pointer again. The bubble wasn't shown for its full duration last
  // time, so it gets shown again.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsPointerLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

// Tests pointer lock is exited on page navigation.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && defined(USE_AURA)
// https://crbug.com/1191964
#define MAYBE_TestTabExitsPointerLockOnNavigation \
  DISABLED_TestTabExitsPointerLockOnNavigation
#else
#define MAYBE_TestTabExitsPointerLockOnNavigation \
  TestTabExitsPointerLockOnNavigation
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsPointerLockOnNavigation) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  // Lock the pointer with a user gesture.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsPointerLocked());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  ASSERT_FALSE(IsPointerLocked());
}

// Tests pointer lock is exited when navigating back.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && defined(USE_AURA)
// https://crbug.com/1192097
#define MAYBE_TestTabExitsPointerLockOnGoBack \
  DISABLED_TestTabExitsPointerLockOnGoBack
#else
#define MAYBE_TestTabExitsPointerLockOnGoBack TestTabExitsPointerLockOnGoBack
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsPointerLockOnGoBack) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // Navigate twice to provide a place to go back to.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  // Lock the pointer with a user gesture.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsPointerLocked());

  GoBack();

  ASSERT_FALSE(IsPointerLocked());
}

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
        defined(USE_AURA) ||                                  \
    BUILDFLAG(IS_WIN) && defined(NDEBUG)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
// Test is flaky on Windows: https://crbug.com/1124492
#define MAYBE_TestTabDoesntExitPointerLockOnSubFrameNavigation \
  DISABLED_TestTabDoesntExitPointerLockOnSubFrameNavigation
#else
#define MAYBE_TestTabDoesntExitPointerLockOnSubFrameNavigation \
  TestTabDoesntExitPointerLockOnSubFrameNavigation
#endif

// Tests pointer lock is not exited on sub frame navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabDoesntExitPointerLockOnSubFrameNavigation) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // Create URLs for test page and test page with #fragment.
  GURL url(embedded_test_server()->GetURL(kFullscreenPointerLockHTML));
  GURL url_with_fragment(url.spec() + "#fragment");

  // Navigate to test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Lock the pointer with a user gesture.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  ASSERT_TRUE(IsPointerLocked());

  // Navigate to url with fragment. Pointer lock should persist.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_fragment));
  ASSERT_TRUE(IsPointerLocked());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       ReloadExitsPointerLockAndFullscreen) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenPointerLockHTML)));

  // Request pointer lock.
  PressKeyAndWaitForPointerLockRequest(ui::VKEY_1);

  ASSERT_TRUE(IsPointerLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());

  // Reload. Pointer lock request should be cleared.
  {
    base::RunLoop run_loop;
    browser()
        ->exclusive_access_manager()
        ->pointer_lock_controller()
        ->set_lock_state_callback_for_test(run_loop.QuitClosure());
    Reload();
    run_loop.Run();
  }

  // Request to lock the pointer and enter fullscreen.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    PressKeyAndWaitForPointerLockRequest(ui::VKEY_B);
    waiter.Wait();
  }

  // We are fullscreen.
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  // Reload. Pointer should be unlocked and fullscreen exited.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    Reload();
    waiter.Wait();
    ASSERT_FALSE(IsPointerLocked());
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
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(false));
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       OpeningPopupExitsFullscreen) {
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  // Open a popup, which is activated. The opener exits fullscreen to mitigate
  // usable security concerns. See WebContents::ForSecurityDropFullscreen().
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ExecuteScriptAsync(tab, "open('.', '', 'popup')");
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(2u, browser_list->size());
  ui_test_utils::BrowserActivationWaiter(popup).WaitForActivation();
  EXPECT_TRUE(ui_test_utils::IsBrowserActive(popup));
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       BlockingContentsExitsFullscreen) {
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  // Blocking the tab for a modal dialog exits fullscreen.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = false});
  static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(browser())
      ->SetWebContentsBlocked(tab, true);
  waiter.Wait();
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       CapturedContentEntersFullscreenWithinTab) {
  // Simulate tab capture, as used by getDisplayMedia() content sharing.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner capture_closure =
      tab->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/false,
                                  /*stay_awake=*/false, /*is_activity=*/true);
  EXPECT_TRUE(tab->IsBeingVisiblyCaptured());

  // The browser enters fullscreen-within-tab mode synchronously, but the window
  // is not made fullscreen, and FullscreenWaiter is not notified.
  content::WebContentsDelegate* delegate = tab->GetDelegate();
  delegate->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(tab->GetDelegate()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);

  delegate->ExitFullscreenModeForTab(tab);
  EXPECT_FALSE(delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(tab->IsFullscreen());
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(tab->GetDelegate()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kWindowed);

  capture_closure.RunAndReset();
  EXPECT_FALSE(tab->IsBeingVisiblyCaptured());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       OpeningPopupDoesNotExitFullscreenWithinTab) {
  // Simulate visible tab capture and enter fullscreen-within-tab.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner capture_closure =
      tab->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/false,
                                  /*stay_awake=*/false, /*is_activity=*/true);
  tab->GetDelegate()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  EXPECT_EQ(tab->GetDelegate()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);
  EXPECT_TRUE(tab->IsFullscreen());

  // Open a popup, which is activated. The opener remains fullscreen-within-tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  content::ExecuteScriptAsync(tab, "open('.', '', 'popup')");
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(popup);
  ui_test_utils::WaitUntilBrowserBecomeActive(popup);
  EXPECT_EQ(2u, browser_list->size());
  EXPECT_EQ(tab->GetDelegate()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       BlockingContentsDoesNotExitFullscreenWithinTab) {
  // Simulate visible tab capture and enter fullscreen-within-tab.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner capture_closure =
      tab->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/false,
                                  /*stay_awake=*/false, /*is_activity=*/true);
  tab->GetDelegate()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  EXPECT_EQ(tab->GetDelegate()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);
  EXPECT_TRUE(tab->IsFullscreen());

  // Blocking the tab for a modal dialog does not exit fullscreen-within-tab.
  static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(browser())
      ->SetWebContentsBlocked(tab, true);
  EXPECT_EQ(tab->GetDelegate()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);
}

// Tests the automatic fullscreen content setting in IWA and non-IWA contexts.
class AutomaticFullscreenTest : public FullscreenControllerInteractiveTest,
                                public testing::WithParamInterface<bool> {
 public:
  AutomaticFullscreenTest() {
    feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode,
         features::kAutomaticFullscreenContentSetting},
        {});
  }

  void SetUpOnMainThread() override {
    auto allow_automatic_fullscreen = [&](const GURL& url) {
      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->SetContentSettingDefaultScope(
              url, url, ContentSettingsType::AUTOMATIC_FULLSCREEN,
              CONTENT_SETTING_ALLOW);
    };

    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    if (GetParam()) {
      embedded_https_test_server().ServeFilesFromSourceDirectory(
          GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
      ASSERT_TRUE(embedded_https_test_server().Start());
      auto url_info = web_app::InstallDevModeProxyIsolatedWebApp(
          browser()->profile(), embedded_https_test_server().GetOrigin());
      allow_automatic_fullscreen(url_info.origin().GetURL());
      auto* frame =
          web_app::OpenIsolatedWebApp(browser()->profile(), url_info.app_id());
      web_contents_ = content::WebContents::FromRenderFrameHost(frame);
    } else {
      ASSERT_TRUE(embedded_https_test_server().Start());
      GURL url = embedded_https_test_server().GetURL("a.com", "/simple.html");
      allow_automatic_fullscreen(url);
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    }
    ASSERT_TRUE(WaitForRenderFrameReady(web_contents_->GetPrimaryMainFrame()));
  }

  void TearDownOnMainThread() override { web_contents_ = nullptr; }

  bool RequestFullscreen(bool gesture = false,
                         content::RenderFrameHost* rfh = nullptr) {
    static constexpr char kScript[] = R"JS(
        (async () => {
          try { await document.body.requestFullscreen(); } catch {}
          return !!document.fullscreenElement;
        })();
    )JS";

    auto options = gesture ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                           : content::EXECUTE_SCRIPT_NO_USER_GESTURE;
    rfh = rfh ? rfh : web_contents_->GetPrimaryMainFrame();
    content::WebContents* tab = content::WebContents::FromRenderFrameHost(rfh);
    Browser* browser = chrome::FindBrowserWithTab(tab);
    if (!gesture) {
      // Ensure nothing inadvertently triggered user activation beforehand.
      EXPECT_EQ(false, EvalJs(rfh, "navigator.userActivation.isActive",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    }
    ui_test_utils::FullscreenWaiter waiter(browser, {.tab_fullscreen = true});
    auto result = EvalJs(rfh, kScript, options);
    if (result.error.empty() && result.ExtractBool()) {
      waiter.Wait();
    }
    return browser->window()->IsFullscreen();
  }

  bool ExitFullscreen(WebContents* web_contents = nullptr) {
    web_contents = web_contents ? web_contents : web_contents_.get();
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser, {.tab_fullscreen = false});
    const std::string script = R"((() => {
      window.lastExit = Date.now();
      return document.exitFullscreen();
    })())";
    // A user gesture is not needed and may break subsequent activation checks.
    auto result =
        EvalJs(web_contents, script, content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    waiter.Wait();
    return result.error.empty() && !browser->window()->IsFullscreen();
  }

  std::pair<bool, Browser*> OpenPopupAndRequestFullscreenOnLoad() {
    ui_test_utils::BrowserChangeObserver popup_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    const std::string script = R"((() => {
      let w = open(location.href, '', 'popup');
      return new Promise(resolve => {
        w.onload = async () => {
          try { await w.document.body.requestFullscreen(); } catch {}
          resolve(!!w.document.fullscreenElement);
        };
      });
    })())";

    Browser* browser = chrome::FindBrowserWithTab(web_contents_);
    auto result = EvalJs(web_contents_, script);
    Browser* popup = popup_observer.Wait();
    if (!popup) {
      return std::make_pair(false, nullptr);
    }
    EXPECT_NE(popup, browser);
    ui_test_utils::WaitUntilBrowserBecomeActive(popup);
    ui_test_utils::FullscreenWaiter waiter(popup, {.tab_fullscreen = true});
    if (result.error.empty() && result.ExtractBool()) {
      waiter.Wait();
    }
    return std::make_pair(popup->window()->IsFullscreen(), popup);
  }

  std::string QueryPermission(
      const content::ToRenderFrameHost& target,
      std::optional<bool> allow_without_gesture = true) {
    const std::string options =
        allow_without_gesture.has_value()
            ? content::JsReplace(", allowWithoutGesture: $1",
                                 allow_without_gesture.value())
            : "";
    const std::string descriptor = "{name: 'fullscreen'" + options + "}";
    const std::string script =
        "navigator.permissions.query(" + descriptor +
        ").then(permission => permission.state).catch(e => e.name);";
    return EvalJs(target, script, content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractString();
  }

 protected:
  raw_ptr<content::WebContents> web_contents_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, RequestFullscreenNoGesture) {
  base::HistogramTester histograms;
  EXPECT_TRUE(RequestFullscreen());

  // Navigate away in order to flush use counters.
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, GURL(url::kAboutBlankURL)));
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  if (!GetParam()) {  // TODO(crbug.com/41497058): Test use counter in IWA too.
    histograms.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kFullscreenAllowedByContentSetting, 1);
  }
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, ImmediatelyAfterExit) {
  EXPECT_TRUE(RequestFullscreen());
  const base::TimeTicks exit = base::TimeTicks::Now();
  EXPECT_TRUE(ExitFullscreen());
  EXPECT_LT(base::TimeTicks::Now() - exit, base::Seconds(5));
  EXPECT_FALSE(RequestFullscreen());
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, WithGestureAfterExit) {
  EXPECT_TRUE(RequestFullscreen());
  EXPECT_TRUE(ExitFullscreen());
  EXPECT_TRUE(RequestFullscreen(/*gesture=*/true));
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, EventuallyAfterExit) {
  EXPECT_TRUE(RequestFullscreen());
  EXPECT_TRUE(ExitFullscreen());
  base::RunLoop run_loop;
  // TODO(crbug.com/333133285): Avoid waiting this long in wall-clock time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(5300));
  run_loop.Run();
  EXPECT_TRUE(RequestFullscreen());
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, Popup) {
  EXPECT_TRUE(OpenPopupAndRequestFullscreenOnLoad().first);
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, PopupImmediatelyAfterExit) {
  EXPECT_TRUE(RequestFullscreen());
  const base::TimeTicks exit = base::TimeTicks::Now();
  EXPECT_TRUE(ExitFullscreen());
  EXPECT_LT(base::TimeTicks::Now() - exit, base::Seconds(5));
  EXPECT_FALSE(OpenPopupAndRequestFullscreenOnLoad().first);
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, PopupEventuallyAfterExit) {
  EXPECT_TRUE(RequestFullscreen());
  EXPECT_TRUE(ExitFullscreen());
  base::RunLoop run_loop;
  // TODO(crbug.com/333133285): Avoid waiting this long in wall-clock time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(5300));
  run_loop.Run();
  EXPECT_TRUE(OpenPopupAndRequestFullscreenOnLoad().first);
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, ImmediatelyAfterPopupExit) {
  auto [success, popup] = OpenPopupAndRequestFullscreenOnLoad();
  EXPECT_TRUE(success);
  ASSERT_TRUE(popup);
  const base::TimeTicks exit = base::TimeTicks::Now();
  ExitFullscreen(popup->tab_strip_model()->GetActiveWebContents());
  EXPECT_LT(base::TimeTicks::Now() - exit, base::Seconds(5));
  EXPECT_FALSE(RequestFullscreen());
  popup->window()->Close();
  ui_test_utils::WaitForBrowserToClose(popup);
  EXPECT_LT(base::TimeTicks::Now() - exit, base::Seconds(5));
  EXPECT_FALSE(RequestFullscreen());
  EXPECT_TRUE(RequestFullscreen(/*gesture=*/true));
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, EventuallyAfterPopupExit) {
  auto [success, popup] = OpenPopupAndRequestFullscreenOnLoad();
  EXPECT_TRUE(success);
  ASSERT_TRUE(popup);
  ExitFullscreen(popup->tab_strip_model()->GetActiveWebContents());
  base::RunLoop run_loop;
  // TODO(crbug.com/333133285): Avoid waiting this long in wall-clock time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(5300));
  run_loop.Run();
  EXPECT_TRUE(RequestFullscreen());
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, BlockingContentsDoesNotExit) {
  EXPECT_TRUE(RequestFullscreen());
  EXPECT_TRUE(web_contents_->IsFullscreen());
  // Blocking the tab for a modal dialog does not exit fullscreen if the origin
  // has been granted the automatic fullscreen content setting.
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(browser)
      ->SetWebContentsBlocked(web_contents_, true);
  EXPECT_TRUE(web_contents_->IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, QueryPermissionWithGesture) {
  // Expect an API TypeError when allowWithoutGesture is false or unspecified.
  EXPECT_EQ(
      "TypeError",
      QueryPermission(web_contents_, /*allow_without_gesture=*/std::nullopt));
  EXPECT_EQ("TypeError",
            QueryPermission(web_contents_, /*allow_without_gesture=*/false));
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, QueryPermissionWithoutGesture) {
  // Permission is pre-granted on the initial test origin and denied elsewhere.
  EXPECT_EQ("granted", QueryPermission(web_contents_));
  const GURL url = embedded_https_test_server().GetURL("b.com", "/simple.html");
  content::RenderFrameHost* rfh = ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ("denied", QueryPermission(rfh));
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, CrossOriginIFrameDenied) {
  // Append a cross-origin iframe without the permission policy.
  const GURL src = embedded_https_test_server().GetURL("b.com", "/simple.html");
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  web_app::CreateIframe(rfh, "", src, /*permissions_policy=*/"");
  content::RenderFrameHost* child = ChildFrameAt(rfh, 0);
  EXPECT_EQ("denied", QueryPermission(child));
  EXPECT_FALSE(RequestFullscreen(/*gesture=*/false, child));
}

IN_PROC_BROWSER_TEST_P(AutomaticFullscreenTest, CrossOriginIFrameGranted) {
  // Append a cross-origin iframe with the permission policy.
  const GURL src = embedded_https_test_server().GetURL("b.com", "/simple.html");
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  web_app::CreateIframe(rfh, "", src, /*permissions_policy=*/"fullscreen *");
  content::RenderFrameHost* child = ChildFrameAt(rfh, 0);
  EXPECT_EQ("granted", QueryPermission(child));
  EXPECT_TRUE(RequestFullscreen(child));
  EXPECT_TRUE(ExitFullscreen());
}

INSTANTIATE_TEST_SUITE_P(, AutomaticFullscreenTest, ::testing::Bool());

// Tests fullscreen with multi-screen features from the Window Management API.
// Sites with the Window Management permission can request fullscreen on a
// specific screen, move fullscreen windows to different displays, and more.
// Tests must run in series to manage virtual displays on supported platforms.
// Use 2+ physical displays to run locally with --gtest_also_run_disabled_tests.
// See: //docs/ui/display/multiscreen_testing.md
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_MultiScreenFullscreenControllerInteractiveTest \
  MultiScreenFullscreenControllerInteractiveTest
#else
#define MAYBE_MultiScreenFullscreenControllerInteractiveTest \
  DISABLED_MultiScreenFullscreenControllerInteractiveTest
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class MAYBE_MultiScreenFullscreenControllerInteractiveTest
    : public FullscreenControllerInteractiveTest {
 public:
  void SetUpOnMainThread() override {
    if (!SetUpVirtualDisplays()) {
      GTEST_SKIP() << "Skipping test; unavailable multi-screen support.";
    }
    display::Screen* screen = display::Screen::GetScreen();
    for (const auto& d : screen->GetAllDisplays()) {
      if (d.id() != screen->GetPrimaryDisplay().id()) {
        secondary_display_id_ = d.id();
        break;
      }
    }
#if BUILDFLAG(IS_MAC)
    ns_window_faked_for_testing_ = ui::NSWindowFakedForTesting::IsEnabled();
    // Disable `NSWindowFakedForTesting` to wait for actual async fullscreen on
    // Mac via `FullscreenWaiter`.
    ui::NSWindowFakedForTesting::SetEnabled(false);
#endif
  }

  void TearDownOnMainThread() override {
    virtual_display_util_.reset();
#if BUILDFLAG(IS_MAC)
    ui::NSWindowFakedForTesting::SetEnabled(ns_window_faked_for_testing_);
#endif
  }

  // Create virtual displays as needed, ensuring 2 displays are available for
  // testing multi-screen functionality. Not all platforms and OS versions are
  // supported. Returns false if virtual displays could not be created.
  bool SetUpVirtualDisplays() {
    if (display::Screen::GetScreen()->GetNumDisplays() > 1) {
      return true;
    }
    if ((virtual_display_util_ = display::test::VirtualDisplayUtil::TryCreate(
             display::Screen::GetScreen()))) {
      virtual_display_util_->AddDisplay(
          display::test::VirtualDisplayUtil::k1024x768);
      return true;
    }
    return false;
  }

  // Get a new tab that observes the test screen environment and auto-accepts
  // Window Management permission prompts.
  content::WebContents* SetUpWindowManagementTab() {
    // Open a new tab that observes the test screen environment.
    EXPECT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL("/simple.html"));
    EXPECT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED));

    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

    // Auto-accept Window Management permission prompts.
    permissions::PermissionRequestManager* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(tab);
    permission_request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);

    return tab;
  }

  // Get the display matching the `browser`'s current window bounds.
  display::Display GetCurrentDisplay(Browser* browser) const {
    return display::Screen::GetScreen()->GetDisplayMatching(
        browser->window()->GetBounds());
  }

  // Wait for a JS content fullscreen change with the given script and options.
  // Returns the script result.
  content::EvalJsResult RequestContentFullscreenFromScript(
      const std::string& eval_js_script,
      bool expect_fullscreen,
      int eval_js_options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
      bool expect_window_fullscreen = true,
      std::optional<int64_t> display_id = std::nullopt) {
    ui_test_utils::FullscreenWaiter waiter(
        browser(),
        {.tab_fullscreen = expect_fullscreen, .display_id = display_id});
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    content::EvalJsResult result = EvalJs(tab, eval_js_script, eval_js_options);
    waiter.Wait();
    EXPECT_EQ(expect_window_fullscreen, browser()->window()->IsFullscreen());
    return result;
  }

  // Execute JS to request content fullscreen on the current screen.
  void RequestContentFullscreen() {
    const std::string script = R"JS(
      (async () => {
        await document.body.requestFullscreen();
        return !!document.fullscreenElement;
      })();
    )JS";
    EXPECT_EQ(true, RequestContentFullscreenFromScript(script, true));
  }

  // Execute JS to request content fullscreen on a different screen from where
  // the window is currently located.
  void RequestContentFullscreenOnAnotherScreen() {
    const std::string script = R"JS(
      (async () => {
        if (!window.screenDetails)
          window.screenDetails = await window.getScreenDetails();
        const otherScreen = window.screenDetails.screens.find(
            s => s !== window.screenDetails.currentScreen);
        const options = { screen: otherScreen };
        await document.body.requestFullscreen(options);
        return !!document.fullscreenElement;
      })();
    )JS";
    EXPECT_EQ(true, RequestContentFullscreenFromScript(
                        script, true, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                        true, secondary_display_id_));
  }

  // Execute JS to exit content fullscreen.
  void ExitContentFullscreen(bool expect_window_fullscreen = false) {
    const std::string script = R"JS(
      (async () => {
        await document.exitFullscreen();
        return !!document.fullscreenElement;
      })();
    )JS";
    // Exiting fullscreen does not require a user gesture; do not supply one.
    EXPECT_EQ(false, RequestContentFullscreenFromScript(
                         script, false, content::EXECUTE_SCRIPT_NO_USER_GESTURE,
                         expect_window_fullscreen));
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
    EXPECT_FALSE(tab->HasRecentInteraction());
  }

 private:
  std::unique_ptr<display::test::VirtualDisplayUtil> virtual_display_util_;
  uint64_t secondary_display_id_ = display::kInvalidDisplayId;
#if BUILDFLAG(IS_MAC)
  bool ns_window_faked_for_testing_ = false;
#endif
};

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SeparateDisplay SeparateDisplay
#else
#define MAYBE_SeparateDisplay DISABLED_SeparateDisplay
#endif
// Test requesting fullscreen on a separate display.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SeparateDisplay) {
  SetUpWindowManagementTab();
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect original_bounds = browser()->window()->GetBounds();
#endif
  const display::Display original_display = GetCurrentDisplay(browser());

  // Execute JS to request fullscreen on a different screen.
  RequestContentFullscreenOnAnotherScreen();
  EXPECT_NE(original_display.id(), GetCurrentDisplay(browser()).id());

  ExitContentFullscreen();
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
#endif
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SeparateDisplayMaximized SeparateDisplayMaximized
#else
#define MAYBE_SeparateDisplayMaximized DISABLED_SeparateDisplayMaximized
#endif
// Test requesting fullscreen on a separate display from a maximized window.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SeparateDisplayMaximized) {
  SetUpWindowManagementTab();
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect original_bounds = browser()->window()->GetBounds();
#endif
  const display::Display original_display = GetCurrentDisplay(browser());

  browser()->window()->Maximize();
  EXPECT_TRUE(browser()->window()->IsMaximized());
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect maximized_bounds = browser()->window()->GetBounds();
#endif

  // Execute JS to request fullscreen on a different screen.
  RequestContentFullscreenOnAnotherScreen();
  EXPECT_NE(original_display.id(), GetCurrentDisplay(browser()).id());

  ExitContentFullscreen();
  EXPECT_TRUE(browser()->window()->IsMaximized());
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(maximized_bounds, browser()->window()->GetBounds());
#endif

  // Unmaximize the window and check that the original bounds are restored.
  browser()->window()->Restore();
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
#endif
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SameDisplayAndSwap SameDisplayAndSwap
#else
#define MAYBE_SameDisplayAndSwap DISABLED_SameDisplayAndSwap
#endif
// Test requesting fullscreen on the current display and then swapping displays.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SameDisplayAndSwap) {
  SetUpWindowManagementTab();
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect original_bounds = browser()->window()->GetBounds();
#endif
  const display::Display original_display = GetCurrentDisplay(browser());

  // Execute JS to request fullscreen on the current screen.
  RequestContentFullscreen();
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());

  // Execute JS to request fullscreen on a different screen.
  RequestContentFullscreenOnAnotherScreen();
  EXPECT_NE(original_display.id(), GetCurrentDisplay(browser()).id());

  ExitContentFullscreen();
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
#endif
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SameDisplayAndSwapMaximized SameDisplayAndSwapMaximized
#else
#define MAYBE_SameDisplayAndSwapMaximized DISABLED_SameDisplayAndSwapMaximized
#endif
// Test requesting fullscreen on the current display and then swapping displays
// from a maximized window.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SameDisplayAndSwapMaximized) {
  SetUpWindowManagementTab();
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect original_bounds = browser()->window()->GetBounds();
#endif
  const display::Display original_display = GetCurrentDisplay(browser());

  browser()->window()->Maximize();
  EXPECT_TRUE(browser()->window()->IsMaximized());
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect maximized_bounds = browser()->window()->GetBounds();
#endif

  // Execute JS to request fullscreen on the current screen.
  RequestContentFullscreen();
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());

  // Execute JS to request fullscreen on a different screen.
  RequestContentFullscreenOnAnotherScreen();
  EXPECT_NE(original_display.id(), GetCurrentDisplay(browser()).id());

  ExitContentFullscreen();
  EXPECT_TRUE(browser()->window()->IsMaximized());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(maximized_bounds, browser()->window()->GetBounds());
#endif

  // Unmaximize the window and check that the original bounds are restored.
  browser()->window()->Restore();
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
#endif
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_BrowserFullscreenContentFullscreenSwapDisplay \
  BrowserFullscreenContentFullscreenSwapDisplay
#else
#define MAYBE_BrowserFullscreenContentFullscreenSwapDisplay \
  DISABLED_BrowserFullscreenContentFullscreenSwapDisplay
#endif
// Test requesting browser fullscreen on current display, launching
// tab-fullscreen on a different display, and then closing tab-fullscreen to
// restore browser-fullscreen on the original display.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_BrowserFullscreenContentFullscreenSwapDisplay) {
  SetUpWindowManagementTab();

  ToggleBrowserFullscreen(true);
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());

  const gfx::Rect fullscreen_bounds = browser()->window()->GetBounds();
  const display::Display original_display = GetCurrentDisplay(browser());

  // On the Mac, the available fullscreen space is not always the entire
  // screen. In non-immersive, on machines with a notch, the menu bar is not
  // visible, but there's a black bar at the top of the screen. In immersive
  // fullscreen, the top chrome appears to be part of the browser window but
  // is actually in a separate widget/window (the overlay widget) positioned
  // just above. The fullscreen bounds rect is therefore reduced in height
  // by the notch bar (maybe) and top chrome.
  //
  // What should always be true is the left, right, and bottom sides of the
  // fullscreen bounds match the those portions of the display bounds. The
  // top is trickier. By using the "work area," we should be able to take the
  // menu bar area out of the equation. Ideally, we would just check that
  // fullscreen_bounds.y - overlay_widget.height == display.work_area.bottom.
  // However, at this location in the source tree, we are not allowed to know
  // anything about Views or widgets, so we cannot access the overlay_widget
  // to query its frame. The most we can, therefore, say, is that the top
  // of the fullscreen bounds must be greater than or equal to the bottom of
  // the display bounds.
#if BUILDFLAG(IS_MAC)
  EXPECT_LE(original_display.work_area().y(), fullscreen_bounds.y());
  EXPECT_EQ(original_display.work_area().x(), fullscreen_bounds.x());
  EXPECT_EQ(original_display.work_area().right(), fullscreen_bounds.right());
  EXPECT_EQ(original_display.work_area().bottom(), fullscreen_bounds.bottom());
#else
  EXPECT_EQ(original_display.bounds(), fullscreen_bounds);
#endif  // BUILDFLAG(IS_MAC)

  // Execute JS to request fullscreen on a different screen.
  RequestContentFullscreenOnAnotherScreen();
  EXPECT_NE(original_display.id(), GetCurrentDisplay(browser()).id());
  // Fullscreen was originally initiated by browser, this should still be true.
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_TRUE(IsWindowFullscreenForTabOrPending());

  ExitContentFullscreen(/*expect_window_fullscreen=*/true);
  EXPECT_EQ(fullscreen_bounds, browser()->window()->GetBounds());
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_FALSE(IsWindowFullscreenForTabOrPending());
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SeparateDisplayAndSwap SeparateDisplayAndSwap
#else
#define MAYBE_SeparateDisplayAndSwap DISABLED_SeparateDisplayAndSwap
#endif
// Test requesting fullscreen on a separate display and then swapping displays.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SeparateDisplayAndSwap) {
  SetUpWindowManagementTab();
#if !BUILDFLAG(IS_MAC)
  const gfx::Rect original_bounds = browser()->window()->GetBounds();
#endif
  const display::Display original_display = GetCurrentDisplay(browser());
  display::Display last_recorded_display = original_display;

  // Execute JS to request fullscreen on a different screen a few times.
  for (size_t i = 0; i < 4; ++i) {
    RequestContentFullscreenOnAnotherScreen();
    EXPECT_NE(last_recorded_display.id(), GetCurrentDisplay(browser()).id());
    last_recorded_display = GetCurrentDisplay(browser());
  }

  ExitContentFullscreen();
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  // TODO(crbug.com/40277425): Bounds are flaky on Mac.
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(original_bounds, browser()->window()->GetBounds());
#endif
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Linux, where the
// window server's async handling of the fullscreen window state may transition
// the window into fullscreen on the actual (non-mocked) display bounds before
// or after the window bounds checks, yielding flaky results.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SwapShowsBubble SwapShowsBubble
#else
#define MAYBE_SwapShowsBubble DISABLED_SwapShowsBubble
#endif
// Test requesting fullscreen on the current display and then swapping displays.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_SwapShowsBubble) {
  SetUpWindowManagementTab();

  // Execute JS to request fullscreen on the current screen.
  RequestContentFullscreen();
  const display::Display original_display = GetCurrentDisplay(browser());

  // Explicitly check for, and destroy, the exclusive access bubble.
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
  base::RunLoop run_loop;
  ExclusiveAccessBubbleHideCallback callback = base::BindLambdaForTesting(
      [&run_loop](ExclusiveAccessBubbleHideReason) { run_loop.Quit(); });
  browser()->exclusive_access_manager()->context()->UpdateExclusiveAccessBubble(
      {}, std::move(callback));
  run_loop.Run();
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // Execute JS to request fullscreen on a different screen.
  RequestContentFullscreenOnAnotherScreen();
  EXPECT_NE(original_display.id(), GetCurrentDisplay(browser()).id());

  // Ensure the exclusive access bubble is re-shown on fullscreen display swap.
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

// TODO(crbug.com/40723237): Disabled on Windows, where RenderWidgetHostViewAura
// blindly casts display::Screen::GetScreen() to display::win::ScreenWin*.
#if BUILDFLAG(IS_WIN)
#define MAYBE_FullscreenOnPermissionGrant DISABLED_FullscreenOnPermissionGrant
#else
#define MAYBE_FullscreenOnPermissionGrant FullscreenOnPermissionGrant
#endif
// Test requesting fullscreen using the permission grant's transient activation.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_FullscreenOnPermissionGrant) {
  EXPECT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  ASSERT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Request the Window Management permission and accept the prompt after user
  // activation expires; accepting should grant a new transient activation
  // signal that can be used to request fullscreen, without another gesture.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  const std::string script = R"(
    (async () => {
      await document.body.requestFullscreen();
      return !!document.fullscreenElement;
    })();
  )";
  EXPECT_EQ(true, RequestContentFullscreenFromScript(
                      script, true, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_OpenPopupWhileFullscreen DISABLED_OpenPopupWhileFullscreen
#else
#define MAYBE_OpenPopupWhileFullscreen OpenPopupWhileFullscreen
#endif
// Test opening a popup on a separate display while fullscreen.
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_OpenPopupWhileFullscreen) {
  content::WebContents* tab = SetUpWindowManagementTab();
  const display::Display original_display = GetCurrentDisplay(browser());

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  blocked_content::PopupBlockerTabHelper* popup_blocker =
      blocked_content::PopupBlockerTabHelper::FromWebContents(tab);
  EXPECT_EQ(0u, popup_blocker->GetBlockedPopupsCount());

  // Execute JS to request fullscreen on the current screen.
  RequestContentFullscreen();
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());

  // Execute JS to open a popup on a different screen.
  const std::string script = R"(
    (async () => {
      // Note: WindowManagementPermissionContext will send an activation signal.
      window.screenDetails = await window.getScreenDetails();
      const otherScreen = window.screenDetails.screens.find(
          s => s !== window.screenDetails.currentScreen);
      const l = otherScreen.availLeft + 100;
      const t = otherScreen.availTop + 100;
      const w = window.open('', '', `left=${l},top=${t},width=300,height=300`);
      // Return true iff the opener is fullscreen and the popup is open.
      return !!document.fullscreenElement && !!w && !w.closed;
    })();
  )";
  content::ExecuteScriptAsync(tab, script);
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_NE(popup, browser());
  auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetPrimaryMainFrame()));
  EXPECT_EQ(0u, popup_blocker->GetBlockedPopupsCount());
  EXPECT_EQ(2u, browser_list->size());
  EXPECT_EQ(original_display.id(), GetCurrentDisplay(browser()).id());
  EXPECT_NE(original_display.id(), GetCurrentDisplay(popup).id());

  // The opener should still be fullscreen.
  EXPECT_TRUE(IsWindowFullscreenForTabOrPending());
  // Popup window activation is delayed until its opener exits fullscreen.
  EXPECT_FALSE(ui_test_utils::IsBrowserActive(popup));
  ToggleTabFullscreen(/*enter_fullscreen=*/false);
  ui_test_utils::BrowserActivationWaiter(popup).WaitForActivation();
  EXPECT_TRUE(ui_test_utils::IsBrowserActive(popup));
}

// TODO(crbug.com/40111905): Disabled on Windows, where views::FullscreenHandler
// implements fullscreen by directly obtaining MONITORINFO, ignoring the mocked
// display::Screen configuration used in this test. Disabled on Mac and Linux,
// where the window server's async handling of the fullscreen window state may
// transition the window into fullscreen on the actual (non-mocked) display
// bounds before or after the window bounds checks, yielding flaky results.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_FullscreenCompanionWindow DISABLED_FullscreenCompanionWindow
#else
#define MAYBE_FullscreenCompanionWindow FullscreenCompanionWindow
#endif
// Test requesting fullscreen on a specific screen and opening a cross-screen
// popup window from one gesture. Check the expected window activation pattern.
// https://w3c.github.io/window-management/#usage-overview-initiate-multi-screen-experiences
IN_PROC_BROWSER_TEST_F(MAYBE_MultiScreenFullscreenControllerInteractiveTest,
                       MAYBE_FullscreenCompanionWindow) {
  content::WebContents* tab = SetUpWindowManagementTab();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  blocked_content::PopupBlockerTabHelper* popup_blocker =
      blocked_content::PopupBlockerTabHelper::FromWebContents(tab);
  EXPECT_EQ(0u, popup_blocker->GetBlockedPopupsCount());

  // Execute JS to request fullscreen and open a popup on separate screens.
  const std::string script = R"(
    (async () => {
      // Note: WindowManagementPermissionContext will send an activation signal.
      window.screenDetails = await window.getScreenDetails();

      const fullscreen_change_promise = new Promise(resolve => {
          function waitAndRemove(e) {
              document.removeEventListener("fullscreenchange", waitAndRemove);
              document.removeEventListener("fullscreenerror", waitAndRemove);
              resolve(document.fullscreenElement);
          }
          document.addEventListener("fullscreenchange", waitAndRemove);
          document.addEventListener("fullscreenerror", waitAndRemove);
      });

      // Request fullscreen and ensure that transient activation is consumed.
      const options = { screen: window.screenDetails.screens[0] };
      const fullscreen_promise = document.body.requestFullscreen(options);
      if (navigator.userActivation.isActive) {
        console.error("Transient activation unexpectedly not consumed");
        return false;
      }

      // Attempt to open a fullscreen companion window.
      const s = window.screenDetails.screens[1];
      const f = `left=${s.availLeft},top=${s.availTop},width=300,height=200`;
      const w = window.open('.', '', f);

      // Now await the fullscreen promise and change (or error) event.
      await fullscreen_promise;
      if (!await fullscreen_change_promise) {
        console.error("Unexpected fullscreen change or error");
        return false;
      }

      // Return true iff the opener is fullscreen and the popup is open.
      return !!document.fullscreenElement && !!w && !w.closed;
    })();
  )";
  EXPECT_TRUE(RequestContentFullscreenFromScript(script, true).ExtractBool());
  EXPECT_TRUE(IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(0u, popup_blocker->GetBlockedPopupsCount());
  EXPECT_EQ(2u, browser_list->size());
  Browser* popup = browser_list->get(1);
  EXPECT_NE(browser(), popup);
  EXPECT_NE(GetCurrentDisplay(browser()).id(), GetCurrentDisplay(popup).id());

  // Popup window activation is delayed until its opener exits fullscreen.
  EXPECT_FALSE(ui_test_utils::IsBrowserActive(popup));
  ToggleTabFullscreen(/*enter_fullscreen=*/false);
  ui_test_utils::BrowserActivationWaiter(popup).WaitForActivation();
  EXPECT_TRUE(ui_test_utils::IsBrowserActive(popup));
}
