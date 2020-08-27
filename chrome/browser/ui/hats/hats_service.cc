// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"

constexpr char kHatsSurveyTriggerTesting[] = "testing";
constexpr char kHatsSurveyTriggerSatisfaction[] = "satisfaction";
constexpr char kHatsSurveyTriggerSettings[] = "settings";
constexpr char kHatsSurveyTriggerSettingsPrivacy[] = "settings-privacy";

constexpr char kHatsNextSurveyTriggerIDTesting[] =
    "zishSVViB0kPN8UwQ150VGjBKuBP";

namespace {

const base::Feature* survey_features[] = {
    &features::kHappinessTrackingSurveysForDesktop,
    &features::kHappinessTrackingSurveysForDesktopSettings,
    &features::kHappinessTrackingSurveysForDesktopSettingsPrivacy};

// Which survey we're triggering
constexpr char kHatsSurveyTrigger[] = "survey";

constexpr char kHatsSurveyProbability[] = "probability";

constexpr char kHatsSurveyEnSiteID[] = "en_site_id";

constexpr double kHatsSurveyProbabilityDefault = 0;

constexpr char kHatsSurveyEnSiteIDDefault[] = "bhej2dndhpc33okm6xexsbyv4y";

constexpr base::TimeDelta kMinimumTimeBetweenSurveyStarts =
    base::TimeDelta::FromDays(60);

constexpr base::TimeDelta kMinimumTimeBetweenSurveyChecks =
    base::TimeDelta::FromDays(1);

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

std::string GetIsSurveyFull(const std::string& trigger) {
  return trigger + ".is_survey_full";
}

std::string GetLastSurveyCheckTime(const std::string& trigger) {
  return trigger + ".last_survey_check_time";
}

constexpr char kHatsShouldShowSurveyReasonHistogram[] =
    "Feedback.HappinessTrackingSurvey.ShouldShowSurveyReason";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShouldShowSurveyReasons {
  kYes = 0,
  kNoOffline = 1,
  kNoLastSessionCrashed = 2,
  kNoReceivedSurveyInCurrentMilestone = 3,
  kNoProfileTooNew = 4,
  kNoLastSurveyTooRecent = 5,
  kNoBelowProbabilityLimit = 6,
  kNoTriggerStringMismatch = 7,
  kNoNotRegularBrowser = 8,
  kNoIncognitoDisabled = 9,
  kNoCookiesBlocked = 10,            // Unused.
  kNoThirdPartyCookiesBlocked = 11,  // Unused.
  kNoSurveyUnreachable = 12,
  kNoSurveyOverCapacity = 13,
  kNoSurveyAlreadyInProgress = 14,
  kMaxValue = kNoSurveyAlreadyInProgress,
};

}  // namespace

HatsService::SurveyMetadata::SurveyMetadata() = default;

HatsService::SurveyMetadata::~SurveyMetadata() = default;

HatsService::DelayedSurveyTask::DelayedSurveyTask(
    HatsService* hats_service,
    const std::string& trigger,
    content::WebContents* web_contents)
    : hats_service_(hats_service), trigger_(trigger) {
  Observe(web_contents);
}

HatsService::DelayedSurveyTask::~DelayedSurveyTask() = default;

base::WeakPtr<HatsService::DelayedSurveyTask>
HatsService::DelayedSurveyTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HatsService::DelayedSurveyTask::Launch() {
  hats_service_->LaunchSurveyForWebContents(trigger_, web_contents());
  hats_service_->RemoveTask(*this);
}

void HatsService::DelayedSurveyTask::WebContentsDestroyed() {
  hats_service_->RemoveTask(*this);
}

