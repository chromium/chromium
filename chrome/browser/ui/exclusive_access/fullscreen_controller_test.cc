// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

FullscreenNotificationObserver::FullscreenNotificationObserver(
    Browser* browser) {
  observer_.Add(browser->exclusive_access_manager()->fullscreen_controller());
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

const char FullscreenControllerTest::kFullscreenKeyboardLockHTML[] =
    "/fullscreen_keyboardlock/fullscreen_keyboardlock.html";

const char FullscreenControllerTest::kFullscreenMouseLockHTML[] =
    "/fullscreen_mouselock/fullscreen_mouselock.html";

FullscreenControllerTest::FullscreenControllerTest() {
  // It is important to disable system keyboard lock as low-level test utilities
  // may install a keyboard hook to listen for keyboard events and having an
  // active system hook may cause issues with that mechanism.
  scoped_feature_list_.InitWithFeatures({}, {features::kSystemKeyboardLock});
}

FullscreenControllerTest::~FullscreenControllerTest() = default;

void FullscreenControllerTest::SetUpOnMainThread() {
  GetExclusiveAccessManager()
      ->mouse_lock_controller()
      ->set_bubble_hide_callback_for_test_(
          base::BindRepeating(&FullscreenControllerTest::OnBubbleHidden,
                              weak_ptr_factory_.GetWeakPtr(),
                              &mouse_lock_bubble_hide_reason_recorder_));
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->bubble_hide_callback_for_test_ = base::BindRepeating(
      &FullscreenControllerTest::OnBubbleHidden, weak_ptr_factory_.GetWeakPtr(),
      &keyboard_lock_bubble_hide_reason_recorder_);
}

void FullscreenControllerTest::TearDownOnMainThread() {
  GetExclusiveAccessManager()
      ->mouse_lock_controller()
      ->set_bubble_hide_callback_for_test_(
          base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>());
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->bubble_hide_callback_for_test_ =
      base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>();
}

bool FullscreenControllerTest::RequestKeyboardLock(bool esc_key_locked) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // If the caller defines |esc_key_locked| as true then we create a set of
  // locked keys which includes the escape key, this will require the user/test
  // to press and hold escape to exit fullscreen.  If |esc_key_locked| is false,
  // then we create a set of keys that does not include escape (we arbitrarily
  // chose the 'a' key) which means the user/test can just press escape to exit
  // fullscreen.
  base::Optional<base::flat_set<ui::DomCode>> codes;
  if (esc_key_locked)
    codes = base::flat_set<ui::DomCode>({ui::DomCode::ESCAPE});
  else
    codes = base::flat_set<ui::DomCode>({ui::DomCode::US_A});

  return content::RequestKeyboardLock(tab, std::move(codes));
}

void FullscreenControllerTest::RequestToLockMouse(
    bool user_gesture,
    bool last_unlocked_by_target) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  MouseLockController* mouse_lock_controller =
      GetExclusiveAccessManager()->mouse_lock_controller();
  mouse_lock_controller->set_fake_mouse_lock_for_test(true);
  browser()->RequestToLockMouse(tab, user_gesture,
      last_unlocked_by_target);
  mouse_lock_controller->set_fake_mouse_lock_for_test(false);
}

void FullscreenControllerTest::
    SetWebContentsGrantedSilentMouseLockPermission() {
  GetExclusiveAccessManager()
      ->mouse_lock_controller()
      ->set_web_contents_granted_silent_mouse_lock_permission_for_test(
          browser()->tab_strip_model()->GetActiveWebContents());
}

FullscreenController* FullscreenControllerTest::GetFullscreenController() {
  return GetExclusiveAccessManager()->fullscreen_controller();
}

ExclusiveAccessManager* FullscreenControllerTest::GetExclusiveAccessManager() {
  return browser()->exclusive_access_manager();
}

void FullscreenControllerTest::CancelKeyboardLock() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::CancelKeyboardLock(tab);
}

void FullscreenControllerTest::LostMouseLock() {
  browser()->LostMouseLock();
}

bool FullscreenControllerTest::SendEscapeToFullscreenController() {
  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  return GetExclusiveAccessManager()->HandleUserKeyEvent(event);
}

bool FullscreenControllerTest::IsFullscreenForBrowser() {
  return GetFullscreenController()->IsFullscreenForBrowser();
}

bool FullscreenControllerTest::IsWindowFullscreenForTabOrPending() {
  return GetFullscreenController()->IsWindowFullscreenForTabOrPending();
}

ExclusiveAccessBubbleType
FullscreenControllerTest::GetExclusiveAccessBubbleType() {
  return GetExclusiveAccessManager()->GetExclusiveAccessExitBubbleType();
}

bool FullscreenControllerTest::IsFullscreenBubbleDisplayed() {
  return GetExclusiveAccessBubbleType() != EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void FullscreenControllerTest::GoBack() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
}

void FullscreenControllerTest::Reload() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
}

void FullscreenControllerTest::SetPrivilegedFullscreen(bool is_privileged) {
  GetFullscreenController()->SetPrivilegedFullscreenForTesting(is_privileged);
}

void FullscreenControllerTest::EnterActiveTabFullscreen() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  FullscreenNotificationObserver fullscreen_observer(browser());
  browser()->EnterFullscreenModeForTab(tab, GURL(),
                                       blink::mojom::FullscreenOptions());
  fullscreen_observer.Wait();
}

void FullscreenControllerTest::ToggleBrowserFullscreen() {
  FullscreenNotificationObserver fullscreen_observer(browser());
  chrome::ToggleFullscreenMode(browser());
  fullscreen_observer.Wait();
}

void FullscreenControllerTest::EnterExtensionInitiatedFullscreen() {
  FullscreenNotificationObserver fullscreen_observer(browser());
  browser()->ToggleFullscreenModeWithExtension(GURL("faux_extension"));
  fullscreen_observer.Wait();
}

void FullscreenControllerTest::SetEscRepeatWindowLength(
    base::TimeDelta esc_repeat_window) {
  GetExclusiveAccessManager()->keyboard_lock_controller()->esc_repeat_window_ =
      esc_repeat_window;
}

void FullscreenControllerTest::SetEscRepeatThresholdReachedCallback(
    base::OnceClosure callback) {
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->esc_repeat_triggered_for_test_ = std::move(callback);
}

void FullscreenControllerTest::SetEscRepeatTestTickClock(
    const base::TickClock* tick_clock_for_test) {
  GetExclusiveAccessManager()
      ->keyboard_lock_controller()
      ->esc_repeat_tick_clock_ = tick_clock_for_test;
}

void FullscreenControllerTest::OnBubbleHidden(
    std::vector<ExclusiveAccessBubbleHideReason>* reason_recorder,
    ExclusiveAccessBubbleHideReason reason) {
  reason_recorder->push_back(reason);
}

int FullscreenControllerTest::InitialBubbleDelayMs() const {
  return ExclusiveAccessBubble::kInitialDelayMs;
}
