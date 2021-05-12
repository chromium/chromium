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
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"

constexpr char kHatsSurveyTriggerTesting[] = "testing";
constexpr char kHatsSurveyTriggerPrivacySandbox[] = "privacy-sandbox";
constexpr char kHatsSurveyTriggerSettings[] = "settings";
constexpr char kHatsSurveyTriggerSettingsPrivacy[] = "settings-privacy";
constexpr char kHatsSurveyTriggerNtpModules[] = "ntp-modules";

constexpr char kHatsNextSurveyTriggerIDTesting[] =
    "zishSVViB0kPN8UwQ150VGjBKuBP";

constexpr char kHatsShouldShowSurveyReasonHistogram[] =
    "Feedback.HappinessTrackingSurvey.ShouldShowSurveyReason";

namespace {

constexpr char kHatsSurveyProbability[] = "probability";

constexpr char kHatsSurveyEnSiteID[] = "en_site_id";

constexpr double kHatsSurveyProbabilityDefault = 0;

// TODO(crbug.com/1160661): When the minimum time between any survey, and the
// minimum time between a specific survey, are the same, the logic supporting
// the latter check is superfluous.
constexpr base::TimeDelta kMinimumTimeBetweenSurveyStarts =
    base::TimeDelta::FromDays(180);

constexpr base::TimeDelta kMinimumTimeBetweenAnySurveyStarts =
    base::TimeDelta::FromDays(180);

constexpr base::TimeDelta kMinimumTimeBetweenSurveyChecks =
    base::TimeDelta::FromDays(1);

constexpr base::TimeDelta kMinimumProfileAge = base::TimeDelta::FromDays(30);

// Preferences Data Model
// The kHatsSurveyMetadata pref points to a dictionary.
// The valid keys and value types for this dictionary are as follows:
// [trigger].last_major_version        ---> Integer
// [trigger].last_survey_started_time  ---> Time
// [trigger].is_survey_full            ---> Bool
// [trigger].last_survey_check_time    ---> Time
// any_last_survey_started_time        ---> Time

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

constexpr char kAnyLastSurveyStartedTimePath[] = "any_last_survey_started_time";

std::vector<HatsService::SurveyConfig> GetSurveyConfigs() {
  std::vector<HatsService::SurveyConfig> survey_configs;

  // Dev tools surveys.
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesCOEP,
                              "devtools-issues-coep",
                              "1DbEs89FS0ugnJ3q1cK0Nx6T99yT");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesMixedContent,
                              "devtools-issues-mixed-content",
                              "BhCYpUmyf0ugnJ3q1cK0VtxCftzo");
  survey_configs.emplace_back(
      &features::
          kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite,
      "devtools-issues-cookies-samesite", "w9JqqpmEr0ugnJ3q1cK0NezVg4iK");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesHeavyAd,
                              "devtools-issues-heavy-ad",
                              "bAeiT5J4P0ugnJ3q1cK0Ra6jg7s8");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesCSP,
                              "devtools-issues-csp",
                              "c9fjDmwjb0ugnJ3q1cK0USeAJJ9C");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsLayoutPanel,
                              "devtools-layout-panel",
                              "hhoMFLFq70ugnJ3q1cK0XYpqkErh");

  // Settings surveys.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopSettings,
      kHatsSurveyTriggerSettings);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopSettingsPrivacy,
      kHatsSurveyTriggerSettingsPrivacy,
      /*presupplied_trigger_id=*/base::nullopt,
      std::vector<std::string>{"3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopPrivacySandbox,
      kHatsSurveyTriggerPrivacySandbox,
      /*presupplied_trigger_id=*/base::nullopt,
      std::vector<std::string>{"3P cookies blocked",
                               "Privacy Sandbox enabled"});

  // NTP modules survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopNtpModules,
      kHatsSurveyTriggerNtpModules);

  return survey_configs;
}

}  // namespace

