// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;
using MouseLockControllerTest = ExclusiveAccessTest;

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest, MouseLockOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  ASSERT_TRUE(AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED));
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       MouseLockBubbleHideCallbackReject) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(false, false);

  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       MouseLockBubbleHideCallbackSilentLock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(false, true);

  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kNotShown,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       MouseLockBubbleHideCallbackUnlock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(true, false);
  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());

  LostMouseLock();
  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       MouseLockBubbleHideCallbackLockThenFullscreen) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(true, false);
  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());

  EnterActiveTabFullscreen();
  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       MouseLockBubbleHideCallbackTimeout) {
  SetWebContentsGrantedSilentMouseLockPermission();
  // TODO(crbug.com/708584): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(true, false);
  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());

  EXPECT_TRUE(task_runner->HasPendingTask());
  // Must fast forward at least |ExclusiveAccessBubble::kInitialDelayMs|.
  task_runner->FastForwardBy(base::Milliseconds(InitialBubbleDelayMs() + 20));
  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kTimeout,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest, FastMouseLockUnlockRelock) {
  // TODO(crbug.com/708584): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  RequestToLockMouse(true, false);
  // Shorter than |ExclusiveAccessBubble::kInitialDelayMs|.
  task_runner->FastForwardBy(base::Milliseconds(InitialBubbleDelayMs() / 2));
  LostMouseLock();
  RequestToLockMouse(true, true);

  EXPECT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->mouse_lock_controller()
                   ->IsMouseLockedSilently());
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest, SlowMouseLockUnlockRelock) {
  // TODO(crbug.com/708584): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  RequestToLockMouse(true, false);
  // Longer than |ExclusiveAccessBubble::kInitialDelayMs|.
  task_runner->FastForwardBy(base::Milliseconds(InitialBubbleDelayMs() + 20));
  LostMouseLock();
  RequestToLockMouse(true, true);

  EXPECT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->mouse_lock_controller()
                  ->IsMouseLockedSilently());
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       RepeatedMouseLockAfterEscapeKey) {
  RequestToLockMouse(true, false);
  EXPECT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  SendEscapeToExclusiveAccessManager();
  EXPECT_FALSE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());

  // A lock request is ignored right after user-escape.
  RequestToLockMouse(true, false);
  EXPECT_FALSE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());

  // A lock request is ignored if we mimic the user-escape happened 1sec ago.
  SetUserEscapeTimestampForTest(base::TimeTicks::Now() - base::Seconds(1));
  RequestToLockMouse(true, false);
  EXPECT_FALSE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());

  // A lock request goes through if we mimic the user-escape happened 5secs ago.
  SetUserEscapeTimestampForTest(base::TimeTicks::Now() - base::Seconds(5));
  RequestToLockMouse(true, false);
  EXPECT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest, MouseLockAfterKeyboardLock) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  RequestToLockMouse(/*user_gesture=*/true, /*last_unlocked_by_target=*/false);
  ASSERT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(MouseLockControllerTest,
                       MouseLockAfterKeyboardLockWithEscLocked) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  RequestToLockMouse(/*user_gesture=*/true, /*last_unlocked_by_target=*/false);
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
}
