// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "ui/base/models/dialog_model.h"

// This class is the delegate for the high efficiency bubble dialog that handles
// the events raised from the dialog.
class HighEfficiencyBubbleDelegate : public ui::DialogModelDelegate {
 public:
  explicit HighEfficiencyBubbleDelegate(Browser* browser,
                                        HighEfficiencyBubbleObserver* observer);

  void OnSettingsClicked();
  void OnAddSiteToExceptionsListClicked();

  void OnDialogDestroy();

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<HighEfficiencyBubbleObserver> observer_;
  HighEfficiencyBubbleActionType close_action_ =
      HighEfficiencyBubbleActionType::kDismiss;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_DELEGATE_H_
