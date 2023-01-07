// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "ui/base/models/dialog_model.h"

class Browser;
class BatterySaverBubbleObserver;

// This class is the delegate for the battery saver bubble dialog that handles
// the events raised from the dialog.
class BatterySaverBubbleDelegate : public ui::DialogModelDelegate {
 public:
  explicit BatterySaverBubbleDelegate(Browser* browser,
                                      BatterySaverBubbleObserver* observer);

  void OnWindowClosing();
  void OnSessionOffClicked();

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<BatterySaverBubbleObserver> observer_;
  BatterySaverBubbleActionType action_type_ =
      BatterySaverBubbleActionType::kDismiss;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_DELEGATE_H_
