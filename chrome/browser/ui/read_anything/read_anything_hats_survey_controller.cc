// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_hats_survey_controller.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/accessibility/accessibility_features.h"

ReadAnythingHatsSurveyController::ReadAnythingHatsSurveyController(
    ReadAnythingController* read_anything_controller,
    tabs::TabInterface* tab)
    : read_anything_controller_(read_anything_controller), tab_(tab) {
  CHECK(read_anything_controller_);
  CHECK(features::IsHatsReadingModeSurveyEnabled());
  read_anything_controller_->AddObserver(this);
}

ReadAnythingHatsSurveyController::~ReadAnythingHatsSurveyController() = default;

void ReadAnythingHatsSurveyController::Activate(
    bool active,
    std::optional<ReadAnythingOpenTrigger> trigger,
    std::optional<base::TimeDelta> completed_session_duration) {
  // The survey is triggered upon closing Reading Mode, so do nothing when
  // activated.
  if (active) {
    return;
  }

  // `completed_session_duration` is null during presentation transitions or if
  // the UI was hidden before being shown. A valid session must be at least 10
  // seconds to qualify for a survey.
  if (!completed_session_duration.has_value() ||
      completed_session_duration.value() < base::Seconds(10)) {
    return;
  }

  // If the usage count has not been recorded yet, record it
  if (!has_recorded_usage_) {
    RecordUsageForHatsSurvey();
    has_recorded_usage_ = true;
  }
  // Check if the survey should be triggered
  MaybeShowReadingModeHatsSurvey();
}

void ReadAnythingHatsSurveyController::OnDestroyed() {
  read_anything_controller_->RemoveObserver(this);
}

void ReadAnythingHatsSurveyController::RecordUsageForHatsSurvey() {
  content::WebContents* web_contents = tab_->GetContents();
  if (!web_contents) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }
  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return;
  }
  const base::ListValue& usages =
      prefs->GetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes);
  base::ListValue new_usages;
  base::Time now = base::Time::Now();
  // Filter out usage timestamps that are older than 14 days and add the
  // current usage timestamp.
  for (const auto& val : usages) {
    std::optional<base::Time> t = base::ValueToTime(val);
    if (t && now - *t <= base::Days(14)) {
      new_usages.Append(val.Clone());
    }
  }
  new_usages.Append(base::TimeToValue(now));

  // Limit the size of the array to keep only the 3 most recent timestamps.
  if (new_usages.size() > 3) {
    new_usages.erase(new_usages.begin(),
                     new_usages.begin() + (new_usages.size() - 3));
  }

  // Set the updated list of recent usages back to the preferences.
  prefs->SetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes,
                 std::move(new_usages));
}

void ReadAnythingHatsSurveyController::MaybeShowReadingModeHatsSurvey() {
  content::WebContents* web_contents = tab_->GetContents();
  if (!web_contents) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }
  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return;
  }
  const base::ListValue& usages =
      prefs->GetList(prefs::kAccessibilityReadAnythingRecentUsagesStartTimes);

  // Condition 1: User must have used Reading Mode at least 3 times within a 2
  // week time period.
  if (usages.size() < 3) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
  // Trigger the survey to be shown.
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerReadingModeExit, web_contents, /*timeout_ms=*/0);
}
