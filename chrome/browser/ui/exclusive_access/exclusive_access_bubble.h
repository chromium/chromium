// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

class ExclusiveAccessManager;

namespace gfx {
class Rect;
}

// Bubble that informs the user when an exclusive access state is in effect and
// as to how to exit out of the state. Currently there are three exclusive
// access states: fullscreen, keyboard lock, and mouse lock.
//
// Notification display design note: if the #simplified-fullscreen-ui flag is
// enabled, the bubble has the following behaviour:
// - The bubble is shown for kInitialDelayMs, then hides.
// - After a bubble has been shown, notifications are suppressed for
//   kSnoozeNotificationsTimeMs, to avoid bothering the user. After this time
//   has elapsed, the next user input re-displays the bubble.
class ExclusiveAccessBubble : public gfx::AnimationDelegate {
 public:
  ExclusiveAccessBubble(ExclusiveAccessManager* manager,
                        const GURL& url,
                        ExclusiveAccessBubbleType bubble_type);
  ~ExclusiveAccessBubble() override;

  // Informs the ExclusiveAccessBubble of some user input, which may update
  // internal timers and/or re-display the bubble.
  void OnUserInput();

 protected:
  static const int kPaddingPx;        // Amount of padding around the link
  static const int kInitialDelayMs;   // Initial time bubble remains onscreen
  static const int kIdleTimeMs;       // Time before mouse idle triggers hide
  static const int kSnoozeNotificationsTimeMs;
  static const int kPositionCheckHz;  // How fast to check the mouse position
  // Height of region triggering slide-in.
  static const int kSlideInRegionHeightPx;
  // Space between the popup and the top of the screen (excluding shadow).
  static const int kPopupTopPx;
  // Space between top of screen and popup, in simplified UI.
  static const int kSimplifiedPopupTopPx;

  // Returns the current desirable rect for the popup window in screen
  // coordinates. If |ignore_animation_state| is true this returns the rect
  // assuming the popup is fully onscreen.
  virtual gfx::Rect GetPopupRect(bool ignore_animation_state) const = 0;
  virtual gfx::Point GetCursorScreenPoint() = 0;
  virtual bool WindowContainsPoint(gfx::Point pos) = 0;

  // Returns true if the window is active.
  virtual bool IsWindowActive() = 0;

  // Hides the bubble.  This is a separate function so it can be called by a
  // timer.
  virtual void Hide() = 0;

  // Shows the bubble.
  virtual void Show() = 0;

  virtual bool IsAnimating() = 0;

  // True if the mouse position can trigger showing the exit fullscreen bubble
  // when the bubble is hidden.
  virtual bool CanTriggerOnMouse() const = 0;

  void StartWatchingMouse();
  void StopWatchingMouse();
  bool IsWatchingMouse() const;

  // Called repeatedly to get the current mouse position and animate the bubble
  // on or off the screen as appropriate.
  void CheckMousePosition();

  void ExitExclusiveAccess();

  // The following strings may change according to the content type and URL.
  base::string16 GetCurrentMessageText() const;
  base::string16 GetCurrentDenyButtonText() const;
  base::string16 GetCurrentAllowButtonText() const;

  // This string *may* contain the name of the key surrounded in pipe characters
  // ('|'), which should be drawn graphically as a key, not displayed literally.
  // |accelerator| is the name of the key to exit fullscreen mode.
  base::string16 GetInstructionText(const base::string16& accelerator) const;

  bool IsHideTimeoutRunning() const;

  // The Manager associated with this bubble.
  ExclusiveAccessManager* const manager_;

  // The host the bubble is for, can be empty.
  GURL url_;

  // The type of the bubble; controls e.g. which buttons to show.
  ExclusiveAccessBubbleType bubble_type_;

 private:
  friend class FullscreenControllerTest;

  // Shows the bubble and sets up timers to auto-hide and prevent re-showing for
  // a certain snooze time.
  void ShowAndStartTimers();

  // When this timer is active, prevent the bubble from hiding. This ensures it
  // will be displayed for a minimum amount of time (which can be extended by
  // the user moving the mouse to the top of the screen and holding it there).
  base::OneShotTimer hide_timeout_;

  // Timer to see how long the user has been idle (from all input sources).
  base::OneShotTimer idle_timeout_;

  // When this timer has elapsed, on the next mouse input, we will notify the
  // user about any currently active exclusive access. This is used to enact
  // both the initial debounce period, and the snooze period before re-notifying
  // the user (see notification display design note above).
  base::OneShotTimer suppress_notify_timeout_;

  // Timer to poll the current mouse position.  We can't just listen for mouse
  // events without putting a non-empty HWND onscreen (or hooking Windows, which
  // has other problems), so instead we run a low-frequency poller to see if the
  // user has moved in or out of our show/hide regions.
  base::RepeatingTimer mouse_position_checker_;

  // The most recently seen mouse position, in screen coordinates.  Used to see
  // if the mouse has moved since our last check. Only used in non-simplified
  // fullscreen mode.
  gfx::Point last_mouse_pos_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessBubble);
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_
