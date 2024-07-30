// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "components/input/native_web_keyboard_event.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;
using PointerLockControllerTest = ExclusiveAccessTest;

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest, PointerLockOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  ASSERT_TRUE(AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED));
  RequestToLockPointer(true, false);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockBubbleHideCallbackReject) {
  SetWebContentsGrantedSilentPointerLockPermission();
  pointer_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockPointer(false, false);

  EXPECT_EQ(0ul, pointer_lock_bubble_hide_reason_recorder_.size());
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockBubbleHideCallbackSilentLock) {
  SetWebContentsGrantedSilentPointerLockPermission();
  pointer_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockPointer(false, true);

  EXPECT_EQ(1ul, pointer_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kNotShown,
            pointer_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockBubbleHideCallbackUnlock) {
  SetWebContentsGrantedSilentPointerLockPermission();
  pointer_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockPointer(true, false);
  EXPECT_EQ(0ul, pointer_lock_bubble_hide_reason_recorder_.size());

  LostPointerLock();
  EXPECT_EQ(1ul, pointer_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            pointer_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockBubbleHideCallbackLockThenFullscreen) {
  SetWebContentsGrantedSilentPointerLockPermission();
  pointer_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockPointer(true, false);
  EXPECT_EQ(0ul, pointer_lock_bubble_hide_reason_recorder_.size());

  EnterActiveTabFullscreen();
  EXPECT_EQ(1ul, pointer_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            pointer_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockBubbleHideCallbackTimeout) {
  SetWebContentsGrantedSilentPointerLockPermission();
  // TODO(crbug.com/40514143): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  pointer_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockPointer(true, false);
  EXPECT_EQ(0ul, pointer_lock_bubble_hide_reason_recorder_.size());

  EXPECT_TRUE(task_runner->HasPendingTask());
  // Must fast forward at least `ExclusiveAccessBubble::kShowTime`.
  task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime * 2);
  EXPECT_EQ(1ul, pointer_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kTimeout,
            pointer_lock_bubble_hide_reason_recorder_[0]);
}

// TODO(crbug.com/348396470): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_FastPointerLockUnlockRelock DISABLED_FastPointerLockUnlockRelock
#else
#define MAYBE_FastPointerLockUnlockRelock FastPointerLockUnlockRelock
#endif
IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       MAYBE_FastPointerLockUnlockRelock) {
  // TODO(crbug.com/40514143): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  RequestToLockPointer(true, false);
  // Shorter than `ExclusiveAccessBubble::kShowTime`.
  task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime / 2);
  LostPointerLock();
  RequestToLockPointer(true, true);

  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->pointer_lock_controller()
                   ->IsPointerLockedSilently());
}

// TODO(crbug.com/348396470): Fix test flakiness.
#if BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_SlowPointerLockUnlockRelock DISABLED_SlowPointerLockUnlockRelock
#else
#define MAYBE_SlowPointerLockUnlockRelock SlowPointerLockUnlockRelock
#endif
IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       MAYBE_SlowPointerLockUnlockRelock) {
  // TODO(crbug.com/40514143): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  RequestToLockPointer(true, false);
  // Longer than `ExclusiveAccessBubble::kShowTime`.
  task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime * 2);
  LostPointerLock();
  RequestToLockPointer(true, true);

  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLockedSilently());
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       RepeatedPointerLockAfterEscapeKey) {
  RequestToLockPointer(true, false);
  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
  SendEscapeToExclusiveAccessManager();
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->pointer_lock_controller()
                   ->IsPointerLocked());

  // A lock request is ignored right after user-escape.
  RequestToLockPointer(true, false);
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->pointer_lock_controller()
                   ->IsPointerLocked());

  // A lock request is ignored if we mimic the user-escape happened 1sec ago.
  SetUserEscapeTimestampForTest(base::TimeTicks::Now() - base::Seconds(1));
  RequestToLockPointer(true, false);
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->pointer_lock_controller()
                   ->IsPointerLocked());

  // A lock request goes through if we mimic the user-escape happened 5secs ago.
  SetUserEscapeTimestampForTest(base::TimeTicks::Now() - base::Seconds(5));
  RequestToLockPointer(true, false);
  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockAfterKeyboardLock) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  RequestToLockPointer(/*user_gesture=*/true,
                       /*last_unlocked_by_target=*/false);
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_EQ(
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION,
      GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(PointerLockControllerTest,
                       PointerLockAfterKeyboardLockWithEscLocked) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  RequestToLockPointer(/*user_gesture=*/true,
                       /*last_unlocked_by_target=*/false);
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->pointer_lock_controller()
                  ->IsPointerLocked());
}
