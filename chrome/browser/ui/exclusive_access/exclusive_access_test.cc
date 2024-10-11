// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "exclusive_access_controller_base.h"
#include "exclusive_access_manager.h"
#include "exclusive_access_test.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

const char ExclusiveAccessTest::kFullscreenKeyboardLockHTML[] =
    "/fullscreen_keyboardlock/fullscreen_keyboardlock.html";

const char ExclusiveAccessTest::kFullscreenPointerLockHTML[] =
    "/fullscreen_pointerlock/fullscreen_pointerlock.html";

MockExclusiveAccessController::MockExclusiveAccessController(
    ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager) {}

MockExclusiveAccessController::~MockExclusiveAccessController() = default;

bool MockExclusiveAccessController::HandleUserPressedEscape() {
  escape_pressed_count_++;
  return false;
}

ExclusiveAccessTest::ExclusiveAccessTest() {
  // It is important to disable system keyboard lock as low-level test utilities
  // may install a keyboard hook to listen for keyboard events and having an
  // active system hook may cause issues with that mechanism.
  scoped_feature_list_.InitWithFeatures({}, {features::kSystemKeyboardLock});
}

ExclusiveAccessTest::~ExclusiveAccessTest() = default;

void ExclusiveAccessTest::SetUpOnMainThread() {
  GetExclusiveAccessManager()
      ->pointer_lock_controller()
      ->bubble_hide_callback_for_test_ = base::BindRepeating(
      &ExclusiveAccessTest::OnBubbleHidden, weak_ptr_factory_.GetWeakPtr(),
      &pointer_lock_bubble_hide_reason_recorder_);
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->bubble_hide_callback_for_test_ = base::BindRepeating(
      &ExclusiveAccessTest::OnBubbleHidden, weak_ptr_factory_.GetWeakPtr(),
      &keyboard_lock_bubble_hide_reason_recorder_);

  mock_controller_ = std::make_unique<MockExclusiveAccessController>(
      GetExclusiveAccessManager());
  GetExclusiveAccessManager()->exclusive_access_controllers_for_test().insert(
      mock_controller_.get());
}

void ExclusiveAccessTest::TearDownOnMainThread() {
  GetExclusiveAccessManager()
      ->pointer_lock_controller()
      ->bubble_hide_callback_for_test_ =
      base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>();
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->bubble_hide_callback_for_test_ =
      base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>();

  GetExclusiveAccessManager()->exclusive_access_controllers_for_test().erase(
      mock_controller_.get());
  mock_controller_.reset();
}

// static
bool ExclusiveAccessTest::IsBubbleDownloadNotification(
    ExclusiveAccessBubble* bubble) {
  return bubble->params_.has_download;
}

bool ExclusiveAccessTest::RequestKeyboardLock(bool esc_key_locked) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // If the caller defines |esc_key_locked| as true then we create a set of
  // locked keys which includes the escape key, this will require the user/test
  // to press and hold escape to exit fullscreen.  If |esc_key_locked| is false,
  // then we create a set of keys that does not include escape (we arbitrarily
  // chose the 'a' key) which means the user/test can just press escape to exit
  // fullscreen.
  std::optional<base::flat_set<ui::DomCode>> codes;
  if (esc_key_locked)
    codes = base::flat_set<ui::DomCode>({ui::DomCode::ESCAPE});
  else
    codes = base::flat_set<ui::DomCode>({ui::DomCode::US_A});

  bool success = false;
  bool callback_called = false;
  base::OnceCallback<void(blink::mojom::KeyboardLockRequestResult)> callback =
      base::BindOnce(
          [](bool* success, bool* callback_called,
             blink::mojom::KeyboardLockRequestResult result) {
            *success =
                result == blink::mojom::KeyboardLockRequestResult::kSuccess;
            *callback_called = true;
          },
          &success, &callback_called);
  content::RequestKeyboardLock(tab, std::move(codes), std::move(callback));
  // We currently assume that content::RequestKeyboardLock() calls the callback
  // synchronously. We'd need to change the test code here if the assumption no
  // longer holds. However, we cannot use base::RunLoop as-is, since this code
  // may be used with base::TestMockTimeTaskRunner::ScopedContext, which cannot
  // be used together with base::RunLoop.
  CHECK(callback_called);
  return success;
}

void ExclusiveAccessTest::RequestToLockPointer(bool user_gesture,
                                               bool last_unlocked_by_target) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  PointerLockController* pointer_lock_controller =
      GetExclusiveAccessManager()->pointer_lock_controller();
  pointer_lock_controller->fake_pointer_lock_for_test_ = true;
  browser()->RequestPointerLock(tab, user_gesture, last_unlocked_by_target);
  pointer_lock_controller->fake_pointer_lock_for_test_ = false;
}

