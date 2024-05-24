// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class Browser;
class TabStripModel;

namespace {

using performance_manager::user_tuning::PerformanceDetectionManager;

}  // namespace

// This class controls the visibility of the performance intervention toolbar
// button. It observes for when taking action on certain tabs can improve
// performance health and update the visibility of the intervention toolbar
// button through a delegate interface.
class PerformanceInterventionButtonController
    : public TabStripModelObserver,
      public PerformanceDetectionManager::ActionableTabsObserver,
      public PerformanceInterventionBubbleObserver {
 public:
  PerformanceInterventionButtonController(
      PerformanceInterventionButtonControllerDelegate* delegate,
      Browser* browser);
  ~PerformanceInterventionButtonController() override;

  PerformanceInterventionButtonController(
      const PerformanceInterventionButtonController&) = delete;
  PerformanceInterventionButtonController& operator=(
      const PerformanceInterventionButtonController&) = delete;

  // PerformanceDetectionManager::ActionableTabsObserver:
  void OnActionableTabListChanged(
      PerformanceDetectionManager::ResourceType type,
      PerformanceDetectionManager::ActionableTabsResult result) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // PerformanceInterventionBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;
  void OnDeactivateButtonClicked() override;

 private:
  void HideToolbarButton();

  raw_ptr<PerformanceInterventionButtonControllerDelegate> delegate_ = nullptr;
  const raw_ptr<Browser> browser_;
  PerformanceDetectionManager::ActionableTabsResult actionable_cpu_tabs_;
  base::RetainingOneShotTimer hide_button_timer_;
  base::WeakPtrFactory<PerformanceInterventionButtonController>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_H_
