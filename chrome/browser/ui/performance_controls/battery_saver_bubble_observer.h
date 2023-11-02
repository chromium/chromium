// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_OBSERVER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_OBSERVER_H_

// This observer interface for the battery saver bubble dialog.
class BatterySaverBubbleObserver {
 public:
  // Called when the battery saver dialog is opened.
  virtual void OnBubbleShown() = 0;

  // Called when the battery saver dialog is closed.
  virtual void OnBubbleHidden() = 0;

 protected:
  virtual ~BatterySaverBubbleObserver() = default;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_OBSERVER_H_
