// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"

#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/test/browser_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

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

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest,
                       HandleKeyEvent_KeyboardLocked) {
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
