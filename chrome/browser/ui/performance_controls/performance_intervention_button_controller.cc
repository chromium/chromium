// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"

#include <cmath>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/values.h"
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
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace {

// This represents the duration that the performance intervention button
// should remain in the toolbar after the user dismisses the intervention
// dialog without taking the suggested action.
const base::TimeDelta kInterventionButtonTimeout = base::Seconds(10);

// Erase the oldest entries if the history size exceeds
// the max acceptance window.
void TrimAcceptHistory(PrefService* pref_service) {
  const base::Value::List& historical_acceptance = pref_service->GetList(
      performance_manager::user_tuning::prefs::
          kPerformanceInterventionNotificationAcceptHistory);
  const size_t current_size = historical_acceptance.size();
  const size_t max_acceptance = static_cast<size_t>(
      performance_manager::features::kAcceptanceRateWindowSize.Get());
  if (current_size > max_acceptance) {
    const size_t difference = current_size - max_acceptance;
    base::Value::List updated_acceptance = historical_acceptance.Clone();
    updated_acceptance.erase(updated_acceptance.begin(),
                             updated_acceptance.begin() + difference);
    pref_service->SetList(performance_manager::user_tuning::prefs::
                              kPerformanceInterventionNotificationAcceptHistory,
                          std::move(updated_acceptance));
  }
}
}  // namespace

PerformanceInterventionButtonController::
    PerformanceInterventionButtonController(
        PerformanceInterventionButtonControllerDelegate* delegate,
        Browser* browser)
    : delegate_(delegate), browser_(browser) {
  // The `PerformanceDetectionManager` is undefined in unit tests because it
  // is constructed in `ChromeContentBrowserClient::CreateBrowserMainParts`.
  if (PerformanceDetectionManager::HasInstance()) {
    const PerformanceDetectionManager::ResourceTypeSet resource_types = {
        PerformanceDetectionManager::ResourceType::kCpu};
    PerformanceDetectionManager::GetInstance()->AddActionableTabsObserver(
        resource_types, this);
    browser->tab_strip_model()->AddObserver(this);
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceInterventionNotificationImprovements)) {
    TrimAcceptHistory(g_browser_process->local_state());
  }
}

PerformanceInterventionButtonController::
    ~PerformanceInterventionButtonController() {
  if (PerformanceDetectionManager::HasInstance()) {
    PerformanceDetectionManager* const detection_manager =
        PerformanceDetectionManager::GetInstance();
    detection_manager->RemoveActionableTabsObserver(this);
    browser_->tab_strip_model()->RemoveObserver(this);
  }
}

// static
int PerformanceInterventionButtonController::GetAcceptancePercentage() {
  PrefService* const pref_service = g_browser_process->local_state();
  const base::Value::List& historical_acceptance = pref_service->GetList(
      performance_manager::user_tuning::prefs::
          kPerformanceInterventionNotificationAcceptHistory);

  if (historical_acceptance.empty()) {
    return 100;
  }

  const size_t current_size = historical_acceptance.size();
  const size_t max_acceptance = static_cast<size_t>(
      performance_manager::features::kAcceptanceRateWindowSize.Get());
  size_t starting_index = 0;
  if (current_size > max_acceptance) {
    starting_index = current_size - max_acceptance;
  }

  int total_acceptance = 0;
  for (size_t i = starting_index; i < current_size; i++) {
    if (historical_acceptance[i].GetBool()) {
      total_acceptance++;
    }
  }

  return total_acceptance * 100.0 / std::min(current_size, max_acceptance);
}

void PerformanceInterventionButtonController::OnActionableTabListChanged(
    PerformanceDetectionManager::ResourceType type,
    PerformanceDetectionManager::ActionableTabsResult result) {
  actionable_cpu_tabs_ = result;

  if (!result.empty()) {
    MaybeShowUi(type, result);
  } else if (!delegate_->IsBubbleShowing()) {
    // Intervention button shouldn't hide while the dialog is being shown.
    HideToolbarButton(false);
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
      HideToolbarButton(false);
      return;
    }
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    for (const TabStripModelChange::RemovedTab& tab :
         change.GetRemove()->contents) {
      std::optional<resource_attribution::PageContext> removed_page_context =
          resource_attribution::PageContext::FromWebContents(tab.contents);
      CHECK(removed_page_context.has_value());
      std::erase(actionable_cpu_tabs_, removed_page_context);
    }

    if (actionable_cpu_tabs_.empty()) {
      HideToolbarButton(false);
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
    HideToolbarButton(false);
    return;
  }

  CHECK(!hide_button_timer_.IsRunning());
  // It is safe to use the base::Unretained(this) for the timer callback
  // as the controller owns the timer and will exist for the lifetime of
  // the timer.
  hide_button_timer_.Start(
      FROM_HERE, kInterventionButtonTimeout,
      base::BindRepeating(
          &PerformanceInterventionButtonController::HideToolbarButton,
          base::Unretained(this), false));
}

void PerformanceInterventionButtonController::OnDeactivateButtonClicked() {
  // Immediately hide the toolbar button since the user has taken the suggested
  // action.
  HideToolbarButton(true);
}

