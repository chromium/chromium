// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"

#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using ExclusiveAccessManagerTest = ExclusiveAccessTest;

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest, HandleKeyEvent_NonEscKey) {
  // Non-Esc key events should be ignored.
  input::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_FALSE(GetExclusiveAccessManager()->HandleUserKeyEvent(event));
  ExpectMockControllerReceivedEscape(0);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       HandleKeyEvent_PointerLocked) {
  // Esc key pressed while pointer is locked should be handled.
  RequestToLockPointer(/*user_gesture=*/true,
                       /*last_unlocked_by_target=*/false);
  EXPECT_TRUE(SendEscapeToExclusiveAccessManager());
  ASSERT_FALSE(GetExclusiveAccessManager()
                   ->pointer_lock_controller()
                   ->IsPointerLocked());
  EXPECT_FALSE(SendEscapeToExclusiveAccessManager());
  ExpectMockControllerReceivedEscape(2);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       HandleKeyEvent_TabFullscreen) {
  // Esc key pressed while in fullscreen mode should be handled.
  EnterActiveTabFullscreen();
  EXPECT_TRUE(SendEscapeToExclusiveAccessManager());
  WaitForTabFullscreenExit();
  EXPECT_FALSE(SendEscapeToExclusiveAccessManager());
  ExpectMockControllerReceivedEscape(2);
}

// Ensure HandleKeyEvent_KeyboardLocked.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       HandleKeyEvent_KeyboardLocked) {
#if BUILDFLAG(IS_OZONE)
  // TODO: crbug.com/352244303 - For some reason the test fails on
  // linux_wayland_rel when kKeyboardAndPointerLockPrompt is disabled. Re-enable
  // the test when the feature is enabled by default.
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland has lock limitations";
  }
#endif
  // Esc key pressed while keyboard is locked without Esc key should be handled.
  EnterActiveTabFullscreen();
  RequestKeyboardLock(/*esc_key_locked=*/false);
  EXPECT_TRUE(SendEscapeToExclusiveAccessManager());
  WaitForTabFullscreenExit();
  ASSERT_FALSE(GetExclusiveAccessManager()
                   ->keyboard_lock_controller()
                   ->IsKeyboardLockActive());
  EXPECT_FALSE(SendEscapeToExclusiveAccessManager());
  ExpectMockControllerReceivedEscape(2);

  // Esc key pressed while keyboard is locked with Esc key should not be
  // handled.
  EnterActiveTabFullscreen();
  RequestKeyboardLock(/*esc_key_locked=*/true);
  EXPECT_FALSE(SendEscapeToExclusiveAccessManager());
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
  ExpectMockControllerReceivedEscape(0);
}

class ExclusiveAccessManagerPressAndHoldEscTest : public ExclusiveAccessTest {
 public:
  ExclusiveAccessManagerPressAndHoldEscTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPressAndHoldEscToExitBrowserFullscreen);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerPressAndHoldEscTest,
                       HoldTimerStartOnEscKeyPressWithModifiers) {
  // Don't start the timer on Esc key down with a non-stateful modifier.
  input::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::kRawKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  GetExclusiveAccessManager()->HandleUserKeyEvent(event);
  EXPECT_FALSE(IsEscKeyHoldTimerRunning());

  // Start the timer on Esc key down with a stateful modifier.
  event.SetModifiers(blink::WebInputEvent::kNumLockOn);
  GetExclusiveAccessManager()->HandleUserKeyEvent(event);
  EXPECT_TRUE(IsEscKeyHoldTimerRunning());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerPressAndHoldEscTest,
                       HandlePressAndHoldKeyEvent) {
  // Start the timer on key down event.
  SendEscapeToExclusiveAccessManager(/*is_key_down=*/true);
  EXPECT_TRUE(IsEscKeyHoldTimerRunning());

  // Multiple key down events won't affect the timer.
  SendEscapeToExclusiveAccessManager(/*is_key_down=*/true);
  EXPECT_TRUE(IsEscKeyHoldTimerRunning());

  // Stop the timer on key up event.
  EXPECT_CALL(*mock_controller(), HandleUserReleasedEscapeEarly());
  SendEscapeToExclusiveAccessManager(/*is_key_down=*/false);
  EXPECT_FALSE(IsEscKeyHoldTimerRunning());

  // Restart the timer and fastforward the clock to trigger the timer.
  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());

    SendEscapeToExclusiveAccessManager(/*is_key_down=*/true);
    EXPECT_TRUE(IsEscKeyHoldTimerRunning());

    EXPECT_CALL(*mock_controller(), HandleUserHeldEscape());
    task_runner->FastForwardBy(base::Seconds(2));
    EXPECT_FALSE(IsEscKeyHoldTimerRunning());
  }

  // Timer won't start on key up event.
  SendEscapeToExclusiveAccessManager(/*is_key_down=*/false);
  EXPECT_FALSE(IsEscKeyHoldTimerRunning());
}

