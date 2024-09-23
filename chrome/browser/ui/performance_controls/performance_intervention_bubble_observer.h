// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_OBSERVER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_OBSERVER_H_

// This observer interface for the performance intervention bubble dialog.
class PerformanceInterventionBubbleObserver {
 public:
  // Called when the performance intervention dialog is opened.
  virtual void OnBubbleShown() = 0;

  // Called when the performance intervention dialog is closed
  // by methods other than clicking the deactivate button (i.e X button,
  // dismiss button, esc.)
  virtual void OnBubbleHidden() = 0;

  // Called when the performance intervention dialog's deactivate tab
  // button (Ok) is clicked.
  virtual void OnDeactivateButtonClicked() = 0;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_OBSERVER_H_