bool PerformanceInterventionButtonController::ShouldShowNotification(
    feature_engagement::Tracker* tracker) {
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceInterventionNotificationImprovements)) {
    return true;
  }

  PrefService* const pref_service = g_browser_process->local_state();
  const base::TimeDelta time_since_intervention =
      base::Time::Now() -
      pref_service->GetTime(performance_manager::user_tuning::prefs::
                                kPerformanceInterventionNotificationLastShown);
  const int acceptance_percentage = GetAcceptancePercentage();
  // Wait until the minimum reshow time before showing another intervention.
  if (time_since_intervention <
      performance_manager::features::kMinimumTimeBetweenReshow.Get()) {
    return false;
  }

  if (acceptance_percentage == 0) {
    return time_since_intervention >=
           performance_manager::features::kNoAcceptanceBackOff.Get();
  }

  const double acceptance_rate = acceptance_percentage / 100.0;
  const int daily_max_count =
      std::ceil(performance_manager::features::kScaleMaxTimesPerDay.Get() *
                acceptance_rate);
  const int weekly_max_count =
      std::ceil(performance_manager::features::kScaleMaxTimesPerWeek.Get() *
                acceptance_rate);

  // Verify that performance detection did not hit the limit to show the
  // intervention per day and week.
  for (const auto& [config, count] : tracker->ListEvents(
           feature_engagement::kIPHPerformanceInterventionDialogFeature)) {
    if (config.window == 1 && (count >= daily_max_count)) {
      return false;
    } else if (config.window == 7 && (count >= weekly_max_count)) {
      return false;
    }
  }

  return true;
}

void PerformanceInterventionButtonController::HideToolbarButton(
    bool accept_intervention) {
  const bool was_showing = delegate_->IsButtonShowing();
  hide_button_timer_.Stop();
  delegate_->Hide();

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceInterventionNotificationImprovements) &&
      was_showing) {
    PrefService* const pref_service = g_browser_process->local_state();
    const base::Value::List& historical_acceptance = pref_service->GetList(
        performance_manager::user_tuning::prefs::
            kPerformanceInterventionNotificationAcceptHistory);

    base::Value::List updated_acceptance = historical_acceptance.Clone();
    updated_acceptance.Append(accept_intervention);

    if (updated_acceptance.size() >
        static_cast<size_t>(
            performance_manager::features::kAcceptanceRateWindowSize.Get())) {
      updated_acceptance.erase(updated_acceptance.begin());
    }

    pref_service->SetList(performance_manager::user_tuning::prefs::
                              kPerformanceInterventionNotificationAcceptHistory,
                          std::move(updated_acceptance));
  }
}

void PerformanceInterventionButtonController::MaybeShowUi(
    PerformanceDetectionManager::ResourceType type,
    const PerformanceDetectionManager::ActionableTabsResult& result) {
  PrefService* const pref_service = g_browser_process->local_state();
  CHECK(pref_service);
  // Only trigger performance detection UI for the active window and if we are
  // not already showing the UI.
  if (browser_ != chrome::FindLastActive() || delegate_->IsButtonShowing() ||
      !performance_manager::user_tuning::prefs::
          ShouldShowPerformanceInterventionNotification(pref_service)) {
    return;
  }

  Profile* const profile = browser_->profile();
  auto* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  CHECK(tracker);

  InterventionMessageTriggerResult trigger_result =
      InterventionMessageTriggerResult::kShown;

  if (ContainsNonLastActiveProfile(result)) {
    trigger_result = InterventionMessageTriggerResult::kMixedProfile;
  } else if (base::FeatureList::IsEnabled(
                 performance_manager::features::
                     kPerformanceInterventionDemoMode)) {
    trigger_result = InterventionMessageTriggerResult::kShown;
  } else if (ShouldShowNotification(tracker) &&
             tracker->ShouldTriggerHelpUI(
                 feature_engagement::
                     kIPHPerformanceInterventionDialogFeature)) {
    // Immediately dismiss the feature engagement tracker because the
    // performance intervention UI shouldn't prevent other promos from
    // showing.
    tracker->Dismissed(
        feature_engagement::kIPHPerformanceInterventionDialogFeature);
    trigger_result = InterventionMessageTriggerResult::kShown;
    RecordInterventionMessageCount(type, pref_service);
  } else {
    trigger_result = InterventionMessageTriggerResult::kRateLimited;
    RecordInterventionRateLimitedCount(type, pref_service);
  }

  RecordInterventionTriggerResult(type, trigger_result);

  if (trigger_result == InterventionMessageTriggerResult::kShown) {
    delegate_->Show();
    if (base::FeatureList::IsEnabled(
            performance_manager::features::
                kPerformanceInterventionNotificationImprovements)) {
      pref_service->SetTime(performance_manager::user_tuning::prefs::
                                kPerformanceInterventionNotificationLastShown,
                            base::Time::Now());
    }
  }
}

bool PerformanceInterventionButtonController::ContainsNonLastActiveProfile(
    const PerformanceDetectionManager::ActionableTabsResult& result) {
  Profile* const profile = chrome::FindLastActive()->profile();
  for (const resource_attribution::PageContext& context : result) {
    content::WebContents* const web_content = context.GetWebContents();
    if (!web_content) {
      // Without a WebContents, we can't check if it's from a different profile.
      return true;
    }
    Profile* const content_profile =
        Profile::FromBrowserContext(web_content->GetBrowserContext());
    if (profile != content_profile) {
      return true;
    }
  }

  return false;
}
