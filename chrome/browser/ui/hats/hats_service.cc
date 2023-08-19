// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"

constexpr char kHatsShouldShowSurveyReasonHistogram[] =
    "Feedback.HappinessTrackingSurvey.ShouldShowSurveyReason";

namespace {

// TODO(crbug.com/1160661): When the minimum time between any survey, and the
// minimum time between a specific survey, are the same, the logic supporting
// the latter check is superfluous.
constexpr base::TimeDelta kMinimumTimeBetweenSurveyStarts = base::Days(180);

constexpr base::TimeDelta kMinimumTimeBetweenAnySurveyStarts = base::Days(180);

constexpr base::TimeDelta kMinimumTimeBetweenSurveyChecks = base::Days(1);

constexpr base::TimeDelta kMinimumProfileAge = base::Days(30);

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

}  // namespace

HatsService::SurveyMetadata::SurveyMetadata() = default;

HatsService::SurveyMetadata::~SurveyMetadata() = default;

HatsService::DelayedSurveyTask::DelayedSurveyTask(
    HatsService* hats_service,
    const std::string& trigger,
    content::WebContents* web_contents,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    bool require_same_origin)
    : hats_service_(hats_service),
      trigger_(trigger),
      product_specific_bits_data_(product_specific_bits_data),
      product_specific_string_data_(product_specific_string_data),
      require_same_origin_(require_same_origin) {
  Observe(web_contents);
}

HatsService::DelayedSurveyTask::~DelayedSurveyTask() = default;

base::WeakPtr<HatsService::DelayedSurveyTask>
HatsService::DelayedSurveyTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HatsService::DelayedSurveyTask::Launch() {
  hats_service_->LaunchSurveyForWebContents(trigger_, web_contents(),
                                            product_specific_bits_data_,
                                            product_specific_string_data_);
  hats_service_->RemoveTask(*this);
}

void HatsService::DelayedSurveyTask::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!require_same_origin_ || !navigation_handle ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      (navigation_handle->HasCommitted() &&
       navigation_handle->IsSameOrigin())) {
    return;
  }

  hats_service_->RemoveTask(*this);
}

void HatsService::DelayedSurveyTask::WebContentsDestroyed() {
  hats_service_->RemoveTask(*this);
}

HatsService::HatsService(Profile* profile) : profile_(profile) {
  hats::GetActiveSurveyConfigs(survey_configs_by_triggers_);
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
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  if (!ShouldShowSurvey(trigger)) {
    std::move(failure_callback).Run();
    return;
  }

  LaunchSurveyForBrowser(
      chrome::FindLastActiveWithProfile(profile_), trigger,
      std::move(success_callback), std::move(failure_callback),
      product_specific_bits_data, product_specific_string_data);
}

bool HatsService::LaunchDelayedSurvey(
    const std::string& trigger,
    int timeout_ms,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  return base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HatsService::LaunchSurvey, weak_ptr_factory_.GetWeakPtr(),
                     trigger, base::DoNothing(), base::DoNothing(),
                     product_specific_bits_data, product_specific_string_data),
      base::Milliseconds(timeout_ms));
}

bool HatsService::LaunchDelayedSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    int timeout_ms,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    bool require_same_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return false;
  }
  auto result = pending_tasks_.emplace(
      this, trigger, web_contents, product_specific_bits_data,
      product_specific_string_data, require_same_origin);
  if (!result.second) {
    return false;
  }
  auto success =
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &HatsService::DelayedSurveyTask::Launch,
              const_cast<HatsService::DelayedSurveyTask&>(*(result.first))
                  .GetWeakPtr()),
          base::Milliseconds(timeout_ms));
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
  auto trigger_survey_config =
      base::ranges::find(survey_configs_by_triggers_, trigger_id,
                         [](const SurveyConfigs::value_type& pair) {
                           return pair.second.trigger_id;
                         });

  DCHECK(trigger_survey_config != survey_configs_by_triggers_.end());
  std::string trigger = trigger_survey_config->first;

  UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                            ShouldShowSurveyReasons::kYes);

  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::Value::Dict& pref_data = update.Get();
  pref_data.SetByDottedPath(
      GetMajorVersionPath(trigger),
      static_cast<int>(version_info::GetVersion().components()[0]));
  pref_data.SetByDottedPath(GetLastSurveyStartedTime(trigger),
                            base::TimeToValue(base::Time::Now()));
  pref_data.SetByDottedPath(kAnyLastSurveyStartedTimePath,
                            base::TimeToValue(base::Time::Now()));
}

void HatsService::HatsNextDialogClosed() {
  hats_next_dialog_exists_ = false;
}

