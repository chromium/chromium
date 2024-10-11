// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_permission_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::TimeTicks;
using content::WebContents;

namespace {

// Amount of time the user must hold ESC to exit full screen.
constexpr base::TimeDelta kHoldEscapeTime = base::Milliseconds(1500);

// Amount of time to look for ESC key presses to reshow the exit instructions.
constexpr base::TimeDelta kDefaultEscRepeatWindow = base::Seconds(1);

// Number of times ESC must be pressed within |kDefaultEscRepeatWindow| to
// trigger the exit instructions to be shown again.
constexpr int kEscRepeatCountToTriggerUiReshow = 3;

// Check whether `event` is a kRawKeyDown type and doesn't have non-stateful
// modifiers (i.e. shift, ctrl etc.).
bool IsUnmodifiedEscKeyDownEvent(const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() != input::NativeWebKeyboardEvent::Type::kRawKeyDown) {
    return false;
  }
  if (event.GetModifiers() & blink::WebInputEvent::kKeyModifiers) {
    return false;
  }
  return true;
}

}  // namespace

KeyboardLockController::KeyboardLockController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager),
      esc_repeat_window_(kDefaultEscRepeatWindow),
      esc_repeat_tick_clock_(base::DefaultTickClock::GetInstance()) {}

KeyboardLockController::~KeyboardLockController() = default;

bool KeyboardLockController::HandleUserPressedEscape() {
  if (!IsKeyboardLockActive() || RequiresPressAndHoldEscToExit()) {
    return false;
  }

  base::RecordAction(base::UserMetricsAction("UnlockKeyboard_PressEsc"));
  UnlockKeyboard();
  return true;
}

void KeyboardLockController::HandleUserHeldEscape() {
  if (!IsKeyboardLockActive()) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("UnlockKeyboard_PressAndHoldEsc"));
  UnlockKeyboard();
}

void KeyboardLockController::HandleUserReleasedEscapeEarly() {
  if (RequiresPressAndHoldEscToExit()) {
    ReShowExitBubbleIfNeeded();
  }
}

bool KeyboardLockController::RequiresPressAndHoldEscToExit() const {
  DCHECK_EQ(keyboard_lock_state_ == KeyboardLockState::kUnlocked,
            exclusive_access_tab() == nullptr);
  return keyboard_lock_state_ == KeyboardLockState::kLockedWithEsc;
}

void KeyboardLockController::ExitExclusiveAccessToPreviousState() {
  UnlockKeyboard();
}

void KeyboardLockController::ExitExclusiveAccessIfNecessary() {
  UnlockKeyboard();
}

void KeyboardLockController::NotifyTabExclusiveAccessLost() {
  UnlockKeyboard();
}

bool KeyboardLockController::IsKeyboardLockActive() const {
  DCHECK_EQ(keyboard_lock_state_ == KeyboardLockState::kUnlocked,
            exclusive_access_tab() == nullptr);
  return keyboard_lock_state_ != KeyboardLockState::kUnlocked;
}

void KeyboardLockController::RequestKeyboardLock(WebContents* web_contents,
                                                 bool esc_key_locked) {
  DCHECK(!exclusive_access_tab() || exclusive_access_tab() == web_contents);
  if (!base::FeatureList::IsEnabled(
          permissions::features::kKeyboardAndPointerLockPrompt)) {
    LockKeyboard(web_contents->GetWeakPtr(), esc_key_locked);
    return;
  }
  exclusive_access_manager()->permission_manager().QueuePermissionRequest(
      blink::PermissionType::KEYBOARD_LOCK,
      base::BindOnce(&KeyboardLockController::LockKeyboard,
                     weak_ptr_factory_.GetWeakPtr(), web_contents->GetWeakPtr(),
                     esc_key_locked),
      base::BindOnce(&KeyboardLockController::UnlockKeyboardForWebContents,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetWeakPtr()),
      web_contents);
}

