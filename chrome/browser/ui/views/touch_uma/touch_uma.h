// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOUCH_UMA_TOUCH_UMA_H_
#define CHROME_BROWSER_UI_VIEWS_TOUCH_UMA_TOUCH_UMA_H_

class TouchUMA {
 public:
  enum GestureActionType {
    kGestureTabSwitchTap = 0,
    kGestureTabNoSwitchTap = 1,
    kGestureTabCloseTap = 2,
    kGestureNewTabTap = 3,
    kGestureRootViewTopTap = 4,
    kMaxValue = kGestureRootViewTopTap,
  };

  TouchUMA() = delete;
  TouchUMA(const TouchUMA&) = delete;
  TouchUMA& operator=(const TouchUMA&) = delete;

  static void RecordGestureAction(GestureActionType action);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOUCH_UMA_TOUCH_UMA_H_
