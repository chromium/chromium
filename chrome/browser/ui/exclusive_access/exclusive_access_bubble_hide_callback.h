// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_HIDE_CALLBACK_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_HIDE_CALLBACK_H_

#include "base/functional/callback_forward.h"

enum class ExclusiveAccessBubbleHideReason {
  // The bubble was never shown. e.g.
  // 1. View destroyed before the bubble could be shown.
  // 2. This is a request to dismiss bubble. e.g.
  //    a. `PointerLockController` sends request A to lock pointer, then
  //    b. `PLC` sends request B to unlock mouse pointer with NULL bubble type.
  //    Resulted callbacks: A <= `kInterrupted`, B <= `kNotShown`.
  kNotShown,

  // The bubble wasn't shown for the full `ExclusiveAccessBubble::kShowTime`,
  // and was dismissed due to user or script actions. e.g.
  // 1. User pressed ESC or switched to another window;
  // 2. Script called `exitPointerLock()`, or triggered pointer lock and
  //    fullscreen at the same time.
  kInterrupted,

  // The bubble was shown for the full `ExclusiveAccessBubble::kShowTime`, and
  // was automatically dismissed by its timer.
  kTimeout,
};

using ExclusiveAccessBubbleHideCallback =
    base::OnceCallback<void(ExclusiveAccessBubbleHideReason)>;

// Repeating callback meant for testing.
using ExclusiveAccessBubbleHideCallbackForTest =
    base::RepeatingCallback<void(ExclusiveAccessBubbleHideReason)>;

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_HIDE_CALLBACK_H_