void HatsService::SetSurveyMetadataForTesting(
    const HatsService::SurveyMetadata& metadata) {
  const std::string& trigger = kHatsSurveyTriggerSettings;
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::Value::Dict& pref_data = update.Get();
  if (!metadata.last_major_version.has_value() &&
      !metadata.last_survey_started_time.has_value() &&
      !metadata.is_survey_full.has_value() &&
      !metadata.last_survey_check_time.has_value()) {
    pref_data.RemoveByDottedPath(trigger);
  }

  if (metadata.last_major_version.has_value()) {
    pref_data.SetByDottedPath(GetMajorVersionPath(trigger),
                              *metadata.last_major_version);
  } else {
    pref_data.RemoveByDottedPath(GetMajorVersionPath(trigger));
  }

  if (metadata.last_survey_started_time.has_value()) {
    pref_data.SetByDottedPath(
        GetLastSurveyStartedTime(trigger),
        base::TimeToValue(*metadata.last_survey_started_time));
  } else {
    pref_data.RemoveByDottedPath(GetLastSurveyStartedTime(trigger));
  }

  if (metadata.any_last_survey_started_time.has_value()) {
    pref_data.SetByDottedPath(
        kAnyLastSurveyStartedTimePath,
        base::TimeToValue(*metadata.any_last_survey_started_time));
  } else {
    pref_data.RemoveByDottedPath(kAnyLastSurveyStartedTimePath);
  }

  if (metadata.is_survey_full.has_value()) {
    pref_data.SetByDottedPath(GetIsSurveyFull(trigger),
                              *metadata.is_survey_full);
  } else {
    pref_data.RemoveByDottedPath(GetIsSurveyFull(trigger));
  }

  if (metadata.last_survey_check_time.has_value()) {
    pref_data.SetByDottedPath(
        GetLastSurveyCheckTime(trigger),
        base::TimeToValue(*metadata.last_survey_check_time));
  } else {
    pref_data.RemoveByDottedPath(GetLastSurveyCheckTime(trigger));
  }
}

void HatsService::GetSurveyMetadataForTesting(
    HatsService::SurveyMetadata* metadata) const {
  const std::string& trigger = kHatsSurveyTriggerSettings;
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  base::Value::Dict& pref_data = update.Get();

  absl::optional<int> last_major_version =
      pref_data.FindIntByDottedPath(GetMajorVersionPath(trigger));
  if (last_major_version.has_value()) {
    metadata->last_major_version = last_major_version;
  }

  absl::optional<base::Time> last_survey_started_time = base::ValueToTime(
      pref_data.FindByDottedPath(GetLastSurveyStartedTime(trigger)));
  if (last_survey_started_time.has_value()) {
    metadata->last_survey_started_time = last_survey_started_time;
  }

  absl::optional<base::Time> any_last_survey_started_time = base::ValueToTime(
      pref_data.FindByDottedPath(kAnyLastSurveyStartedTimePath));
  if (any_last_survey_started_time.has_value()) {
    metadata->any_last_survey_started_time = any_last_survey_started_time;
  }

  absl::optional<bool> is_survey_full =
      pref_data.FindBoolByDottedPath(GetIsSurveyFull(trigger));
  if (is_survey_full.has_value()) {
    metadata->is_survey_full = is_survey_full;
  }

  absl::optional<base::Time> last_survey_check_time = base::ValueToTime(
      pref_data.FindByDottedPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value()) {
    metadata->last_survey_check_time = last_survey_check_time;
  }
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
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  if (ShouldShowSurvey(trigger) && web_contents &&
      web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    LaunchSurveyForBrowser(chrome::FindBrowserWithWebContents(web_contents),
                           trigger, base::DoNothing(), base::DoNothing(),
                           product_specific_bits_data,
                           product_specific_string_data);
  }
}

