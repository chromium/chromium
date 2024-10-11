// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_TEST_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_TEST_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "exclusive_access_controller_base.h"
#include "exclusive_access_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace base {
class TickClock;
}  // namespace base

class FullscreenController;

class MockExclusiveAccessController : public ExclusiveAccessControllerBase {
 public:
  explicit MockExclusiveAccessController(ExclusiveAccessManager* manager);
  ~MockExclusiveAccessController() override;

  // ExclusiveAccessControllerBase:
  bool HandleUserPressedEscape() override;

  MOCK_METHOD(void, HandleUserHeldEscape, (), (override));
  MOCK_METHOD(void, HandleUserReleasedEscapeEarly, (), (override));
  MOCK_METHOD(bool, RequiresPressAndHoldEscToExit, (), (const, override));
  MOCK_METHOD(void, ExitExclusiveAccessToPreviousState, (), (override));
  MOCK_METHOD(void, ExitExclusiveAccessIfNecessary, (), (override));
  MOCK_METHOD(void, NotifyTabExclusiveAccessLost, (), (override));

  int escape_pressed_count() { return escape_pressed_count_; }
  void reset_escape_pressed_count() { escape_pressed_count_ = 0; }

 private:
  int escape_pressed_count_ = 0;
};

// Test fixture with convenience functions for fullscreen, keyboard lock, and
// pointer lock.
class ExclusiveAccessTest : public InProcessBrowserTest {
 public:
  ExclusiveAccessTest(const ExclusiveAccessTest&) = delete;
  ExclusiveAccessTest& operator=(const ExclusiveAccessTest&) = delete;

  static bool IsBubbleDownloadNotification(ExclusiveAccessBubble* bubble);

 protected:
  ExclusiveAccessTest();
  ~ExclusiveAccessTest() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  bool RequestKeyboardLock(bool esc_key_locked);
  void RequestToLockPointer(bool user_gesture, bool last_unlocked_by_target);
  void SetWebContentsGrantedSilentPointerLockPermission();
  void CancelKeyboardLock();
  void LostPointerLock();
  bool SendEscapeToExclusiveAccessManager(bool is_key_down = true);
  bool IsFullscreenForBrowser();
  bool IsWindowFullscreenForTabOrPending();
  ExclusiveAccessBubbleType GetExclusiveAccessBubbleType();
  bool IsExclusiveAccessBubbleDisplayed();
  void GoBack();
  void Reload();
  void EnterActiveTabFullscreen();
  void WaitForTabFullscreenExit();
  void WaitAndVerifyFullscreenState(bool browser_fullscreen,
                                    bool tab_fullscreen);
  void EnterExtensionInitiatedFullscreen();
  bool IsEscKeyHoldTimerRunning();

  static const char kFullscreenKeyboardLockHTML[];
  static const char kFullscreenPointerLockHTML[];
  FullscreenController* GetFullscreenController();
  ExclusiveAccessManager* GetExclusiveAccessManager();

  void OnBubbleHidden(
      std::vector<ExclusiveAccessBubbleHideReason>* reason_recorder,
      ExclusiveAccessBubbleHideReason);

  void SetEscRepeatWindowLength(base::TimeDelta esc_repeat_window);

  void SetEscRepeatThresholdReachedCallback(base::OnceClosure callback);

  void SetEscRepeatTestTickClock(const base::TickClock* tick_clock_for_test);

  void SetUserEscapeTimestampForTest(const base::TimeTicks timestamp);

  void ExpectMockControllerReceivedEscape(int count);

  MockExclusiveAccessController* mock_controller() {
    return mock_controller_.get();
  }

  std::vector<ExclusiveAccessBubbleHideReason>
      pointer_lock_bubble_hide_reason_recorder_;

  std::vector<ExclusiveAccessBubbleHideReason>
      keyboard_lock_bubble_hide_reason_recorder_;

 private:
#if BUILDFLAG(IS_MAC)
  // On Mac, entering into the system fullscreen mode can tickle crashes in
  // the WindowServer (c.f. https://crbug.com/828031), so provide a fake for
  // testing.
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen_window_;
#endif
  std::unique_ptr<MockExclusiveAccessController> mock_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::WeakPtrFactory<ExclusiveAccessTest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_TEST_H_
