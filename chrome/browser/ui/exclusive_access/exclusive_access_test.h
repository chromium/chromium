// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_TEST_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_TEST_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

class Browser;

namespace base {
class TickClock;
}  // namespace base

// Observer for fullscreen state change notifications.
class FullscreenNotificationObserver : public FullscreenObserver {
 public:
  explicit FullscreenNotificationObserver(Browser* browser);

  FullscreenNotificationObserver(const FullscreenNotificationObserver&) =
      delete;
  FullscreenNotificationObserver& operator=(
      const FullscreenNotificationObserver&) = delete;

  ~FullscreenNotificationObserver() override;

  // Runs a loop until a fullscreen change is seen (unless one has already been
  // observed, in which case it returns immediately).
  void Wait();

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

 protected:
  bool observed_change_ = false;
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      observation_{this};
  base::RunLoop run_loop_;
};

// Test fixture with convenience functions for fullscreen, keyboard lock, and
// mouse lock.
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
  void RequestToLockMouse(bool user_gesture, bool last_unlocked_by_target);
  void SetWebContentsGrantedSilentMouseLockPermission();
  void CancelKeyboardLock();
  void LostMouseLock();
  bool SendEscapeToExclusiveAccessManager();
  bool IsFullscreenForBrowser();
  bool IsWindowFullscreenForTabOrPending();
  ExclusiveAccessBubbleType GetExclusiveAccessBubbleType();
  bool IsExclusiveAccessBubbleDisplayed();
  void GoBack();
  void Reload();
  void EnterActiveTabFullscreen();
  void ToggleBrowserFullscreen();
  void EnterExtensionInitiatedFullscreen();

  static const char kFullscreenKeyboardLockHTML[];
  static const char kFullscreenMouseLockHTML[];
  FullscreenController* GetFullscreenController();
  ExclusiveAccessManager* GetExclusiveAccessManager();

  void OnBubbleHidden(
      std::vector<ExclusiveAccessBubbleHideReason>* reason_recorder,
      ExclusiveAccessBubbleHideReason);

  void SetEscRepeatWindowLength(base::TimeDelta esc_repeat_window);

  void SetEscRepeatThresholdReachedCallback(base::OnceClosure callback);

  void SetEscRepeatTestTickClock(const base::TickClock* tick_clock_for_test);

  void SetUserEscapeTimestampForTest(const base::TimeTicks timestamp);

  int InitialBubbleDelayMs() const;

  std::vector<ExclusiveAccessBubbleHideReason>
      mouse_lock_bubble_hide_reason_recorder_;

  std::vector<ExclusiveAccessBubbleHideReason>
      keyboard_lock_bubble_hide_reason_recorder_;

 private:
  void ToggleTabFullscreen_Internal(bool enter_fullscreen,
                                    bool retry_until_success);

#if BUILDFLAG(IS_MAC)
  // On Mac, entering into the system fullscreen mode can tickle crashes in
  // the WindowServer (c.f. https://crbug.com/828031), so provide a fake for
  // testing.
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen_window_;
#endif

  base::test::ScopedFeatureList scoped_feature_list_;

  base::WeakPtrFactory<ExclusiveAccessTest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_TEST_H_
