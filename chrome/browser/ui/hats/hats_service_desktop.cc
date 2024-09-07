// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service_desktop.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/hats/hats_service.h"
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

// TODO(crbug.com/40162245): When the minimum time between any survey, and the
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

HatsServiceDesktop::DelayedSurveyTask::DelayedSurveyTask(
    HatsServiceDesktop* hats_service,
    std::string trigger,
    content::WebContents* web_contents,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    NavigationBehaviour navigation_behaviour,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    std::optional<std::string_view> supplied_trigger_id)
    : hats_service_(hats_service),
      trigger_(trigger),
      product_specific_bits_data_(product_specific_bits_data),
      product_specific_string_data_(product_specific_string_data),
      navigation_behaviour_(navigation_behaviour),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)),
      supplied_trigger_id_(std::move(supplied_trigger_id)) {
  Observe(web_contents);
}

HatsServiceDesktop::DelayedSurveyTask::~DelayedSurveyTask() = default;

base::WeakPtr<HatsServiceDesktop::DelayedSurveyTask>
HatsServiceDesktop::DelayedSurveyTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HatsServiceDesktop::DelayedSurveyTask::Launch() {
  CHECK(web_contents());
  if (web_contents() &&
      web_contents()->GetVisibility() == content::Visibility::VISIBLE) {
    hats_service_->LaunchSurveyForWebContents(
        trigger_, web_contents(), product_specific_bits_data_,
        product_specific_string_data_, std::move(success_callback_),
        std::move(failure_callback_), supplied_trigger_id_);
    hats_service_->RemoveTask(*this);
  }
}

void HatsServiceDesktop::DelayedSurveyTask::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_behaviour_ == NavigationBehaviour::ALLOW_ANY ||
      !navigation_handle || !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_behaviour_ == NavigationBehaviour::REQUIRE_SAME_DOCUMENT &&
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (navigation_behaviour_ == NavigationBehaviour::REQUIRE_SAME_ORIGIN &&
      navigation_handle->HasCommitted() && navigation_handle->IsSameOrigin()) {
    return;
  }

  if (!failure_callback_.is_null()) {
    std::move(failure_callback_).Run();
  }
  hats_service_->RemoveTask(*this);
}

void HatsServiceDesktop::DelayedSurveyTask::WebContentsDestroyed() {
  if (!failure_callback_.is_null()) {
    std::move(failure_callback_).Run();
  }
  hats_service_->RemoveTask(*this);
}

HatsServiceDesktop::HatsServiceDesktop(Profile* profile)
    : HatsService(profile) {}

HatsServiceDesktop::~HatsServiceDesktop() = default;

// static
void HatsServiceDesktop::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kHatsSurveyMetadata,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void HatsServiceDesktop::LaunchSurvey(
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  if (!ShouldShowSurvey(trigger)) {
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
    return;
  }
  LaunchSurveyForBrowser(
      chrome::FindLastActiveWithProfile(profile()), trigger,
      std::move(success_callback), std::move(failure_callback),
      product_specific_bits_data, product_specific_string_data);
}

void HatsServiceDesktop::LaunchSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::optional<std::string>& supplied_trigger_id,
    const SurveyOptions& survey_options) {
  CHECK(!survey_options.custom_invitation.has_value() &&
        !survey_options.message_identifier.has_value())
      << "Custom invitation strings and message types are not supported on "
         "desktop.";
  if (ShouldShowSurvey(trigger) && web_contents &&
      web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    LaunchSurveyForBrowser(chrome::FindBrowserWithTab(web_contents), trigger,
                           std::move(success_callback),
                           std::move(failure_callback),
                           product_specific_bits_data,
                           product_specific_string_data, supplied_trigger_id);
  }
}

bool HatsServiceDesktop::LaunchDelayedSurvey(
    const std::string& trigger,
    int timeout_ms,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  return base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HatsServiceDesktop::LaunchSurvey,
                     weak_ptr_factory_.GetWeakPtr(), trigger, base::DoNothing(),
                     base::DoNothing(), product_specific_bits_data,
                     product_specific_string_data),
      base::Milliseconds(timeout_ms));
}

