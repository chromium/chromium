// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/keyboard_codes.h"

using ExclusiveAccessManagerTest = ExclusiveAccessTest;

IN_PROC_BROWSER_TEST_F(ExclusiveAccessManagerTest, HandleKeyEvent_NonEscKey) {
  // Non-Esc key events should be ignored.
  content::NativeWebKeyboardEvent event(
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
