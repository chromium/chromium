// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_KEYBOARD_LOCK_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_KEYBOARD_LOCK_CONTROLLER_H_

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"

namespace base {
class TickClock;
}  // namespace base

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

// This class implements keyboard lock behavior in the UI and decides whether
// a KeyboardLock request from a WebContents instance should be allowed or
// rejected.
class KeyboardLockController : public ExclusiveAccessControllerBase {
 public:
  explicit KeyboardLockController(ExclusiveAccessManager* manager);

  KeyboardLockController(const KeyboardLockController&) = delete;
  KeyboardLockController& operator=(const KeyboardLockController&) = delete;

  ~KeyboardLockController() override;

  // Requests KeyboardLock for |web_contents|, request is allowed if
  // |web_contents| is in tab-initiated fullscreen.
  void RequestKeyboardLock(content::WebContents* web_contents,
                           bool esc_key_locked);

  // Cancels an existing request for keyboard lock for |web_contents|.
  void CancelKeyboardLockRequest(content::WebContents* web_contents);

  // ExclusiveAccessControllerBase implementation.
  bool HandleUserPressedEscape() override;
  void HandleUserHeldEscape() override;
  void HandleUserReleasedEscapeEarly() override;
  bool RequiresPressAndHoldEscToExit() const override;
  void ExitExclusiveAccessToPreviousState() override;
  void ExitExclusiveAccessIfNecessary() override;
  void NotifyTabExclusiveAccessLost() override;

  // Returns true if the keyboard is locked.
  bool IsKeyboardLockActive() const;

  // Allows for special handling for KeyDown/KeyUp events.  Returns true if the
  // event was handled by the KeyboardLockController.
  bool HandleKeyEvent(const input::NativeWebKeyboardEvent& event);

 private:
  friend class ExclusiveAccessTest;
  friend class FullscreenControlViewTest;

  enum class KeyboardLockState {
    kUnlocked,
    kLockedWithEsc,
    kLockedWithoutEsc,
  };

  // Notifies |web_contents| that it can activate keyboard lock.
  void LockKeyboard(base::WeakPtr<content::WebContents> web_contents,
                    bool esc_key_locked);

  // Notifies the exclusive access tab that it must deactivate keyboard lock.
  void UnlockKeyboard();

  // Notifies `web_contents` that it must deactivate keyboard lock.
  void UnlockKeyboardForWebContents(
      base::WeakPtr<content::WebContents> web_contents);

  // Called when the user has held down Escape.
  void HandleUserHeldEscapeDeprecated();

  // Displays the exit instructions if the user presses escape rapidly.
  void ReShowExitBubbleIfNeeded();

  // Called after the bubble is hidden in tests, if set.
  ExclusiveAccessBubbleHideCallbackForTest bubble_hide_callback_for_test_;

  // Called after the esc repeat threshold is reached, if set.
  base::OnceClosure esc_repeat_triggered_for_test_;

  KeyboardLockState keyboard_lock_state_ = KeyboardLockState::kUnlocked;
  base::OneShotTimer hold_timer_;

  // Window which determines whether to reshow the exit fullscreen instructions.
  base::TimeDelta esc_repeat_window_;

  raw_ptr<const base::TickClock> esc_repeat_tick_clock_ = nullptr;

  base::circular_deque<base::TimeTicks> esc_keypress_tracker_;

  base::WeakPtrFactory<KeyboardLockController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_KEYBOARD_LOCK_CONTROLLER_H_
