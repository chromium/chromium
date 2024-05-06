// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

PerformanceInterventionButtonController::
    PerformanceInterventionButtonController(
        PerformanceInterventionButtonControllerDelegate* delegate) {
  CHECK(delegate);
  delegate_ = delegate;
  CHECK(PerformanceDetectionManager::HasInstance());
  PerformanceDetectionManager* const detection_manager =
      PerformanceDetectionManager::GetInstance();
  const PerformanceDetectionManager::ResourceTypeSet resource_types = {
      PerformanceDetectionManager::ResourceType::kCpu};
  detection_manager->AddActionableTabsObserver(resource_types, this);
}

PerformanceInterventionButtonController::
    ~PerformanceInterventionButtonController() {
  if (PerformanceDetectionManager::HasInstance()) {
    PerformanceDetectionManager* const detection_manager =
        PerformanceDetectionManager::GetInstance();
    detection_manager->RemoveActionableTabsObserver(this);
  }
}

void PerformanceInterventionButtonController::OnActionableTabListChanged(
    PerformanceDetectionManager::ResourceType type,
    PerformanceDetectionManager::ActionableTabsResult result) {
  // TODO(crbug.com/338072465): Implement show/hide toolbar button functionality
  delegate_->Show();
}
