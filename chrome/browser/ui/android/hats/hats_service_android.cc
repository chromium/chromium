// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/hats_service_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/android/hats/survey_client_android.h"
#include "chrome/browser/ui/android/hats/survey_ui_delegate_android.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_wrapper.h"
#include "components/resources/android/theme_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

constexpr char kHatsShouldShowSurveyReasonAndroidHistogram[] =
    "Feedback.HappinessTrackingSurvey.ShouldShowSurveyReasonAndroid";

HatsServiceAndroid::DelayedSurveyTask::DelayedSurveyTask(
    HatsServiceAndroid* hats_service,
    const std::string& trigger,
    content::WebContents* web_contents,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::optional<std::string>& supplied_trigger_id,
    const SurveyOptions& survey_options)
    : hats_service_(hats_service),
      trigger_(trigger),
      product_specific_bits_data_(product_specific_bits_data),
      product_specific_string_data_(product_specific_string_data),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)),
      supplied_trigger_id_(supplied_trigger_id),
      survey_options_(survey_options) {
  Observe(web_contents);
}

HatsServiceAndroid::DelayedSurveyTask::~DelayedSurveyTask() = default;

void HatsServiceAndroid::DelayedSurveyTask::Launch() {
  CHECK(web_contents());
  if (!web_contents() ||
      web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    return;
  }

  message_ = std::make_unique<messages::MessageWrapper>(
      survey_options_.message_identifier.value_or(
          messages::MessageIdentifier::CHROME_SURVEY),
      std::move(success_callback_),
      base::BindOnce(&HatsServiceAndroid::DelayedSurveyTask::DismissCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  if (survey_options_.custom_invitation.has_value()) {
    message_->SetTitle(survey_options_.custom_invitation.value());
  }

  hats::SurveyUiDelegateAndroid delegate(
      message_.get(), web_contents()->GetTopLevelNativeWindow());

  // Create survey client with delegate.
  hats::SurveyClientAndroid survey_client(
      trigger_, &delegate, hats_service_->profile(), supplied_trigger_id_);
  survey_client.LaunchSurvey(web_contents()->GetTopLevelNativeWindow(),
                             product_specific_bits_data_,
                             product_specific_string_data_);
}

void HatsServiceAndroid::DelayedSurveyTask::DismissCallback(
    messages::DismissReason dismiss_reason) {
  if (dismiss_reason != messages::DismissReason::PRIMARY_ACTION &&
      !failure_callback_.is_null()) {
    std::move(failure_callback_).Run();
  }

  ShouldShowSurveyReasonsAndroid reason;
  switch (dismiss_reason) {
    case messages::DismissReason::UNKNOWN:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidUnknown;
      break;
    case messages::DismissReason::PRIMARY_ACTION:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidAccepted;
      break;
    case messages::DismissReason::SECONDARY_ACTION:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidSecondaryAction;
      break;
    case messages::DismissReason::TIMER:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidExpired;
      break;
    case messages::DismissReason::GESTURE:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidDismissedByGesture;
      break;
    case messages::DismissReason::TAB_SWITCHED:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidTabSwitched;
      break;
    case messages::DismissReason::TAB_DESTROYED:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidTabDestroyed;
      break;
    case messages::DismissReason::ACTIVITY_DESTROYED:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidActivityDestroyed;
      break;
    case messages::DismissReason::SCOPE_DESTROYED:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidScopeDestroyed;
      break;
    case messages::DismissReason::DISMISSED_BY_FEATURE:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidDismissedByFeature;
      break;
    case messages::DismissReason::COUNT:
      reason = ShouldShowSurveyReasonsAndroid::kAndroidUnknown;
      NOTREACHED_IN_MIGRATION();
  }
  UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonAndroidHistogram,
                            reason);
  hats_service_->RemoveTask(*this);
}

void HatsServiceAndroid::DelayedSurveyTask::WebContentsDestroyed() {
  if (!failure_callback_.is_null()) {
    std::move(failure_callback_).Run();
  }
  hats_service_->RemoveTask(*this);
}

base::WeakPtr<HatsServiceAndroid::DelayedSurveyTask>
HatsServiceAndroid::DelayedSurveyTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

HatsServiceAndroid::HatsServiceAndroid(Profile* profile)
    : HatsService(profile) {}

HatsServiceAndroid::~HatsServiceAndroid() = default;

void HatsServiceAndroid::LaunchSurvey(
    const std::string& trigger,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  NOTIMPLEMENTED();
}

void HatsServiceAndroid::LaunchSurveyForWebContents(
    const std::string& trigger,
    content::WebContents* web_contents,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const std::optional<std::string>& supplied_trigger_id,
    const SurveyOptions& survey_options) {
  // By using a delayed survey with a delay of 0, we can centralize the object
  // lifecycle management duties for native clank survey triggers.
  LaunchDelayedSurveyForWebContents(
      trigger, web_contents, 0, product_specific_bits_data,
      product_specific_string_data, HatsService::NavigationBehaviour::ALLOW_ANY,
      std::move(success_callback), std::move(failure_callback),
      supplied_trigger_id, survey_options);
}

bool HatsServiceAndroid::LaunchDelayedSurvey(
    const std::string& trigger,
    int timeout_ms,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  NOTIMPLEMENTED();
  return false;
}

bool HatsServiceAndroid::LaunchDelayedSurveyForWebContents(
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
  CHECK(web_contents);
  CHECK(navigation_behaviour ==
        NavigationBehaviour::ALLOW_ANY);  // Currently only ALLOW_ANY is
                                          // supported on Android
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (survey_configs_by_triggers_.find(trigger) ==
      survey_configs_by_triggers_.end()) {
    // Survey configuration is not available.
    if (!failure_callback.is_null()) {
      std::move(failure_callback).Run();
    }
    return false;
  }
  auto result = pending_tasks_.emplace(
      this, trigger, web_contents, product_specific_bits_data,
      product_specific_string_data, std::move(success_callback),
      std::move(failure_callback), supplied_trigger_id, survey_options);
  if (!result.second) {
    return false;
  }
  auto success =
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&HatsServiceAndroid::DelayedSurveyTask::Launch,
                         const_cast<HatsServiceAndroid::DelayedSurveyTask&>(
                             *(result.first))
                             .GetWeakPtr()),
          base::Milliseconds(timeout_ms));
  if (!success) {
    pending_tasks_.erase(result.first);
  }
  return success;
}

bool HatsServiceAndroid::CanShowAnySurvey(bool user_prompted) const {
  NOTIMPLEMENTED();  // Survey throttling happens on the clank side
  return false;
}

bool HatsServiceAndroid::CanShowSurvey(const std::string& trigger) const {
  NOTIMPLEMENTED();  // Survey throttling happens on the clank side
  return false;
}

void HatsServiceAndroid::RecordSurveyAsShown(std::string trigger_id) {
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

  UMA_HISTOGRAM_ENUMERATION(kHatsShouldShowSurveyReasonAndroidHistogram,
                            ShouldShowSurveyReasonsAndroid::kYes);
}

void HatsServiceAndroid::RemoveTask(const DelayedSurveyTask& task) {
  pending_tasks_.erase(task);
}