HatsService::SurveyConfig::SurveyConfig(
    const base::Feature* feature,
    const std::string& trigger,
    const base::Optional<std::string>& presupplied_trigger_id,
    const std::vector<std::string>& product_specific_data_fields)
    : trigger(trigger),
      product_specific_data_fields(product_specific_data_fields) {
  DCHECK(product_specific_data_fields.size() <= 3)
      << "A maximum of 3 survey specific data fields is supported";

  enabled = base::FeatureList::IsEnabled(*feature);
  if (!enabled)
    return;

  probability = base::FeatureParam<double>(feature, kHatsSurveyProbability,
                                           kHatsSurveyProbabilityDefault)
                    .Get();

  // The trigger_id may be provided through the associated feature parameter or
  // may have been included in the source code. The latter is required to enable
  // multiple surveys with a single finch group, as a limitation with finch
  // prevents duplicate param names even for different features within a group.
  // The feature parameter name is "en_site_id" for legacy reasons, as this
  // was the HaTS v1 equivalent of a trigger ID in HaTS Next.
  trigger_id = presupplied_trigger_id ? *presupplied_trigger_id
                                      : base::FeatureParam<std::string>(
                                            feature, kHatsSurveyEnSiteID, "")
                                            .Get();

  user_prompted =
      base::FeatureParam<bool>(feature, "user_prompted", false).Get();
}

HatsService::SurveyConfig::SurveyConfig() = default;

HatsService::SurveyConfig::SurveyConfig(const SurveyConfig&) = default;

HatsService::SurveyConfig::~SurveyConfig() = default;

HatsService::SurveyMetadata::SurveyMetadata() = default;

HatsService::SurveyMetadata::~SurveyMetadata() = default;

HatsService::DelayedSurveyTask::DelayedSurveyTask(
    HatsService* hats_service,
    const std::string& trigger,
    content::WebContents* web_contents,
    const std::map<std::string, bool>& product_specific_data)
    : hats_service_(hats_service),
      trigger_(trigger),
      product_specific_data_(product_specific_data) {
  Observe(web_contents);
}

HatsService::DelayedSurveyTask::~DelayedSurveyTask() = default;

base::WeakPtr<HatsService::DelayedSurveyTask>
HatsService::DelayedSurveyTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HatsService::DelayedSurveyTask::Launch() {
  hats_service_->LaunchSurveyForWebContents(trigger_, web_contents(),
                                            product_specific_data_);
  hats_service_->RemoveTask(*this);
}

void HatsService::DelayedSurveyTask::WebContentsDestroyed() {
  hats_service_->RemoveTask(*this);
}

struct SurveyIdentifiers {
  const base::Feature* feature;
  const char* trigger;
  const char* trigger_id;
  std::vector<std::string> product_specific_data_fields;
};

HatsService::HatsService(Profile* profile) : profile_(profile) {
  auto surveys = GetSurveyConfigs();

  // Filter down to active surveys configs and store them in a map for faster
  // access. Triggers within the browser may attempt to show surveys regardless
  // of whether the feature is enabled, so checking whether a particular survey
  // is enabled should be fast.
  for (const SurveyConfig& survey : surveys) {
    if (!survey.enabled)
      continue;

    survey_configs_by_triggers_.emplace(survey.trigger, survey);
  }

  // Ensure a default survey exists (for testing and demo purpose).
  SurveyConfig default_survey;
  default_survey.enabled = true;
  default_survey.probability = 1.0f;
  default_survey.trigger = kHatsSurveyTriggerTesting;
  default_survey.trigger_id = kHatsNextSurveyTriggerIDTesting;
  default_survey.product_specific_data_fields = {"Test Field 1", "Test Field 2",
                                                 "Test Field 3"};
  survey_configs_by_triggers_.emplace(kHatsSurveyTriggerTesting,
                                      default_survey);
}

HatsService::~HatsService() = default;

// static
void HatsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kHatsSurveyMetadata,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void HatsService::LaunchSurvey(
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::map<std::string, bool>& product_specific_data) {
  if (!ShouldShowSurvey(trigger)) {
    std::move(failure_callback).Run();
    return;
  }

  LaunchSurveyForBrowser(chrome::FindLastActiveWithProfile(profile_), trigger,
                         std::move(success_callback),
                         std::move(failure_callback), product_specific_data);
}