void HatsService::LaunchSurveyForBrowser(
    Browser* browser,
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
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
      policy::IncognitoModeAvailability::kDisabled) {
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
                                product_specific_bits_data,
                                product_specific_string_data);
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
  const hats::SurveyConfig config = config_iterator->second;

  // Always show the survey in demo mode. This check is duplicated in
  // CanShowAnySurvey, but because of the semantics of that function, must be
  // included here.
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    return true;
  }

  if (!CanShowAnySurvey(config.user_prompted)) {
    return false;
  }

  // Survey can not be loaded and shown if there is no network connection.
  if (net::NetworkChangeNotifier::IsOffline()) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoOffline);
    return false;
  }

  const base::Value::Dict& pref_data =
      profile_->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);
  absl::optional<int> last_major_version =
      pref_data.FindIntByDottedPath(GetMajorVersionPath(trigger));
  if (last_major_version.has_value() &&
      static_cast<uint32_t>(*last_major_version) ==
          version_info::GetVersion().components()[0]) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoReceivedSurveyInCurrentMilestone);
    return false;
  }

  if (!config.user_prompted) {
    absl::optional<base::Time> last_survey_started_time = base::ValueToTime(
        pref_data.FindByDottedPath(GetLastSurveyStartedTime(trigger)));
    if (last_survey_started_time.has_value()) {
      base::TimeDelta elapsed_time_since_last_start =
          base::Time::Now() - *last_survey_started_time;
      if (elapsed_time_since_last_start < kMinimumTimeBetweenSurveyStarts) {
        UMA_HISTOGRAM_ENUMERATION(
            kHatsShouldShowSurveyReasonHistogram,
            ShouldShowSurveyReasons::kNoLastSurveyTooRecent);
        return false;
      }
    }
  }

  // If an attempt to check with the HaTS servers whether a survey should be
  // delivered was made too recently, another survey cannot be shown.
  absl::optional<base::Time> last_survey_check_time = base::ValueToTime(
      pref_data.FindByDottedPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value()) {
    base::TimeDelta elapsed_time_since_last_check =
        base::Time::Now() - *last_survey_check_time;
    if (elapsed_time_since_last_check < kMinimumTimeBetweenSurveyChecks) {
      return false;
    }
  }

  return true;
}

bool HatsService::CanShowAnySurvey(bool user_prompted) const {
  // Surveys can always be shown in Demo mode.
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    return true;
  }

  // HaTS requires metrics consent to run. This is also how HaTS can be disabled
  // by policy.
  if (!g_browser_process->GetMetricsServicesManager()
           ->IsMetricsConsentGiven()) {
    return false;
  }

  // Do not show surveys if Chrome's last exit was a crash. This avoids
  // biasing survey results unnecessarily.
  if (ExitTypeService::GetLastSessionExitType(profile_) == ExitType::kCrashed) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoLastSessionCrashed);
    return false;
  }

  // Some surveys may be "user prompted", which means the user has already been
  // asked in context if they would like to take a survey (in a less
  // confrontational manner than the standard HaTS prompt). The bar for whether
  // a user is eligible is thus lower for these types of surveys.
  if (!user_prompted) {
    const base::Value::Dict& pref_data =
        profile_->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);

    // If the profile is too new, measured as the age of the profile directory,
    // the user is ineligible.
    base::Time now = base::Time::Now();
    if ((now - profile_->GetCreationTime()) < kMinimumProfileAge) {
      UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                                ShouldShowSurveyReasons::kNoProfileTooNew);
      return false;
    }

    // If a user has received any HaTS survey too recently, they are also
    // ineligible.
    absl::optional<base::Time> last_any_started_time =
        base::ValueToTime(pref_data.Find(kAnyLastSurveyStartedTimePath));
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

  return true;
}

bool HatsService::ShouldShowSurvey(const std::string& trigger) const {
  if (!CanShowSurvey(trigger)) {
    return false;
  }

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
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  // Check the survey status in profile first.
  // We record the survey's over capacity information in user profile to avoid
  // duplicated checks since the survey won't change once it is full.
  const base::Value::Dict& pref_data =
      profile_->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);
  absl::optional<int> is_full =
      pref_data.FindBoolByDottedPath(GetIsSurveyFull(trigger));
  if (is_full.has_value() && is_full) {
    std::move(failure_callback).Run();
    return;
  }

  CHECK(survey_configs_by_triggers_.find(trigger) !=
        survey_configs_by_triggers_.end());
  auto survey_config = survey_configs_by_triggers_[trigger];

  // Check that the |product_specific_bits_data| matches the fields for this
  // trigger. If fields are set for a trigger, they must be provided.
  CHECK_EQ(product_specific_bits_data.size(),
           survey_config.product_specific_bits_data_fields.size());
  for (auto field_value : product_specific_bits_data) {
    CHECK(base::Contains(survey_config.product_specific_bits_data_fields,
                         field_value.first));
  }

  // Check that the |product_specific_string_data| matches the fields for this
  // trigger. If fields are set for a trigger, they must be provided.
  CHECK_EQ(product_specific_string_data.size(),
           survey_config.product_specific_string_data_fields.size());
  for (auto field_value : product_specific_string_data) {
    CHECK(base::Contains(survey_config.product_specific_string_data_fields,
                         field_value.first));
  }

  // As soon as the HaTS Next dialog is created it will attempt to contact
  // the HaTS servers to check for a survey.
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kHatsSurveyMetadata);
  update->SetByDottedPath(GetLastSurveyCheckTime(trigger),
                          base::TimeToValue(base::Time::Now()));

  DCHECK(!hats_next_dialog_exists_);
  browser->window()->ShowHatsDialog(
      survey_configs_by_triggers_[trigger].trigger_id,
      std::move(success_callback), std::move(failure_callback),
      product_specific_bits_data, product_specific_string_data);
  hats_next_dialog_exists_ = true;
}
