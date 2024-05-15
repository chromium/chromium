// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

PerformanceInterventionButtonController::
    PerformanceInterventionButtonController(
        PerformanceInterventionButtonControllerDelegate* delegate,
        Browser* browser)
    : browser_(browser) {
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
  Profile* const profile = browser_->profile();
  actionable_cpu_tabs_ = result;
  if (!result.empty()) {
    auto* const tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(profile);
    CHECK(tracker);
    if (tracker->ShouldTriggerHelpUI(
            feature_engagement::kIPHPerformanceInterventionDialogFeature)) {
      delegate_->Show();
      // Immediately dismiss the feature engagement tracker because the
      // performance intervention button shouldn't prevent other promos from
      // showing.
      tracker->Dismissed(
          feature_engagement::kIPHPerformanceInterventionDialogFeature);
    }
  } else {
    delegate_->Hide();
  }
}