bool HatsService::LaunchDelayedSurvey(
    const std::string& trigger,
    int timeout_ms,
    const std::map<std::string, bool>& product_specific_data) {
  return base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HatsService::LaunchSurvey, weak_ptr_factory_.GetWeakPtr(),
                     trigger, base::DoNothing::Once(), base::DoNothing::Once(),
                     product_specific_data),
      base::TimeDelta::FromMilliseconds(timeout_ms));
}

bool HatsService::LaunchDelayedSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    int timeout_ms,
    const std::map<std::string, bool>& product_specific_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return false;
  auto result = pending_tasks_.emplace(this, trigger, web_contents,
                                       product_specific_data);
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

void HatsService::RecordSurveyAsShown(std::string trigger_id) {
  // Record the trigger associated with the trigger_id. This is recorded instead
  // of the trigger ID itself, as the ID is specific to individual survey
  // versions. There should be a cooldown before a user is prompted to take a
  // survey from the same trigger, regardless of whether the survey was updated.
  auto trigger_survey_config = std::find_if(
      survey_configs_by_triggers_.begin(), survey_configs_by_triggers_.end(),
      [&](const std::pair<std::string, SurveyConfig>& pair) {
        return pair.second.trigger_id == trigger_id;
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
  pref_data->SetPath(kAnyLastSurveyStartedTimePath,
                     util::TimeToValue(base::Time::Now()));
}

void HatsService::HatsNextDialogClosed() {
  hats_next_dialog_exists_ = false;
}

void HatsService::SetSurveyMetadataForTesting(
    const HatsService::SurveyMetadata& metadata) {
  const std::string& trigger = kHatsSurveyTriggerSettings;
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

  if (metadata.any_last_survey_started_time.has_value()) {
    pref_data->SetPath(
        kAnyLastSurveyStartedTimePath,
        util::TimeToValue(*metadata.any_last_survey_started_time));
  } else {
    pref_data->RemovePath(kAnyLastSurveyStartedTimePath);
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
  const std::string& trigger = kHatsSurveyTriggerSettings;
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

  base::Optional<base::Time> any_last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(kAnyLastSurveyStartedTimePath));
  if (any_last_survey_started_time.has_value())
    metadata->any_last_survey_started_time = any_last_survey_started_time;

  base::Optional<bool> is_survey_full =
      pref_data->FindBoolPath(GetIsSurveyFull(trigger));
  if (is_survey_full.has_value())
    metadata->is_survey_full = is_survey_full;

  base::Optional<base::Time> last_survey_check_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value())
    metadata->last_survey_check_time = last_survey_check_time;
}

void HatsService::RemoveTask(const DelayedSurveyTask& task) {
  pending_tasks_.erase(task);
}

bool HatsService::HasPendingTasks() {
  return !pending_tasks_.empty();
}

void HatsService::LaunchSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    const std::map<std::string, bool>& product_specific_data) {
  if (ShouldShowSurvey(trigger) && web_contents &&
      web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    LaunchSurveyForBrowser(chrome::FindBrowserWithWebContents(web_contents),
                           trigger, base::DoNothing(), base::DoNothing(),
                           product_specific_data);
  }
}

void HatsService::LaunchSurveyForBrowser(
    Browser* browser,
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::map<std::string, bool>& product_specific_data) {
  if (!browser ||
      (!browser->is_type_normal() && !browser->is_type_devtools()) ||
      !profiles::IsRegularOrGuestSession(browser)) {
    // Never show HaTS bubble for Incognito mode.
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoNotRegularBrowser);
    std::move(failure_callback).Run();
    return;
  }
  if (IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) ==
      IncognitoModePrefs::DISABLED) {
    // Incognito mode needs to be enabled to create an off-the-record profile
    // for HaTS dialog.
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoIncognitoDisabled);
    std::move(failure_callback).Run();
    return;
  }
  // Checking survey's status could be costly due to a network request, so
  // we check it at the last.
  CheckSurveyStatusAndMaybeShow(browser, trigger, std::move(success_callback),
                                std::move(failure_callback),
                                product_specific_data);
}