void ExclusiveAccessTest::SetWebContentsGrantedSilentPointerLockPermission() {
  GetExclusiveAccessManager()
      ->pointer_lock_controller()
      ->web_contents_granted_silent_pointer_lock_permission_ =
      browser()->tab_strip_model()->GetActiveWebContents();
}

FullscreenController* ExclusiveAccessTest::GetFullscreenController() {
  return GetExclusiveAccessManager()->fullscreen_controller();
}

ExclusiveAccessManager* ExclusiveAccessTest::GetExclusiveAccessManager() {
  return browser()->exclusive_access_manager();
}

void ExclusiveAccessTest::CancelKeyboardLock() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::CancelKeyboardLock(tab);
}

void ExclusiveAccessTest::LostPointerLock() {
  browser()->LostPointerLock();
}

bool ExclusiveAccessTest::SendEscapeToExclusiveAccessManager(bool is_key_down) {
  input::NativeWebKeyboardEvent event(
      is_key_down ? blink::WebInputEvent::Type::kRawKeyDown
                  : blink::WebInputEvent::Type::kKeyUp,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  return GetExclusiveAccessManager()->HandleUserKeyEvent(event);
}

bool ExclusiveAccessTest::IsFullscreenForBrowser() {
  return GetFullscreenController()->IsFullscreenForBrowser();
}

bool ExclusiveAccessTest::IsWindowFullscreenForTabOrPending() {
  return GetFullscreenController()->IsWindowFullscreenForTabOrPending();
}

ExclusiveAccessBubbleType ExclusiveAccessTest::GetExclusiveAccessBubbleType() {
  return GetExclusiveAccessManager()->GetExclusiveAccessExitBubbleType();
}

bool ExclusiveAccessTest::IsExclusiveAccessBubbleDisplayed() {
  return GetExclusiveAccessManager()
      ->context()
      ->IsExclusiveAccessBubbleDisplayed();
}

void ExclusiveAccessTest::GoBack() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
}

void ExclusiveAccessTest::Reload() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
}

void ExclusiveAccessTest::EnterActiveTabFullscreen() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
  browser()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  waiter.Wait();
}

void ExclusiveAccessTest::WaitForTabFullscreenExit() {
  ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = false});
  waiter.Wait();
}

void ExclusiveAccessTest::WaitAndVerifyFullscreenState(bool browser_fullscreen,
                                                       bool tab_fullscreen) {
  ui_test_utils::FullscreenWaiter waiter(
      browser(), {.browser_fullscreen = browser_fullscreen,
                  .tab_fullscreen = tab_fullscreen});
  waiter.Wait();
}

void ExclusiveAccessTest::EnterExtensionInitiatedFullscreen() {
  ui_test_utils::FullscreenWaiter waiter(browser(),
                                         {.browser_fullscreen = true});
  static const char kExtensionId[] = "extension-id";
  browser()->ToggleFullscreenModeWithExtension(
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionId));
  waiter.Wait();
}

bool ExclusiveAccessTest::IsEscKeyHoldTimerRunning() {
  return GetExclusiveAccessManager()->esc_key_hold_timer_for_test().IsRunning();
}

void ExclusiveAccessTest::SetEscRepeatWindowLength(
    base::TimeDelta esc_repeat_window) {
  GetExclusiveAccessManager()->keyboard_lock_controller()->esc_repeat_window_ =
      esc_repeat_window;
}

void ExclusiveAccessTest::SetEscRepeatThresholdReachedCallback(
    base::OnceClosure callback) {
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->esc_repeat_triggered_for_test_ = std::move(callback);
}

void ExclusiveAccessTest::SetEscRepeatTestTickClock(
    const base::TickClock* tick_clock_for_test) {
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->esc_repeat_tick_clock_ = tick_clock_for_test;
}

void ExclusiveAccessTest::OnBubbleHidden(
    std::vector<ExclusiveAccessBubbleHideReason>* reason_recorder,
    ExclusiveAccessBubbleHideReason reason) {
  reason_recorder->push_back(reason);
}

void ExclusiveAccessTest::SetUserEscapeTimestampForTest(
    const base::TimeTicks timestamp) {
  GetExclusiveAccessManager()
      ->pointer_lock_controller()
      ->last_user_escape_time_ = timestamp;
}

void ExclusiveAccessTest::ExpectMockControllerReceivedEscape(int count) {
  EXPECT_EQ(count, mock_controller()->escape_pressed_count());
  mock_controller()->reset_escape_pressed_count();
}
