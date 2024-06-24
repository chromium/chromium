// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/prefs/pref_service.h"
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
  actionable_cpu_tabs_ = result;

  if (!result.empty()) {
    // Only trigger performance detection UI for the active window.
    if (browser_ != chrome::FindLastActive()) {
      return;
    }
    Profile* const profile = browser_->profile();
    auto* const tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(profile);
    CHECK(tracker);

    const bool can_show_intervention = tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHPerformanceInterventionDialogFeature);

    RecordInterventionTriggerResult(
        type, can_show_intervention
                  ? InterventionMessageTriggerResult::kShown
                  : InterventionMessageTriggerResult::kRateLimited);

    PrefService* const pref_service = g_browser_process->local_state();
    CHECK(pref_service);
    if (can_show_intervention) {
      RecordInterventionMessageCount(type, pref_service);
    } else {
      RecordInterventionRateLimitedCount(type, pref_service);
    }

    if (base::FeatureList::IsEnabled(
            performance_manager::features::kPerformanceInterventionUI) &&
        !delegate_->IsButtonShowing() && can_show_intervention) {
      delegate_->Show();
      // Immediately dismiss the feature engagement tracker because the
      // performance intervention button shouldn't prevent other promos from
      // showing.
      tracker->Dismissed(
          feature_engagement::kIPHPerformanceInterventionDialogFeature);
    }
  } else if (!delegate_->IsBubbleShowing()) {
    // Intervention button shouldn't hide while the dialog is being shown.
    HideToolbarButton();
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
      HideToolbarButton();
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
      HideToolbarButton();
    }
  }
}

void PerformanceInterventionButtonController::OnBubbleShown() {
  hide_button_timer_.Stop();
}

void PerformanceInterventionButtonController::OnBubbleHidden() {
  // Immediately hide the toolbar button since there is no longer
  // any actionable tabs.
  if (actionable_cpu_tabs_.empty()) {
    HideToolbarButton();
    return;
  }

  CHECK(!hide_button_timer_.IsRunning());
  // It is safe to use the base::Unretained(this) for the timer callback
  // as the controller owns the timer and will exist for the lifetime of
  // the timer.
  hide_button_timer_.Start(
      FROM_HERE,
      performance_manager::features::kInterventionButtonTimeout.Get(),
      base::BindRepeating(
          &PerformanceInterventionButtonController::HideToolbarButton,
          base::Unretained(this)));
}

void PerformanceInterventionButtonController::OnDeactivateButtonClicked() {
  // Immediately hide the toolbar button since the user has taken the suggested
  // action.
  HideToolbarButton();
}

void PerformanceInterventionButtonController::HideToolbarButton() {
  hide_button_timer_.Stop();
  delegate_->Hide();
}
