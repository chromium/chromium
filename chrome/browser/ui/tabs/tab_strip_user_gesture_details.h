// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_USER_GESTURE_DETAILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_USER_GESTURE_DETAILS_H_

#include "base/time/time.h"

// Encapsulates user gesture information for tab activation
struct TabStripUserGestureDetails {
  // User gesture type that triggers ActivateTabAt. kNone indicates that it was
  // not triggered by a user gesture, but by a by-product of some other action.
  enum class GestureType {
    kMouse,
    kTouch,
    kWheel,
    kKeyboard,
    kOther,
    kTabMenu,
    kNone
  };

  explicit TabStripUserGestureDetails(
      GestureType type,
      base::TimeTicks time_stamp = base::TimeTicks::Now())
      : type(type), time_stamp(time_stamp) {}

  GestureType type;
  base::TimeTicks time_stamp;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_USER_GESTURE_DETAILS_H_