bool KeyboardLockController::HandleKeyEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (base::FeatureList::IsEnabled(
          features::kPressAndHoldEscToExitBrowserFullscreen)) {
    return false;
  }

  DCHECK_EQ(ui::VKEY_ESCAPE, event.windows_key_code);
  // This method handles the press and hold gesture used for exiting fullscreen.
  // If we don't have a feature which requires press and hold, or there isn't an
  // active keyboard lock request which requires press and hold, then we just
  // return as the simple 'press esc to exit' case is handled by the caller
  // (which is the ExclusiveAccessManager in this case).
  if (!RequiresPressAndHoldEscToExit())
    return false;

  // Note: This logic handles exiting fullscreen but the UI feedback element is
  // created and managed by the FullscreenControlHost class.
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kKeyUp &&
      hold_timer_.IsRunning()) {
    // Seeing a key up event on Esc with the hold timer running cancels the
    // timer and doesn't exit. This means the user pressed Esc, but not long
    // enough to trigger an exit
    hold_timer_.Stop();
    ReShowExitBubbleIfNeeded();
  } else if (IsUnmodifiedEscKeyDownEvent(event) && !hold_timer_.IsRunning()) {
    // Seeing a key down event on Esc when the hold timer is stopped starts
    // the timer. When the timer fires, the callback will trigger an exit from
    // fullscreen/pointerlock/keyboardlock.
    hold_timer_.Start(
        FROM_HERE, kHoldEscapeTime,
        base::BindOnce(&KeyboardLockController::HandleUserHeldEscapeDeprecated,
                       base::Unretained(this)));
  }

  return true;
}

void KeyboardLockController::CancelKeyboardLockRequest(WebContents* tab) {
  if (tab == exclusive_access_tab())
    UnlockKeyboard();
}

void KeyboardLockController::LockKeyboard(
    base::WeakPtr<content::WebContents> web_contents,
    bool esc_key_locked) {
  if (!web_contents) {
    return;
  }
  // Call GotResponseToKeyboardLockRequest() to notify `web_contents` of the
  // result, regardless of the fullscreen state.
  if (!web_contents->GotResponseToKeyboardLockRequest(true) ||
      !web_contents->IsFullscreen()) {
    UnlockKeyboard();
    return;
  }
  KeyboardLockState new_lock_state = esc_key_locked
                                         ? KeyboardLockState::kLockedWithEsc
                                         : KeyboardLockState::kLockedWithoutEsc;
  // Only re-show the exit bubble if the requesting web_contents has changed
  // (or is new) or if the esc key lock state has changed.
  bool reshow_exit_bubble = exclusive_access_tab() != web_contents.get() ||
                            new_lock_state != keyboard_lock_state_;
  keyboard_lock_state_ = new_lock_state;
  SetTabWithExclusiveAccess(web_contents.get());
  if (reshow_exit_bubble) {
    exclusive_access_manager()->UpdateBubble(
        bubble_hide_callback_for_test_
            ? base::BindOnce(bubble_hide_callback_for_test_)
            : base::NullCallback());
  }
}

void KeyboardLockController::UnlockKeyboard() {
  UnlockKeyboardForWebContents(
      exclusive_access_tab() ? exclusive_access_tab()->GetWeakPtr() : nullptr);
}

void KeyboardLockController::UnlockKeyboardForWebContents(
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }

  keyboard_lock_state_ = KeyboardLockState::kUnlocked;

  web_contents->GotResponseToKeyboardLockRequest(false);
  SetTabWithExclusiveAccess(nullptr);
  exclusive_access_manager()->UpdateBubble(base::NullCallback());
}

void KeyboardLockController::HandleUserHeldEscapeDeprecated() {
  if (base::FeatureList::IsEnabled(
          features::kPressAndHoldEscToExitBrowserFullscreen)) {
    return;
  }

  ExclusiveAccessManager* const manager = exclusive_access_manager();
  manager->fullscreen_controller()->HandleUserPressedEscape();
  manager->pointer_lock_controller()->HandleUserPressedEscape();
  HandleUserPressedEscape();
  base::RecordAction(base::UserMetricsAction("UnlockKeyboard_PressAndHoldEsc"));
}

void KeyboardLockController::ReShowExitBubbleIfNeeded() {
  TimeTicks now = esc_repeat_tick_clock_->NowTicks();
  TimeTicks esc_repeat_window = now - esc_repeat_window_;
  // Remove any events which are outside of the window.
  while (!esc_keypress_tracker_.empty() &&
         esc_keypress_tracker_.front() < esc_repeat_window) {
    esc_keypress_tracker_.pop_front();
  }

  esc_keypress_tracker_.push_back(now);
  if (esc_keypress_tracker_.size() >= kEscRepeatCountToTriggerUiReshow) {
    exclusive_access_manager()->UpdateBubble(base::NullCallback(),
                                             /*force_update=*/true);
    esc_keypress_tracker_.clear();

    if (esc_repeat_triggered_for_test_)
      std::move(esc_repeat_triggered_for_test_).Run();
  }
}
