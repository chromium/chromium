// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"

// Bubble that informs the user when an exclusive access state is in effect and
// how to exit out of the state. There are three exclusive access states:
// fullscreen, keyboard lock, and pointer lock.
// - The bubble is shown for kShowTime, then hides.
// - The bubble re-shows on user input after kSnoozeTime with no user input.
class ExclusiveAccessBubble {
 public:
  explicit ExclusiveAccessBubble(const ExclusiveAccessBubbleParams& params);
  ExclusiveAccessBubble(const ExclusiveAccessBubble&) = delete;
  ExclusiveAccessBubble& operator=(const ExclusiveAccessBubble&) = delete;
  virtual ~ExclusiveAccessBubble();

  // Called on user input to update timers and/or re-show the bubble.
  void OnUserInput();

  // Time the bubble is shown before hiding automatically.
  static constexpr base::TimeDelta kShowTime = base::Milliseconds(3800);
  // Time without user input that must elapse before the bubble is re-shown.
  static constexpr base::TimeDelta kSnoozeTime = base::Minutes(15);

 protected:
  // Hides the bubble.
  virtual void Hide() = 0;

  // Shows the bubble.
  virtual void Show() = 0;

  // Shows the bubble and sets up timers to auto-hide and snooze.
  void ShowAndStartTimers();

  // Cached content and traits for this bubble.
  ExclusiveAccessBubbleParams params_;

  // Hides the bubble after it has been displayed for a short time.
  base::RetainingOneShotTimer hide_timeout_;

  // Bubble re-shows on user input are suppressed until this time elapses.
  base::TimeTicks snooze_until_;

 private:
  friend class ExclusiveAccessTest;
  friend class ExclusiveAccessBubbleViewsTest;
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_