bool HatsServiceDesktop::LaunchDelayedSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    int timeout_ms,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    NavigationBehaviour navigation_behaviour,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::optional<std::string>& supplied_trigger_id,
    const SurveyOptions& survey_options) {
  CHECK(!survey_options.custom_invitation.has_value() &&
        !survey_options.message_identifier.has_value())
      << "Custom invitation strings and message types are not supported on "
         "desktop.";
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (survey_configs_by_triggers_.find(trigger) ==
      survey_configs_by_triggers_.end()) {
    // Survey configuration is not available.
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
    return false;
  }
  if (!web_contents) {
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
    return false;
  }
  auto result = pending_tasks_.emplace(
      this, trigger, web_contents, product_specific_bits_data,
      product_specific_string_data, navigation_behaviour,
      std::move(success_callback), std::move(failure_callback),
      supplied_trigger_id);
  if (!result.second) {
    return false;
  }
  auto success =
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&HatsServiceDesktop::DelayedSurveyTask::Launch,
                         const_cast<HatsServiceDesktop::DelayedSurveyTask&>(
                             *(result.first))
                             .GetWeakPtr()),
          base::Milliseconds(timeout_ms));
  if (!success) {
    pending_tasks_.erase(result.first);
  }
  return success;
}

void HatsServiceDesktop::SetSurveyMetadataForTesting(
    const HatsService::SurveyMetadata& metadata) {
  const std::string& trigger = kHatsSurveyTriggerSettings;
  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kHatsSurveyMetadata);
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

void HatsServiceDesktop::GetSurveyMetadataForTesting(
    HatsService::SurveyMetadata* metadata) const {
  const std::string& trigger = kHatsSurveyTriggerSettings;
  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kHatsSurveyMetadata);
  base::Value::Dict& pref_data = update.Get();

  std::optional<int> last_major_version =
      pref_data.FindIntByDottedPath(GetMajorVersionPath(trigger));
  if (last_major_version.has_value()) {
    metadata->last_major_version = last_major_version;
  }

  std::optional<base::Time> last_survey_started_time = base::ValueToTime(
      pref_data.FindByDottedPath(GetLastSurveyStartedTime(trigger)));
  if (last_survey_started_time.has_value()) {
    metadata->last_survey_started_time = last_survey_started_time;
  }

  std::optional<base::Time> any_last_survey_started_time = base::ValueToTime(
      pref_data.FindByDottedPath(kAnyLastSurveyStartedTimePath));
  if (any_last_survey_started_time.has_value()) {
    metadata->any_last_survey_started_time = any_last_survey_started_time;
  }

  std::optional<bool> is_survey_full =
      pref_data.FindBoolByDottedPath(GetIsSurveyFull(trigger));
  if (is_survey_full.has_value()) {
    metadata->is_survey_full = is_survey_full;
  }

  std::optional<base::Time> last_survey_check_time = base::ValueToTime(
      pref_data.FindByDottedPath(GetLastSurveyCheckTime(trigger)));
  if (last_survey_check_time.has_value()) {
    metadata->last_survey_check_time = last_survey_check_time;
  }
}

bool HatsServiceDesktop::HasPendingTasks() {
  return !pending_tasks_.empty();
}

bool HatsServiceDesktop::CanShowSurvey(const std::string& trigger) const {
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

  // Do not show if a survey dialog already exists.
  if (hats_next_dialog_exists_) {
    UMA_HISTOGRAM_ENUMERATION(
        kHatsShouldShowSurveyReasonHistogram,
        ShouldShowSurveyReasons::kNoSurveyAlreadyInProgress);
    return false;
  }

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
      profile()->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);
  std::optional<int> last_major_version =
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
    std::optional<base::Time> last_survey_started_time = base::ValueToTime(
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
  std::optional<base::Time> last_survey_check_time = base::ValueToTime(
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

bool HatsServiceDesktop::CanShowAnySurvey(bool user_prompted) const {
  // HaTS requires metrics consent to run. This is also how HaTS can be
  // disabled by policy.
  if (!g_browser_process->GetMetricsServicesManager()
           ->IsMetricsConsentGiven()) {
    return false;
  }

  // HaTs can also be disabled by policy if metrics consent is given.
  if (!profile()->GetPrefs()->GetBoolean(
          policy::policy_prefs::kFeedbackSurveysEnabled)) {
    return false;
  }

  // Surveys can always be shown in Demo mode.
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    return true;
  }

  // Do not show surveys if Chrome's last exit was a crash. This avoids
  // biasing survey results unnecessarily.
  if (ExitTypeService::GetLastSessionExitType(profile()) ==
      ExitType::kCrashed) {
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoLastSessionCrashed);
    return false;
  }

  // Some surveys may be "user prompted", which means the user has already
  // been asked in context if they would like to take a survey (in a less
  // confrontational manner than the standard HaTS prompt). The bar for
  // whether a user is eligible is thus lower for these types of surveys.
  if (!user_prompted) {
    const base::Value::Dict& pref_data =
        profile()->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);

    // If the profile is too new, measured as the age of the profile
    // directory, the user is ineligible.
    base::Time now = base::Time::Now();
    if ((now - profile()->GetCreationTime()) < kMinimumProfileAge) {
      UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                                ShouldShowSurveyReasons::kNoProfileTooNew);
      return false;
    }

    // If a user has received any HaTS survey too recently, they are also
    // ineligible.
    std::optional<base::Time> last_any_started_time =
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