HatsService::HatsService(Profile* profile) : profile_(profile) {
  for (auto* survey_feature : survey_features) {
    if (!base::FeatureList::IsEnabled(*survey_feature))
      continue;
    survey_configs_by_triggers_.emplace(
        base::FeatureParam<std::string>(survey_feature, kHatsSurveyTrigger, "")
            .Get(),
        SurveyConfig(
            base::FeatureParam<double>(survey_feature, kHatsSurveyProbability,
                                       kHatsSurveyProbabilityDefault)
                .Get(),
            base::FeatureParam<std::string>(survey_feature, kHatsSurveyEnSiteID,
                                            kHatsSurveyEnSiteIDDefault)
                .Get()));
  }
  // Ensure a default survey exists (for testing and demo purpose).
  auto* default_survey_id =
      base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopMigration)
          ? kHatsNextSurveyTriggerIDTesting
          : kHatsSurveyEnSiteIDDefault;
  survey_configs_by_triggers_.emplace(kHatsSurveyTriggerTesting,
                                      SurveyConfig(1.0f, default_survey_id));
}

HatsService::~HatsService() = default;

// static
void HatsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kHatsSurveyMetadata);
}

void HatsService::LaunchSurvey(const std::string& trigger) {
  if (!ShouldShowSurvey(trigger))
    return;
  LaunchSurveyForBrowser(trigger, chrome::FindLastActiveWithProfile(profile_));
}

bool HatsService::LaunchDelayedSurvey(const std::string& trigger,
                                      int timeout_ms) {
  return base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HatsService::LaunchSurvey, weak_ptr_factory_.GetWeakPtr(),
                     trigger),
      base::TimeDelta::FromMilliseconds(timeout_ms));
}

bool HatsService::LaunchDelayedSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    int timeout_ms) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return false;
  auto result = pending_tasks_.emplace(this, trigger, web_contents);
  if (!result.second)
    return false;
  auto success = base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &HatsService::DelayedSurveyTask::Launch,
          const_cast<HatsService::DelayedSurveyTask&>(*(result.first))
              .GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(timeout_ms));
  if (!success) {
    pending_tasks_.erase(result.first);
  }
  return success;
}

void HatsService::RecordSurveyAsShown(std::string survey_id) {
  // Record the trigger associated with the survey_id. This is recorded instead
  // of the survey ID itself, as the ID is specific to individual survey
  // versions. There should be a cooldown before a user is prompted to take a
  // survey from the same trigger, regardless of whether the survey was updated.
  // TODO(crbug.com/1110888): When HaTS V1 is deprecated, improve nomenclature
  // to remove confusion between trigger, trigger ID, survey ID and site ID.
  auto trigger_survey_config = std::find_if(
      survey_configs_by_triggers_.begin(), survey_configs_by_triggers_.end(),
      [&](const std::pair<std::string, SurveyConfig>& pair) {
        return pair.second.en_site_id_ == survey_id;
      });

  DCHECK(trigger_survey_config != survey_configs_by_triggers_.end());
  std::string trigger = trigger_survey_config->first;

  UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                            ShouldShowSurveyReasons::kYes);

  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::DictionaryValue* pref_data = update.Get();
  pref_data->SetIntPath(GetMajorVersionPath(trigger),
                        version_info::GetVersion().components()[0]);
  pref_data->SetPath(GetLastSurveyStartedTime(trigger),
                     util::TimeToValue(base::Time::Now()));
}

void HatsService::HatsNextDialogClosed() {
  hats_next_dialog_exists_ = false;
}