// Disable the test on ChromeOS because the Exclusive Access Bubble isn't shown
// for browser fullscreen.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowExclusiveAccessBubble DISABLED_ShowExclusiveAccessBubble
#else
#define MAYBE_ShowExclusiveAccessBubble ShowExclusiveAccessBubble
#endif  // IS_CHROMEOS
IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerPressAndHoldEscTest,
                       MAYBE_ShowExclusiveAccessBubble) {
  // The bubble is shown after the browser enters fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(IsFullscreenForBrowser());
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());

  // Setting the bubble type to none will hide the bubble.
  GetExclusiveAccessManager()->context()->UpdateExclusiveAccessBubble(
      {.force_update = true}, base::NullCallback());
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());

  // The bubble is not shown after a short press on Esc key.
  SendEscapeToExclusiveAccessManager(/*is_key_down=*/true);
  SendEscapeToExclusiveAccessManager(/*is_key_down=*/false);
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());

  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());

    // The bubble is shown after pressing on the Esc key for 0.5 second.
    SendEscapeToExclusiveAccessManager(/*is_key_down=*/true);
    task_runner->FastForwardBy(base::Seconds(0.5));
    EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
  }
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       GetOriginForFullscreenBubble) {
  const GURL kTestUrl("https://example.com");
  const url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestUrl));
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->context()
                   ->IsExclusiveAccessBubbleDisplayed());

  // Enter fullscreen
  EnterActiveTabFullscreen();
  EXPECT_EQ(kTestOrigin,
            GetExclusiveAccessManager()->GetExclusiveAccessBubbleOrigin());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       GetOriginForPointerLockBubble) {
  const GURL kTestUrl("https://example.com");
  const url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestUrl));
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->context()
                   ->IsExclusiveAccessBubbleDisplayed());

  RequestToLockPointer(/*user_gesture=*/true,
                       /*last_unlocked_by_target=*/false);
  EXPECT_EQ(kTestOrigin,
            GetExclusiveAccessManager()->GetExclusiveAccessBubbleOrigin());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       GetOriginForPointerLockBubbleDuringPendingNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL committed_url = embedded_test_server()->GetURL("/title1.html");
  const GURL pending_url("https://pending.example/title2.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), committed_url));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::TestNavigationManager navigation_manager(web_contents, pending_url);
  web_contents->GetController().LoadURL(pending_url, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED,
                                        std::string());
  ASSERT_TRUE(navigation_manager.WaitForRequestStart());
  ASSERT_FALSE(navigation_manager.was_committed());
  ASSERT_EQ(pending_url, web_contents->GetVisibleURL());
  ASSERT_EQ(committed_url, web_contents->GetLastCommittedURL());

  RequestToLockPointer(/*user_gesture=*/false,
                       /*last_unlocked_by_target=*/true);
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
  EXPECT_EQ(url::Origin::Create(committed_url),
            GetExclusiveAccessManager()->GetExclusiveAccessBubbleOrigin());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       GetOriginForPointerLockBubbleInAboutBlankPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL opener_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));

  content::WebContents* opener =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(content::ExecJs(opener, "window.open('about:blank', '_blank');"));
  content::WebContents* popup = popup_observer.GetWebContents();
  ASSERT_TRUE(popup);
  EXPECT_TRUE(content::WaitForLoadStop(popup));
  ASSERT_EQ(popup, browser()->GetTabStripModel()->GetActiveWebContents());
  ASSERT_EQ(GURL(url::kAboutBlankURL), popup->GetLastCommittedURL());

  RequestToLockPointer(/*user_gesture=*/true,
                       /*last_unlocked_by_target=*/false);
  EXPECT_EQ(url::Origin::Create(opener_url),
            GetExclusiveAccessManager()->GetExclusiveAccessBubbleOrigin());
}