void HatsServiceDesktop::RecordSurveyAsShown(std::string trigger_id) {
  // Record the trigger associated with the trigger_id. This is recorded
  // instead of the trigger ID itself, as the ID is specific to individual
  // survey versions. There should be a cooldown before a user is prompted to
  // take a survey from the same trigger, regardless of whether the survey was
  // updated.
  auto trigger_survey_config =
      base::ranges::find(survey_configs_by_triggers_, trigger_id,
                         [](const SurveyConfigs::value_type& pair) {
                           return pair.second.trigger_id;
                         });

  CHECK(trigger_survey_config != survey_configs_by_triggers_.end(),
        base::NotFatalUntil::M130);
  std::string trigger = trigger_survey_config->first;

  UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                            ShouldShowSurveyReasons::kYes);

  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kHatsSurveyMetadata);
  base::Value::Dict& pref_data = update.Get();
  pref_data.SetByDottedPath(
      GetMajorVersionPath(trigger),
      static_cast<int>(version_info::GetVersion().components()[0]));
  pref_data.SetByDottedPath(GetLastSurveyStartedTime(trigger),
                            base::TimeToValue(base::Time::Now()));
  pref_data.SetByDottedPath(kAnyLastSurveyStartedTimePath,
                            base::TimeToValue(base::Time::Now()));
}

void HatsServiceDesktop::HatsNextDialogClosed() {
  hats_next_dialog_exists_ = false;
}

void HatsServiceDesktop::RemoveTask(const DelayedSurveyTask& task) {
  pending_tasks_.erase(task);
}

bool HatsServiceDesktop::ShouldShowSurvey(const std::string& trigger) const {
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

void HatsServiceDesktop::LaunchSurveyForBrowser(
    Browser* browser,
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    const std::optional<std::string_view>& supplied_trigger_id) {
  if (!browser ||
      (!browser->is_type_normal() && !browser->is_type_devtools()) ||
      !profiles::IsRegularOrGuestSession(browser)) {
    // Never show HaTS bubble for Incognito mode.
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoNotRegularBrowser);
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
    return;
  }
  if (IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) ==
      policy::IncognitoModeAvailability::kDisabled) {
    // Incognito mode needs to be enabled to create an off-the-record profile
    // for HaTS dialog.
    UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonHistogram,
                              ShouldShowSurveyReasons::kNoIncognitoDisabled);
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
    return;
  }
  // Checking survey's status could be costly due to a network request, so
  // we check it at the last.
  CheckSurveyStatusAndMaybeShow(
      browser, trigger, std::move(success_callback),
      std::move(failure_callback), product_specific_bits_data,
      product_specific_string_data, supplied_trigger_id);
}

void HatsServiceDesktop::CheckSurveyStatusAndMaybeShow(
    Browser* browser,
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    const std::optional<std::string_view>& supplied_trigger_id) {
  // Check the survey status in profile first.
  // We record the survey's over capacity information in user profile to avoid
  // duplicated checks since the survey won't change once it is full.
  const base::Value::Dict& pref_data =
      profile()->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);
  std::optional<int> is_full =
      pref_data.FindBoolByDottedPath(GetIsSurveyFull(trigger));
  if (is_full.has_value() && is_full) {
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
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
  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kHatsSurveyMetadata);
  update->SetByDottedPath(GetLastSurveyCheckTime(trigger),
                          base::TimeToValue(base::Time::Now()));

  DCHECK(!hats_next_dialog_exists_);
  if (supplied_trigger_id.has_value()) {
    survey_configs_by_triggers_[trigger].trigger_id =
        std::string(supplied_trigger_id.value());
  }
  browser->window()->ShowHatsDialog(
      survey_configs_by_triggers_[trigger].trigger_id,
      survey_configs_by_triggers_[trigger].hats_histogram_name,
      survey_configs_by_triggers_[trigger].hats_survey_ukm_id,
      std::move(success_callback), std::move(failure_callback),
      product_specific_bits_data, product_specific_string_data);
  hats_next_dialog_exists_ = true;
}