void HatsService::SetSurveyMetadataForTesting(
    const HatsService::SurveyMetadata& metadata) {
  const std::string& trigger = kHatsSurveyTriggerSatisfaction;
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::DictionaryValue* pref_data = update.Get();
  if (!metadata.last_major_version.has_value() &&
      !metadata.last_survey_started_time.has_value() &&
      !metadata.is_survey_full.has_value() &&
      !metadata.last_survey_check_time.has_value()) {
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

  if (metadata.is_survey_full.has_value()) {
    pref_data->SetBoolPath(GetIsSurveyFull(trigger), *metadata.is_survey_full);
  } else {
    pref_data->RemovePath(GetIsSurveyFull(trigger));
  }

  if (metadata.last_survey_check_time.has_value()) {
    pref_data->SetPath(GetLastSurveyCheckTime(trigger),
                       util::TimeToValue(*metadata.last_survey_check_time));
  } else {
    pref_data->RemovePath(GetLastSurveyCheckTime(trigger));
  }
}

void HatsService::GetSurveyMetadataForTesting(
    HatsService::SurveyMetadata* metadata) const {
  const std::string& trigger = kHatsSurveyTriggerSatisfaction;
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::DictionaryValue* pref_data = update.Get();

  base::Optional<int> last_major_version =
      pref_data->FindIntPath(GetMajorVersionPath(trigger));
  if (last_major_version.has_value())
    metadata->last_major_version = last_major_version;

  base::Optional<base::Time> last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyStartedTime(trigger)));
  if (last_survey_started_time.has_value())
    metadata->last_survey_started_time = last_survey_started_time;

  base::Optional<bool> is_survey_full =
      pref_data->FindBoolPath(GetIsSurveyFull(trigger));
  if (is_survey_full.has_value())
    metadata->is_survey_full = is_survey_full;

  base::Optional<base::Time> last_survey_check_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value())
    metadata->last_survey_check_time = last_survey_check_time;
}

void HatsService::SetSurveyCheckerForTesting(
    std::unique_ptr<HatsSurveyStatusChecker> checker) {
  checker_ = std::move(checker);
}

void HatsService::RemoveTask(const DelayedSurveyTask& task) {
  pending_tasks_.erase(task);
}

bool HatsService::HasPendingTasks() {
  return !pending_tasks_.empty();
}

void HatsService::LaunchSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents) {
  if (ShouldShowSurvey(trigger) && web_contents &&
      web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    LaunchSurveyForBrowser(trigger,
                           chrome::FindBrowserWithWebContents(web_contents));
  }
}

void HatsService::LaunchSurveyForBrowser(const std::string& trigger,
                                         Browser* browser) {
  if (!browser || !browser->is_type_normal() ||
      !profiles::IsRegularOrGuestSession(browser)) {
    // Never show HaTS bubble for Incognito mode.
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoNotRegularBrowser);
    return;
  }
  if (IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) ==
      IncognitoModePrefs::DISABLED) {
    // Incognito mode needs to be enabled to create an off-the-record profile
    // for HaTS dialog.
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoIncognitoDisabled);
    return;
  }
  // Checking survey's status could be costly due to a network request, so
  // we check it at the last.
  CheckSurveyStatusAndMaybeShow(browser, trigger);
}

bool HatsService::ShouldShowSurvey(const std::string& trigger) const {
  // Do not show if a survey dialog already exists.
  if (hats_next_dialog_exists_) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoSurveyAlreadyInProgress);
    return false;
  }

  // Survey should not be loaded if the corresponding survey config is
  // unavailable.
  if (survey_configs_by_triggers_.find(trigger) ==
      survey_configs_by_triggers_.end()) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoTriggerStringMismatch);
    return false;
  }

  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    // Always show the survey in demo mode.
    return true;
  }

  // Survey can not be loaded and shown if there is no network connection.
  if (net::NetworkChangeNotifier::IsOffline()) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoOffline);
    return false;
  }

  bool consent_given =
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven();
  if (!consent_given)
    return false;

  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoLastSessionCrashed);
    return false;
  }

  const base::DictionaryValue* pref_data =
      profile_->GetPrefs()->GetDictionary(prefs::kHatsSurveyMetadata);
  base::Optional<int> last_major_version =
      pref_data->FindIntPath(GetMajorVersionPath(trigger));
  if (last_major_version.has_value() &&
      static_cast<uint32_t>(*last_major_version) ==
          version_info::GetVersion().components()[0]) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoReceivedSurveyInCurrentMilestone);
    return false;
  }

  base::Time now = base::Time::Now();

  if ((now - profile_->GetCreationTime()) < kMinimumProfileAge) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoProfileTooNew);
    return false;
  }

  base::Optional<base::Time> last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyStartedTime(trigger)));
  if (last_survey_started_time.has_value()) {
    base::TimeDelta elapsed_time_since_last_start =
        now - *last_survey_started_time;
    if (elapsed_time_since_last_start < kMinimumTimeBetweenSurveyStarts) {
      UMA_HISTOGRAM_ENUMERATION(
          kHatsShouldShowSurveyReasonHistogram,
          ShouldShowSurveyReasons::kNoLastSurveyTooRecent);
      return false;
    }
  }

  auto probability_ = survey_configs_by_triggers_.at(trigger).probability_;
  bool should_show_survey = base::RandDouble() < probability_;
  if (!should_show_survey) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoBelowProbabilityLimit);
  }

  return should_show_survey;
}

