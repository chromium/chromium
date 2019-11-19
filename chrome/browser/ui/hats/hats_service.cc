// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/version_info/version_info.h"
#include "net/base/network_change_notifier.h"

namespace {
// Which survey we're triggering
constexpr char kHatsSurveyTrigger[] = "survey";

constexpr char kHatsSurveyProbability[] = "probability";

constexpr char kHatsSurveyEnSiteID[] = "en_site_id";

constexpr double kHatsSurveyProbabilityDefault = 0;

constexpr char kHatsSurveyEnSiteIDDefault[] = "ty52vxwjrabfvhusawtrmkmx6m";

constexpr char kHatsSurveyTriggerSatisfaction[] = "satisfaction";

constexpr base::TimeDelta kMinimumTimeBetweenSurveyStarts =
    base::TimeDelta::FromDays(60);

constexpr base::TimeDelta kMinimumProfileAge = base::TimeDelta::FromDays(30);

// Preferences Data Model
// The kHatsSurveyMetadata pref points to a dictionary.
// The valid keys and value types for this dictionary are as follows:
// [trigger].last_major_version        ---> Integer
// [trigger].last_survey_started_time  ---> Time

std::string GetMajorVersionPath(const std::string& trigger) {
  return trigger + ".last_major_version";
}

std::string GetLastSurveyStartedTime(const std::string& trigger) {
  return trigger + ".last_survey_started_time";
}

}  // namespace

HatsService::SurveyMetadata::SurveyMetadata() = default;
HatsService::SurveyMetadata::~SurveyMetadata() = default;

HatsService::HatsService(Profile* profile)
    : profile_(profile),
      trigger_(base::FeatureParam<std::string>(
                   &features::kHappinessTrackingSurveysForDesktop,
                   kHatsSurveyTrigger,
                   "")
                   .Get()),
      probability_(base::FeatureParam<double>(
                       &features::kHappinessTrackingSurveysForDesktop,
                       kHatsSurveyProbability,
                       kHatsSurveyProbabilityDefault)
                       .Get()),
      en_site_id_(base::FeatureParam<std::string>(
                      &features::kHappinessTrackingSurveysForDesktop,
                      kHatsSurveyEnSiteID,
                      kHatsSurveyEnSiteIDDefault)
                      .Get()) {}

// static
void HatsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kHatsSurveyMetadata);
}

void HatsService::LaunchSatisfactionSurvey() {
  if (ShouldShowSurvey(kHatsSurveyTriggerSatisfaction)) {
    Browser* browser = chrome::FindLastActive();
    // Never show HaTS bubble in Incognito mode.
    if (browser && browser->is_type_normal() &&
        profiles::IsRegularOrGuestSession(browser)) {
      browser->window()->ShowHatsBubble(en_site_id_);

      DictionaryPrefUpdate update(profile_->GetPrefs(),
                                  prefs::kHatsSurveyMetadata);
      base::DictionaryValue* pref_data = update.Get();
      pref_data->SetIntPath(GetMajorVersionPath(kHatsSurveyTriggerSatisfaction),
                            version_info::GetVersion().components()[0]);
      pref_data->SetPath(
          GetLastSurveyStartedTime(kHatsSurveyTriggerSatisfaction),
          util::TimeToValue(base::Time::Now()));
    }
  }
}

void HatsService::SetSurveyMetadataForTesting(
    const HatsService::SurveyMetadata& metadata) {
  const std::string& trigger = kHatsSurveyTriggerSatisfaction;
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::DictionaryValue* pref_data = update.Get();
  if (!metadata.last_major_version.has_value() &&
      !metadata.last_survey_started_time.has_value()) {
    pref_data->RemovePath(trigger);
  }

  if (metadata.last_major_version.has_value()) {
    pref_data->SetIntPath(GetMajorVersionPath(trigger),
                          *metadata.last_major_version);
  } else {
    pref_data->RemovePath(GetMajorVersionPath(trigger));
  }

  if (metadata.last_survey_started_time.has_value()) {
    pref_data->SetPath(GetLastSurveyStartedTime(trigger),
                       util::TimeToValue(*metadata.last_survey_started_time));
  } else {
    pref_data->RemovePath(GetLastSurveyStartedTime(trigger));
  }
}

bool HatsService::ShouldShowSurvey(const std::string& trigger) const {
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    // Always show the survey in demo mode.
    return true;
  }

  // Survey can not be loaded and shown if there is no network connection.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  bool consent_given =
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven();
  if (!consent_given)
    return false;

  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED)
    return false;

  const base::DictionaryValue* pref_data =
      profile_->GetPrefs()->GetDictionary(prefs::kHatsSurveyMetadata);
  base::Optional<int> last_major_version =
      pref_data->FindIntPath(GetMajorVersionPath(trigger));
  if (last_major_version.has_value() &&
      static_cast<uint32_t>(*last_major_version) ==
          version_info::GetVersion().components()[0]) {
    return false;
  }

  base::Time now = base::Time::Now();

  if ((now - profile_->GetCreationTime()) < kMinimumProfileAge)
    return false;

  base::Optional<base::Time> last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyStartedTime(trigger)));
  if (last_survey_started_time.has_value()) {
    base::TimeDelta elapsed_time_since_last_start =
        now - *last_survey_started_time;
    if (elapsed_time_since_last_start < kMinimumTimeBetweenSurveyStarts)
      return false;
  }

  if (trigger_ != trigger)
    return false;

  return base::RandDouble() < probability_;
}
