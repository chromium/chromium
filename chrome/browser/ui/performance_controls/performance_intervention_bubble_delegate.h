// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/models/dialog_model.h"

class Browser;
class PerformanceInterventionBubbleObserver;
class TabListModel;

// This class is the delegate for the performance intervention bubble dialog
// that handles the events raised from the dialog.
class PerformanceInterventionBubbleDelegate : public ui::DialogModelDelegate {
 public:
  PerformanceInterventionBubbleDelegate(
      Browser* browser,
      std::unique_ptr<TabListModel> tab_list_model,
      PerformanceInterventionBubbleObserver* observer);

  ~PerformanceInterventionBubbleDelegate() override;

  // Notify intervention bubble observers that the intervention bubble is
  // closed.
  void OnBubbleClosed();

  // Record that the intervention dialog dismiss button was clicked.
  void OnDismissButtonClicked();

  // Record that the deactivate button was clicked and discard the selected
  // tabs in the tab list.
  void OnDeactivateButtonClicked();

 private:
  raw_ptr<Browser> browser_;
  std::unique_ptr<TabListModel> tab_list_model_;
  const raw_ptr<PerformanceInterventionBubbleObserver> observer_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_DELEGATE_H_
