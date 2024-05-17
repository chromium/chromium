// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/web_contents.h"

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
  browser->tab_strip_model()->AddObserver(this);
}

PerformanceInterventionButtonController::
    ~PerformanceInterventionButtonController() {
  if (PerformanceDetectionManager::HasInstance()) {
    PerformanceDetectionManager* const detection_manager =
        PerformanceDetectionManager::GetInstance();
    detection_manager->RemoveActionableTabsObserver(this);
  }

  browser_->tab_strip_model()->RemoveObserver(this);
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

void PerformanceInterventionButtonController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    std::optional<resource_attribution::PageContext> current_page_context =
        resource_attribution::PageContext::FromWebContents(
            selection.new_contents);

    if (!current_page_context.has_value()) {
      return;
    }

    // Invalidate the actionable tab list since one of the actionable tabs is no
    // longer eligible and taking action on the remaining tabs no longer improve
    // resource health.
    if (base::Contains(actionable_cpu_tabs_, current_page_context.value())) {
      actionable_cpu_tabs_.clear();
      delegate_->Hide();
      return;
    }
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    for (const TabStripModelChange::RemovedTab& tab :
         change.GetRemove()->contents) {
      std::optional<resource_attribution::PageContext> removed_page_context =
          resource_attribution::PageContext::FromWebContents(tab.contents);
      CHECK(removed_page_context.has_value());
      const auto iter_position =
          std::remove(actionable_cpu_tabs_.begin(), actionable_cpu_tabs_.end(),
                      removed_page_context);
      if (iter_position != actionable_cpu_tabs_.end()) {
        actionable_cpu_tabs_.erase(iter_position);
      }
    }

    if (actionable_cpu_tabs_.empty()) {
      delegate_->Hide();
    }
  }
}
