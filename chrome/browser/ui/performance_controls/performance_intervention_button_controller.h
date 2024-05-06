// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_H_

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"

namespace {

using performance_manager::user_tuning::PerformanceDetectionManager;

}  // namespace

class PerformanceInterventionButtonController
    : public PerformanceDetectionManager::ActionableTabsObserver {
 public:
  explicit PerformanceInterventionButtonController(
      PerformanceInterventionButtonControllerDelegate* delegate);
  ~PerformanceInterventionButtonController() override;

  PerformanceInterventionButtonController(
      const PerformanceInterventionButtonController&) = delete;
  PerformanceInterventionButtonController& operator=(
      const PerformanceInterventionButtonController&) = delete;

  // PerformanceDetectionManager::ActionableTabsObserver:
  void OnActionableTabListChanged(
      PerformanceDetectionManager::ResourceType type,
      PerformanceDetectionManager::ActionableTabsResult result) override;

 private:
  raw_ptr<PerformanceInterventionButtonControllerDelegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_CONTROLLER_H_
