// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

FullscreenNotificationObserver::FullscreenNotificationObserver(
    Browser* browser) {
  observation_.Observe(
      browser->exclusive_access_manager()->fullscreen_controller());
}

FullscreenNotificationObserver::~FullscreenNotificationObserver() = default;

void FullscreenNotificationObserver::OnFullscreenStateChanged() {
  observed_change_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

void FullscreenNotificationObserver::Wait() {
  if (observed_change_)
    return;

  run_loop_.Run();
}

const char ExclusiveAccessTest::kFullscreenKeyboardLockHTML[] =
    "/fullscreen_keyboardlock/fullscreen_keyboardlock.html";

const char ExclusiveAccessTest::kFullscreenMouseLockHTML[] =
    "/fullscreen_mouselock/fullscreen_mouselock.html";

ExclusiveAccessTest::ExclusiveAccessTest() {
  // It is important to disable system keyboard lock as low-level test utilities
  // may install a keyboard hook to listen for keyboard events and having an
  // active system hook may cause issues with that mechanism.
  scoped_feature_list_.InitWithFeatures({}, {features::kSystemKeyboardLock});
}

ExclusiveAccessTest::~ExclusiveAccessTest() = default;

void ExclusiveAccessTest::SetUpOnMainThread() {
  GetExclusiveAccessManager()
      ->mouse_lock_controller()
      ->bubble_hide_callback_for_test_ = base::BindRepeating(
      &ExclusiveAccessTest::OnBubbleHidden, weak_ptr_factory_.GetWeakPtr(),
      &mouse_lock_bubble_hide_reason_recorder_);
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->bubble_hide_callback_for_test_ = base::BindRepeating(
      &ExclusiveAccessTest::OnBubbleHidden, weak_ptr_factory_.GetWeakPtr(),
      &keyboard_lock_bubble_hide_reason_recorder_);
}

void ExclusiveAccessTest::TearDownOnMainThread() {
  GetExclusiveAccessManager()
      ->mouse_lock_controller()
      ->bubble_hide_callback_for_test_ =
      base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>();
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->bubble_hide_callback_for_test_ =
      base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>();
}

// static
bool ExclusiveAccessTest::IsBubbleDownloadNotification(
    ExclusiveAccessBubble* bubble) {
  return bubble->notify_download_;
}

bool ExclusiveAccessTest::RequestKeyboardLock(bool esc_key_locked) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // If the caller defines |esc_key_locked| as true then we create a set of
  // locked keys which includes the escape key, this will require the user/test
  // to press and hold escape to exit fullscreen.  If |esc_key_locked| is false,
  // then we create a set of keys that does not include escape (we arbitrarily
  // chose the 'a' key) which means the user/test can just press escape to exit
  // fullscreen.
  absl::optional<base::flat_set<ui::DomCode>> codes;
  if (esc_key_locked)
    codes = base::flat_set<ui::DomCode>({ui::DomCode::ESCAPE});
  else
    codes = base::flat_set<ui::DomCode>({ui::DomCode::US_A});

  return content::RequestKeyboardLock(tab, std::move(codes));
}

void ExclusiveAccessTest::RequestToLockMouse(bool user_gesture,
                                             bool last_unlocked_by_target) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  MouseLockController* mouse_lock_controller =
      GetExclusiveAccessManager()->mouse_lock_controller();
  mouse_lock_controller->fake_mouse_lock_for_test_ = true;
  browser()->RequestToLockMouse(tab, user_gesture, last_unlocked_by_target);
  mouse_lock_controller->fake_mouse_lock_for_test_ = false;
}

void ExclusiveAccessTest::SetWebContentsGrantedSilentMouseLockPermission() {
  GetExclusiveAccessManager()
      ->mouse_lock_controller()
      ->web_contents_granted_silent_mouse_lock_permission_ =
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

void ExclusiveAccessTest::LostMouseLock() {
  browser()->LostMouseLock();
}

bool ExclusiveAccessTest::SendEscapeToExclusiveAccessManager() {
  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
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
  FullscreenNotificationObserver fullscreen_observer(browser());
  browser()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  fullscreen_observer.Wait();
}

void ExclusiveAccessTest::ToggleBrowserFullscreen() {
  FullscreenNotificationObserver fullscreen_observer(browser());
  chrome::ToggleFullscreenMode(browser());
  fullscreen_observer.Wait();
}

void ExclusiveAccessTest::EnterExtensionInitiatedFullscreen() {
  FullscreenNotificationObserver fullscreen_observer(browser());
  static const char kExtensionId[] = "extension-id";
  browser()->ToggleFullscreenModeWithExtension(
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionId));
  fullscreen_observer.Wait();
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
  GetExclusiveAccessManager()->mouse_lock_controller()->last_user_escape_time_ =
      timestamp;
}

int ExclusiveAccessTest::InitialBubbleDelayMs() const {
  return ExclusiveAccessBubble::kInitialDelayMs;
}