void HatsService::CheckSurveyStatusAndMaybeShow(Browser* browser,
                                                const std::string& trigger) {
  // Check the survey status in profile first.
  // We record the survey's over capacity information in user profile to avoid
  // duplicated checks since the survey won't change once it is full.
  const base::DictionaryValue* pref_data =
      profile_->GetPrefs()->GetDictionary(prefs::kHatsSurveyMetadata);
  base::Optional<int> is_full =
      pref_data->FindBoolPath(GetIsSurveyFull(trigger));
  if (is_full.has_value() && is_full)
    return;

  base::Optional<base::Time> last_survey_check_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value()) {
    base::TimeDelta elapsed_time_since_last_check =
        base::Time::Now() - *last_survey_check_time;
    if (elapsed_time_since_last_check < kMinimumTimeBetweenSurveyChecks)
      return;
  }

  DCHECK(survey_configs_by_triggers_.find(trigger) !=
         survey_configs_by_triggers_.end());

  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopMigration)) {
    // Bypass the checker for showing HaTS Next surveys as the survey website
    // itself will determine eligibility. This is communicated via updates to
    // HatsNextWebDialog::OnSurveyStateUpdateReceived.
    DCHECK(!hats_next_dialog_exists_);
    browser->window()->ShowHatsBubble(
        survey_configs_by_triggers_[trigger].en_site_id_);
    hats_next_dialog_exists_ = true;
  } else {
    if (!checker_)
      checker_ = std::make_unique<HatsSurveyStatusChecker>(profile_);
    checker_->CheckSurveyStatus(
        survey_configs_by_triggers_[trigger].en_site_id_,
        base::BindOnce(&HatsService::ShowSurvey, weak_ptr_factory_.GetWeakPtr(),
                       browser, trigger),
        base::BindOnce(&HatsService::OnSurveyStatusError,
                       weak_ptr_factory_.GetWeakPtr(), trigger));
  }
}

void HatsService::ShowSurvey(Browser* browser, const std::string& trigger) {
  auto survey_id = survey_configs_by_triggers_[trigger].en_site_id_;
  RecordSurveyAsShown(survey_id);
  browser->window()->ShowHatsBubble(survey_id);
  checker_.reset();
}

void HatsService::OnSurveyStatusError(const std::string& trigger,
                                      HatsSurveyStatusChecker::Status error) {
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::DictionaryValue* pref_update_data = update.Get();

  if (error == HatsSurveyStatusChecker::Status::kUnreachable) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoSurveyUnreachable);
    pref_update_data->SetPath(GetLastSurveyCheckTime(trigger),
                              util::TimeToValue(base::Time::Now()));
  } else if (error == HatsSurveyStatusChecker::Status::kOverCapacity) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoSurveyOverCapacity);
    pref_update_data->SetBoolPath(GetIsSurveyFull(trigger), true);
  }
  checker_.reset();
}