bool HatsService::CanShowSurvey(const std::string& trigger) const {
  // Do not show if a survey dialog already exists.
  if (hats_next_dialog_exists_) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoSurveyAlreadyInProgress);
    return false;
  }

  // Survey should not be loaded if the corresponding survey config is
  // unavailable.
  const auto config_iterator = survey_configs_by_triggers_.find(trigger);
  if (config_iterator == survey_configs_by_triggers_.end()) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoTriggerStringMismatch);
    return false;
  }
  const SurveyConfig config = config_iterator->second;

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

  if (!config.user_prompted) {
    if ((now - profile_->GetCreationTime()) < kMinimumProfileAge) {
      UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                                ShouldShowSurveyReasons::kNoProfileTooNew);
      return false;
    }

    base::Optional<base::Time> last_survey_started_time = util::ValueToTime(
        pref_data->FindPath(GetLastSurveyStartedTime(trigger)));
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

    // The time any survey was started will always be equal or more recent than
    // the time a particular survey was started, so it is checked afterwards to
    // improve UMA logging.
    base::Optional<base::Time> last_any_started_time =
        util::ValueToTime(pref_data->FindPath(kAnyLastSurveyStartedTimePath));
    if (last_any_started_time.has_value()) {
      base::TimeDelta elapsed_time_any_started = now - *last_any_started_time;
      if (elapsed_time_any_started < kMinimumTimeBetweenAnySurveyStarts) {
        UMA_HISTOGRAM_ENUMERATION(
            kHatsShouldShowSurveyReasonHistogram,
            ShouldShowSurveyReasons::kNoAnyLastSurveyTooRecent);
        return false;
      }
    }
  }

  // If an attempt to check with the HaTS servers whether a survey should be
  // delivered was made too recently, another survey cannot be shown.
  base::Optional<base::Time> last_survey_check_time =
      util::ValueToTime(pref_data->FindPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value()) {
    base::TimeDelta elapsed_time_since_last_check =
        base::Time::Now() - *last_survey_check_time;
    if (elapsed_time_since_last_check < kMinimumTimeBetweenSurveyChecks)
      return false;
  }

  return true;
}

bool HatsService::ShouldShowSurvey(const std::string& trigger) const {
  if (!CanShowSurvey(trigger))
    return false;

  auto probability = survey_configs_by_triggers_.at(trigger).probability;
  bool should_show_survey = base::RandDouble() < probability;
  if (!should_show_survey) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoBelowProbabilityLimit);
  }

  return should_show_survey;
}

void HatsService::CheckSurveyStatusAndMaybeShow(
    Browser* browser,
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::map<std::string, bool>& product_specific_data) {
  // Check the survey status in profile first.
  // We record the survey's over capacity information in user profile to avoid
  // duplicated checks since the survey won't change once it is full.
  const base::DictionaryValue* pref_data =
      profile_->GetPrefs()->GetDictionary(prefs::kHatsSurveyMetadata);
  base::Optional<int> is_full =
      pref_data->FindBoolPath(GetIsSurveyFull(trigger));
  if (is_full.has_value() && is_full) {
    std::move(failure_callback).Run();
    return;
  }

  DCHECK(survey_configs_by_triggers_.find(trigger) !=
         survey_configs_by_triggers_.end());
  auto survey_config = survey_configs_by_triggers_[trigger];

  // Check that the |product_specific_data| matches the fields for this trigger.
  // If fields are set for a trigger, they must be provided.
  DCHECK_EQ(product_specific_data.size(),
            survey_config.product_specific_data_fields.size());
  for (auto field_value : product_specific_data) {
    DCHECK(std::find(survey_config.product_specific_data_fields.begin(),
                     survey_config.product_specific_data_fields.end(),
                     field_value.first) !=
           survey_config.product_specific_data_fields.end());
  }

  // As soon as the HaTS Next dialog is created it will attempt to contact
  // the HaTS servers to check for a survey.
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  update->SetPath(GetLastSurveyCheckTime(trigger),
                  util::TimeToValue(base::Time::Now()));

  DCHECK(!hats_next_dialog_exists_);
  browser->window()->ShowHatsDialog(
      survey_configs_by_triggers_[trigger].trigger_id,
      std::move(success_callback), std::move(failure_callback),
      product_specific_data);
  hats_next_dialog_exists_ = true;
}
