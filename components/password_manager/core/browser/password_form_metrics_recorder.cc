// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/default_clock.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

PasswordFormMetricsRecorder::BubbleDismissalReason GetBubbleDismissalReason(
    metrics_util::UIDismissalReason ui_dismissal_reason) {
  using BubbleDismissalReason =
      PasswordFormMetricsRecorder::BubbleDismissalReason;
  switch (ui_dismissal_reason) {
    // Accepted by user.
    case metrics_util::CLICKED_ACCEPT:
      return BubbleDismissalReason::kAccepted;

    // Declined by user.
    case metrics_util::CLICKED_CANCEL:
    case metrics_util::CLICKED_NEVER:
      return BubbleDismissalReason::kDeclined;

    // Ignored by user.
    case metrics_util::NO_DIRECT_INTERACTION:
      return BubbleDismissalReason::kIgnored;

    // Ignore these for metrics collection:
    case metrics_util::CLICKED_MANAGE:
    case metrics_util::CLICKED_MANAGE_PASSWORD:
    case metrics_util::CLICKED_PASSWORDS_DASHBOARD:
    case metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT:
    case metrics_util::CLICKED_GOT_IT:
      break;

    // These should not reach here:
    case metrics_util::CLICKED_DONE_OBSOLETE:
    case metrics_util::CLICKED_OK_OBSOLETE:
    case metrics_util::CLICKED_UNBLOCKLIST_OBSOLETE:
    case metrics_util::CLICKED_CREDENTIAL_OBSOLETE:
    case metrics_util::AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE:
    case metrics_util::CLICKED_BRAND_NAME_OBSOLETE:
    case metrics_util::NUM_UI_RESPONSES:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return BubbleDismissalReason::kUnknown;
}

bool HasGeneratedPassword(
    std::optional<PasswordFormMetricsRecorder::GeneratedPasswordStatus>
        status) {
  return status.has_value() &&
         (status == PasswordFormMetricsRecorder::GeneratedPasswordStatus::
                        kPasswordAccepted ||
          status == PasswordFormMetricsRecorder::GeneratedPasswordStatus::
                        kPasswordEdited);
}

// Contains information whether saved username/password were filled or typed.
struct UsernamePasswordsState {
  bool saved_password_typed = false;
  bool saved_username_typed = false;
  bool password_manually_filled = false;
  bool username_manually_filled = false;
  bool password_automatically_filled = false;
  bool username_automatically_filled = false;
  bool unknown_password_typed = false;
  bool unknown_username_typed = false;

  bool password_exists_in_profile_store = false;
  bool password_exists_in_account_store = false;
  bool username_exists_in_profile_store = false;
  bool username_exists_in_account_store = false;

  bool IsPasswordFilled() {
    return password_automatically_filled || password_manually_filled;
  }

  bool IsUsernameFilled() {
    return username_automatically_filled || username_manually_filled;
  }
};

// Calculates whether saved usernames/passwords were filled or typed in
// |submitted_form|.
UsernamePasswordsState CalculateUsernamePasswordsState(
    const FormData& submitted_form,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_usernames,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_passwords) {
  UsernamePasswordsState result;

  for (const FormFieldData& field : submitted_form.fields()) {
    const std::u16string& value =
        field.user_input().empty() ? field.value() : field.user_input();

    bool user_typed =
        field.properties_mask() & FieldPropertiesFlags::kUserTyped;
    bool manually_filled = field.properties_mask() &
                           FieldPropertiesFlags::kAutofilledOnUserTrigger;
    bool automatically_filled =
        field.properties_mask() & FieldPropertiesFlags::kAutofilledOnPageLoad;

    // The typed `value` could appear in `saved_usernames`, `saved_passwords`,
    // or both. In the last case we use the control type of the form as a
    // tie-break, if this is `password`, the user likely typed a password,
    // otherwise a username.
    bool is_possibly_saved_username_in_profile_store = base::Contains(
        saved_usernames,
        std::make_pair(value, PasswordForm::Store::kProfileStore));
    bool is_possibly_saved_username_in_account_store = base::Contains(
        saved_usernames,
        std::make_pair(value, PasswordForm::Store::kAccountStore));
    bool is_possibly_saved_username =
        is_possibly_saved_username_in_profile_store ||
        is_possibly_saved_username_in_account_store;

    bool is_possibly_saved_password_in_profile_store = base::Contains(
        saved_passwords,
        std::make_pair(value, PasswordForm::Store::kProfileStore));
    bool is_possibly_saved_password_in_account_store = base::Contains(
        saved_passwords,
        std::make_pair(value, PasswordForm::Store::kAccountStore));
    bool is_possibly_saved_password =
        is_possibly_saved_password_in_profile_store ||
        is_possibly_saved_password_in_account_store;

    bool field_has_password_type =
        field.form_control_type() == autofill::FormControlType::kInputPassword;

    if (is_possibly_saved_username &&
        (!is_possibly_saved_password || !field_has_password_type)) {
      result.saved_username_typed |= user_typed;
      result.username_manually_filled |= manually_filled;
      result.username_automatically_filled |= automatically_filled;
      result.username_exists_in_profile_store |=
          is_possibly_saved_username_in_profile_store;
      result.username_exists_in_account_store |=
          is_possibly_saved_username_in_account_store;
    } else if (is_possibly_saved_password &&
               (!is_possibly_saved_username || field_has_password_type)) {
      result.saved_password_typed |= user_typed;
      result.password_manually_filled |= manually_filled;
      result.password_automatically_filled |= automatically_filled;
      result.password_exists_in_profile_store |=
          is_possibly_saved_password_in_profile_store;
      result.password_exists_in_account_store |=
          is_possibly_saved_password_in_account_store;
    } else if (user_typed) {
      if (field_has_password_type) {
        result.unknown_password_typed = true;
      } else {
        result.unknown_username_typed = true;
      }
    }
  }

  return result;
}

// Returns whether any value of |submitted_form| is listed in the
// |interactions_stats| has having been prompted to save as a credential and
// being ignored too often.
bool BlocklistedBySmartBubble(
    const FormData& submitted_form,
    const std::vector<InteractionsStats>& interactions_stats) {
  const int show_threshold =
      password_bubble_experiment::GetSmartBubbleDismissalThreshold();
  for (const FormFieldData& field : submitted_form.fields()) {
    const std::u16string& value =
        field.user_input().empty() ? field.value() : field.user_input();
    for (const InteractionsStats& stat : interactions_stats) {
      if (stat.username_value == value &&
          stat.dismissal_count >= show_threshold) {
        return true;
      }
    }
  }
  return false;
}

PasswordFormMetricsRecorder::FillingSource ComputeFillingSource(
    bool filled_from_profile_store,
    bool filled_from_account_store) {
  using FillingSource = PasswordFormMetricsRecorder::FillingSource;
  if (filled_from_profile_store) {
    if (filled_from_account_store) {
      return FillingSource::kFilledFromBothStores;
    }
    return FillingSource::kFilledFromProfileStore;
  }
  if (filled_from_account_store) {
    return FillingSource::kFilledFromAccountStore;
  }
  return FillingSource::kNotFilled;
}

}  // namespace

PasswordFormMetricsRecorder::PasswordFormMetricsRecorder(
    bool is_main_frame_secure,
    ukm::SourceId source_id,
    PrefService* pref_service)
    : clock_(base::DefaultClock::GetInstance()),
      is_main_frame_secure_(is_main_frame_secure),
      source_id_(source_id),
      ukm_entry_builder_(source_id),
      pref_service_(pref_service) {}

PasswordFormMetricsRecorder::~PasswordFormMetricsRecorder() {
  if (submit_result_ == SubmitResult::kNotSubmitted) {
    if (HasGeneratedPassword(generated_password_status_)) {
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
    } else if (generation_available_) {
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
    }
    ukm_entry_builder_.SetSubmission_Observed(0 /*false*/);
  }

  if (submit_result_ != SubmitResult::kNotSubmitted) {
    if (submitted_form_type_.has_value()) {
      base::UmaHistogramEnumeration("PasswordManager.SubmittedFormType2",
                                    submitted_form_type_.value());
      ukm_entry_builder_.SetSubmission_SubmittedFormType2(
          static_cast<int64_t>(submitted_form_type_.value()));
    }

    if (automation_rate_.has_value()) {
      base::UmaHistogramPercentage("PasswordManager.FillingAutomationRate",
                                   100 * automation_rate_.value());
    }
  }

  ukm_entry_builder_.SetUpdating_Prompt_Shown(update_prompt_shown_);
  ukm_entry_builder_.SetSaving_Prompt_Shown(save_prompt_shown_);

  for (const auto& action : detailed_user_actions_counts_) {
    switch (action.first) {
      case DetailedUserAction::kEditedUsernameInBubble:
        ukm_entry_builder_.SetUser_Action_EditedUsernameInBubble(action.second);
        break;
      case DetailedUserAction::kSelectedDifferentPasswordInBubble:
        ukm_entry_builder_.SetUser_Action_SelectedDifferentPasswordInBubble(
            action.second);
        break;
      case DetailedUserAction::kTriggeredManualFallbackForSaving:
        ukm_entry_builder_.SetUser_Action_TriggeredManualFallbackForSaving(
            action.second);
        break;
      case DetailedUserAction::kCorrectedUsernameInForm:
        ukm_entry_builder_.SetUser_Action_CorrectedUsernameInForm(
            action.second);
        break;
      case DetailedUserAction::kObsoleteTriggeredManualFallbackForUpdating:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  ukm_entry_builder_.SetGeneration_GeneratedPassword(
      HasGeneratedPassword(generated_password_status_));
  if (HasGeneratedPassword(generated_password_status_)) {
    ukm_entry_builder_.SetGeneration_GeneratedPasswordModified(
        generated_password_status_ !=
        GeneratedPasswordStatus::kPasswordAccepted);
  }
  if (generated_password_status_.has_value()) {
    // static cast to bypass a compilation error.
    UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.UserDecision",
                              static_cast<GeneratedPasswordStatus>(
                                  generated_password_status_.value()));
  }

  if (submitted_form_frame_.has_value()) {
    base::UmaHistogramEnumeration(
        "PasswordManager.SubmittedFormFrame2", submitted_form_frame_.value(),
        metrics_util::SubmittedFormFrame::SUBMITTED_FORM_FRAME_COUNT);
  }

  if (password_generation_popup_shown_ !=
      PasswordGenerationPopupShown::kNotShown) {
    UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.PopupShown",
                              password_generation_popup_shown_);
    ukm_entry_builder_.SetGeneration_PopupShown(
        static_cast<int64_t>(password_generation_popup_shown_));
  }

  if (showed_manual_fallback_for_saving_) {
    ukm_entry_builder_.SetSaving_ShowedManualFallbackForSaving(
        showed_manual_fallback_for_saving_.value());
  }

  if (form_changes_bitmask_) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.DynamicFormChanges",
                              *form_changes_bitmask_,
                              static_cast<uint32_t>(kMaxFormDifferencesValue));
    ukm_entry_builder_.SetDynamicFormChanges(*form_changes_bitmask_);
  }

  if (absl::holds_alternative<SingleUsernameFillingAssistance>(
          filling_assistance_)) {
    // Record the filling assistance for the single username case without
    // considering the `submit_result_` because submission success is only
    // calculated for forms that have password fields.
    SingleUsernameFillingAssistance filling_assistance =
        absl::get<SingleUsernameFillingAssistance>(filling_assistance_);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.FillingAssistanceForSingleUsername",
        filling_assistance);
    ukm_entry_builder_.SetManagerFill_AssistanceForSingleUsername(
        static_cast<int64_t>(filling_assistance));
  }

  if (submit_result_ == SubmitResult::kPassed &&
      absl::holds_alternative<FillingAssistance>(filling_assistance_)) {
    FillingAssistance filling_assistance =
        absl::get<FillingAssistance>(filling_assistance_);
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.FillingAssistance",
                              filling_assistance);
    ukm_entry_builder_.SetManagerFill_Assistance(
        static_cast<int64_t>(filling_assistance));

    if (is_main_frame_secure_) {
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.FillingAssistance.SecureOrigin", filling_assistance);
      if (is_mixed_content_form_) {
        UMA_HISTOGRAM_ENUMERATION("PasswordManager.FillingAssistance.MixedForm",
                                  filling_assistance);
      }
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.FillingAssistance.InsecureOrigin",
          filling_assistance);
    }

    if (account_storage_usage_level_) {
      std::string suffix =
          metrics_util::GetPasswordAccountStorageUsageLevelHistogramSuffix(
              *account_storage_usage_level_);
      base::UmaHistogramEnumeration(
          "PasswordManager.FillingAssistance." + suffix, filling_assistance);
    }

    if (filling_source_) {
      base::UmaHistogramEnumeration("PasswordManager.FillingSource",
                                    *filling_source_);

      // Update the "last used for filling" timestamp for the affected store(s).
      base::Time now = clock_->Now();
      if (*filling_source_ == FillingSource::kFilledFromProfileStore ||
          *filling_source_ == FillingSource::kFilledFromBothStores) {
        pref_service_->SetTime(prefs::kProfileStoreDateLastUsedForFilling, now);
      }
      if (*filling_source_ == FillingSource::kFilledFromAccountStore ||
          *filling_source_ == FillingSource::kFilledFromBothStores) {
        pref_service_->SetTime(prefs::kAccountStoreDateLastUsedForFilling, now);
      }

      // Determine which of the store(s) were used in the last 7/28 days.
      base::Time profile_store_last_use =
          pref_service_->GetTime(prefs::kProfileStoreDateLastUsedForFilling);
      base::Time account_store_last_use =
          pref_service_->GetTime(prefs::kAccountStoreDateLastUsedForFilling);

      bool was_profile_store_used_in_last_7_days =
          (now - profile_store_last_use) < base::Days(7);
      bool was_account_store_used_in_last_7_days =
          (now - account_store_last_use) < base::Days(7);
      base::UmaHistogramEnumeration(
          "PasswordManager.StoresUsedForFillingInLast7Days",
          ComputeFillingSource(was_profile_store_used_in_last_7_days,
                               was_account_store_used_in_last_7_days));

      bool was_profile_store_used_in_last_28_days =
          (now - profile_store_last_use) < base::Days(28);
      bool was_account_store_used_in_last_28_days =
          (now - account_store_last_use) < base::Days(28);
      base::UmaHistogramEnumeration(
          "PasswordManager.StoresUsedForFillingInLast28Days",
          ComputeFillingSource(was_profile_store_used_in_last_28_days,
                               was_account_store_used_in_last_28_days));
    }
  }

  if (submit_result_ == SubmitResult::kPassed && js_only_input_) {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.JavaScriptOnlyValueInSubmittedForm", *js_only_input_);
  }

  if (submit_result_ == SubmitResult::kPassed &&
      parsing_diff_on_filling_and_saving_.has_value()) {
    ukm_entry_builder_.SetParsingDiffFillingAndSaving(
        static_cast<int64_t>(parsing_diff_on_filling_and_saving_.value()));
  }

  ukm_entry_builder_.Record(ukm::UkmRecorder::Get());

#if BUILDFLAG(IS_ANDROID)
  if (form_submission_reached_) {
    LogFormSubmissionsVsSavePromptsHistogram(
        metrics_util::SaveFlowStep::kFormSubmitted);
  }
#endif
}

void PasswordFormMetricsRecorder::MarkGenerationAvailable() {
  generation_available_ = true;
}

void PasswordFormMetricsRecorder::SetGeneratedPasswordStatus(
    GeneratedPasswordStatus status) {
  generated_password_status_ = status;
}

void PasswordFormMetricsRecorder::SetManagerAction(
    ManagerAction manager_action) {
  manager_action_ = manager_action;
}

void PasswordFormMetricsRecorder::LogSubmitPassed() {
  if (submit_result_ != SubmitResult::kFailed) {
    if (HasGeneratedPassword(generated_password_status_)) {
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    } else if (generation_available_) {
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    }
  }
  base::RecordAction(base::UserMetricsAction("PasswordManager_LoginPassed"));
  ukm_entry_builder_.SetSubmission_Observed(1 /*true*/);
  ukm_entry_builder_.SetSubmission_SubmissionResult(
      static_cast<int64_t>(SubmitResult::kPassed));
  if (HasGeneratedPassword(generated_password_status_)) {
    ukm_entry_builder_.SetSubmission_SubmissionResult_GeneratedPassword(
        static_cast<int64_t>(SubmitResult::kPassed));
  }
  submit_result_ = SubmitResult::kPassed;
}

void PasswordFormMetricsRecorder::LogSubmitFailed() {
  if (HasGeneratedPassword(generated_password_status_)) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::GENERATED_PASSWORD_FORCE_SAVED);
  } else if (generation_available_) {
    metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
        metrics_util::PASSWORD_SUBMISSION_FAILED);
  }
  base::RecordAction(base::UserMetricsAction("PasswordManager_LoginFailed"));
  ukm_entry_builder_.SetSubmission_Observed(1 /*true*/);
  ukm_entry_builder_.SetSubmission_SubmissionResult(
      static_cast<int64_t>(SubmitResult::kFailed));
  if (HasGeneratedPassword(generated_password_status_)) {
    ukm_entry_builder_.SetSubmission_SubmissionResult_GeneratedPassword(
        static_cast<int64_t>(SubmitResult::kFailed));
  }
  submit_result_ = SubmitResult::kFailed;
}

void PasswordFormMetricsRecorder::SetPasswordGenerationPopupShown(
    bool generation_popup_was_shown,
    bool is_manual_generation) {
  password_generation_popup_shown_ =
      generation_popup_was_shown
          ? (is_manual_generation
                 ? PasswordGenerationPopupShown::kShownManually
                 : PasswordGenerationPopupShown::kShownAutomatically)
          : PasswordGenerationPopupShown::kNotShown;
}

void PasswordFormMetricsRecorder::SetSubmittedFormType(
    metrics_util::SubmittedFormType form_type) {
  submitted_form_type_ = form_type;
}

void PasswordFormMetricsRecorder::SetSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  ukm_entry_builder_.SetSubmission_Indicator(static_cast<int>(event));
}

void PasswordFormMetricsRecorder::RecordDetailedUserAction(
    PasswordFormMetricsRecorder::DetailedUserAction action) {
  detailed_user_actions_counts_[action]++;
}

// static
int64_t PasswordFormMetricsRecorder::HashFormSignature(
    autofill::FormSignature form_signature) {
  // Note that this is an intentionally small hash domain for privacy reasons.
  return static_cast<uint64_t>(form_signature) % 1021;
}

void PasswordFormMetricsRecorder::RecordFormSignature(
    autofill::FormSignature form_signature) {
  ukm_entry_builder_.SetContext_FormSignature(
      HashFormSignature(form_signature));
}

void PasswordFormMetricsRecorder::RecordReadonlyWhenFilling(uint64_t value) {
  ukm_entry_builder_.SetReadonlyWhenFilling(value);
}

void PasswordFormMetricsRecorder::RecordReadonlyWhenSaving(uint64_t value) {
  ukm_entry_builder_.SetReadonlyWhenSaving(value);
}

void PasswordFormMetricsRecorder::RecordShowManualFallbackForSaving(
    bool has_generated_password,
    bool is_update) {
  showed_manual_fallback_for_saving_ =
      1 + (has_generated_password ? 2 : 0) + (is_update ? 4 : 0);
}

void PasswordFormMetricsRecorder::RecordFormChangeBitmask(uint32_t bitmask) {
  if (!form_changes_bitmask_) {
    form_changes_bitmask_ = bitmask;
  } else {
    *form_changes_bitmask_ |= bitmask;
  }
}

void PasswordFormMetricsRecorder::RecordFirstFillingResult(int32_t result) {
  if (recorded_first_filling_result_) {
    return;
  }
  ukm_entry_builder_.SetFill_FirstFillingResultInRenderer(result);
  recorded_first_filling_result_ = true;
}

void PasswordFormMetricsRecorder::RecordFirstWaitForUsernameReason(
    WaitForUsernameReason reason) {
  if (recorded_wait_for_username_reason_) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FirstWaitForUsernameReason",
                            reason);
  ukm_entry_builder_.SetFill_FirstWaitForUsernameReason(
      static_cast<int64_t>(reason));
  recorded_wait_for_username_reason_ = true;
}

void PasswordFormMetricsRecorder::RecordMatchedFormType(
    const PasswordForm& form) {
  if (std::exchange(recorded_preferred_matched_password_type, true)) {
    return;
  }

  using FormMatchType =
      password_manager::PasswordFormMetricsRecorder::MatchedFormType;
  FormMatchType match_type;
  switch (password_manager_util::GetMatchType(form)) {
    case password_manager_util::GetLoginMatchType::kExact:
      match_type = FormMatchType::kExactMatch;
      break;
    case password_manager_util::GetLoginMatchType::kAffiliated:
      match_type = affiliations::IsValidAndroidFacetURI(form.signon_realm)
                       ? FormMatchType::kAffiliatedApp
                       : FormMatchType::kAffiliatedWebsites;
      break;
    case password_manager_util::GetLoginMatchType::kPSL:
      match_type = FormMatchType::kPublicSuffixMatch;
      break;
    case password_manager_util::GetLoginMatchType::kGrouped:
      // Grouped credentials are never filled on page load.
      NOTREACHED();
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.MatchedFormType", match_type);
}

void PasswordFormMetricsRecorder::RecordPotentialPreferredMatch(
    std::optional<MatchedFormType> form_type) {
  if (!form_type) {
    return;
  }
  if (std::exchange(recorded_potential_preferred_matched_password_type, true)) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PotentialBestMatchFormType",
                            form_type.value());
}

void PasswordFormMetricsRecorder::CalculateFillingAssistanceMetric(
    const PasswordForm& submitted_form,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_usernames,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_passwords,
    bool is_blocklisted,
    const std::vector<InteractionsStats>& interactions_stats,
    features_util::PasswordAccountStorageUsageLevel
        account_storage_usage_level) {
  if (submitted_form.HasNonEmptyPasswordValue()) {
    CalculatePasswordFillingAssistanceMetric(
        submitted_form.form_data, saved_usernames, saved_passwords,
        is_blocklisted, interactions_stats, account_storage_usage_level);
  } else if (!submitted_form.username_value.empty()) {
    CalculateSingleUsernameFillingAssistanceMetric(
        submitted_form.form_data, saved_usernames, is_blocklisted,
        interactions_stats);
  }
}

void PasswordFormMetricsRecorder::CalculatePasswordFillingAssistanceMetric(
    const FormData& submitted_form,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_usernames,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_passwords,
    bool is_blocklisted,
    const std::vector<InteractionsStats>& interactions_stats,
    features_util::PasswordAccountStorageUsageLevel
        account_storage_usage_level) {
  CalculateJsOnlyInput(submitted_form);
  CalculateAutomationRate(submitted_form);
  if (is_main_frame_secure_ && submitted_form.action().is_valid() &&
      !submitted_form.is_action_empty() &&
      !submitted_form.action().SchemeIsCryptographic()) {
    is_mixed_content_form_ = true;
  }

  filling_source_ = FillingSource::kNotFilled;
  account_storage_usage_level_ = account_storage_usage_level;

  if (saved_passwords.empty() && is_blocklisted) {
    filling_assistance_ = FillingAssistance::kNoSavedCredentialsAndBlocklisted;
    return;
  }

  if (saved_passwords.empty()) {
    filling_assistance_ =
        BlocklistedBySmartBubble(submitted_form, interactions_stats)
            ? FillingAssistance::kNoSavedCredentialsAndBlocklistedBySmartBubble
            : FillingAssistance::kNoSavedCredentials;
    return;
  }

  // Saved credentials are assumed to be correct as they match stored
  // credentials in subsequent calculations.

  UsernamePasswordsState username_password_state =
      CalculateUsernamePasswordsState(submitted_form, saved_usernames,
                                      saved_passwords);

  // Consider cases when the user typed known or unknown credentials.
  if (username_password_state.saved_password_typed) {
    filling_assistance_ = FillingAssistance::kKnownPasswordTyped;
    return;
  }

  if (!username_password_state.IsPasswordFilled()) {
    filling_assistance_ =
        username_password_state.unknown_password_typed
            ? FillingAssistance::kNewPasswordTypedWhileCredentialsExisted
            : FillingAssistance::kNoUserInputNoFillingInPasswordFields;
    return;
  }

  // At this point, the password was filled from at least one of the two stores,
  // so compute the filling source now.
  filling_source_ = ComputeFillingSource(
      username_password_state.password_exists_in_profile_store,
      username_password_state.password_exists_in_account_store);
  DCHECK_NE(*filling_source_, FillingSource::kNotFilled);

  if (username_password_state.saved_username_typed) {
    filling_assistance_ = FillingAssistance::kUsernameTypedPasswordFilled;
    return;
  }

  // Cases related to user typing are already considered and excluded. Only
  // filling related cases are left.
  if (username_password_state.username_manually_filled ||
      username_password_state.password_manually_filled) {
    filling_assistance_ = FillingAssistance::kManual;
    return;
  }

  if (username_password_state.password_automatically_filled) {
    filling_assistance_ = FillingAssistance::kAutomatic;
    return;
  }

  // If execution gets here, we have a bug in our state machine.
  NOTREACHED_IN_MIGRATION();
}

void PasswordFormMetricsRecorder::
    CalculateSingleUsernameFillingAssistanceMetric(
        const FormData& submitted_form,
        const std::set<std::pair<std::u16string, PasswordForm::Store>>&
            saved_usernames,
        bool is_blocklisted,
        const std::vector<InteractionsStats>& interactions_stats) {
  // Cases related to not stored crendentials. Do not proceed with the filling
  // experience cases if there are no stored usernames.
  if (saved_usernames.empty()) {
    if (is_blocklisted) {
      filling_assistance_ =
          SingleUsernameFillingAssistance::kNoSavedCredentialsAndBlocklisted;
    } else {
      filling_assistance_ =
          BlocklistedBySmartBubble(submitted_form, interactions_stats)
              ? SingleUsernameFillingAssistance::
                    kNoSavedCredentialsAndBlocklistedBySmartBubble
              : SingleUsernameFillingAssistance::kNoSavedCredentials;
    }
    return;
  }

  // Cases related to the username filling experience while there are stored
  // credentials. At this point, it is known that there are stored credentials.

  UsernamePasswordsState username_password_state =
      CalculateUsernamePasswordsState(submitted_form, saved_usernames,
                                      /*saved_passwords=*/{});

  // Case where the username was typed regardless of whether or not it was
  // filled.
  if (username_password_state.saved_username_typed) {
    // Case where the user typed a known username.
    filling_assistance_ = SingleUsernameFillingAssistance::kKnownUsernameTyped;
    return;
  }

  // Cases where the username wasn't filled but might have been typed.
  if (!username_password_state.IsUsernameFilled()) {
    if (username_password_state.unknown_username_typed) {
      // Case where the user typed an unknown username.
      filling_assistance_ = SingleUsernameFillingAssistance::
          kNewUsernameTypedWhileCredentialsExisted;
    } else {
      // Case there was no filling and no typing detected .
      filling_assistance_ =
          SingleUsernameFillingAssistance::kNoUserInputNoFillingOfUsername;
    }
    return;
  }

  // Cases related to user typing are already considered and excluded. Only
  // filling related cases are left.
  if (username_password_state.username_manually_filled) {
    filling_assistance_ = SingleUsernameFillingAssistance::kManual;
    return;
  }
  if (username_password_state.username_automatically_filled) {
    filling_assistance_ = SingleUsernameFillingAssistance::kAutomatic;
    return;
  }

  // It MUST BE impossible to reach this code path because all the filling
  // cases are handled at this point: (1) when there is no manual nor automatic
  // fill, (2) when there is manual fill, and (3) when there is automatic fill.
  // The check here is to make sure that all states are handled to calculate the
  // filling assistance metric.
  NOTREACHED();
}

void PasswordFormMetricsRecorder::CalculateJsOnlyInput(
    const FormData& submitted_form) {
  bool had_focus = false;
  bool had_user_input_or_autofill_on_password = false;
  for (const auto& field : submitted_form.fields()) {
    if (field.HadFocus()) {
      had_focus = true;
    }
    if (field.IsPasswordInputElement() &&
        (field.DidUserType() || field.WasPasswordAutofilled())) {
      had_user_input_or_autofill_on_password = true;
    }
  }

  js_only_input_ = had_user_input_or_autofill_on_password
                       ? JsOnlyInput::kAutofillOrUserInput
                       : (had_focus ? JsOnlyInput::kOnlyJsInputWithFocus
                                    : JsOnlyInput::kOnlyJsInputNoFocus);
}

void PasswordFormMetricsRecorder::CalculateAutomationRate(
    const FormData& submitted_form) {
  float total_length_autofilled_fields = 0.0;
  float total_length = 0.0;
  for (const auto& field : submitted_form.fields()) {
    if (!field.IsTextInputElement()) {
      continue;
    }

    // The field was never filled or typed in, ignore it.
    if (!field.DidUserType() && !field.WasPasswordAutofilled()) {
      continue;
    }
    if (field.WasPasswordAutofilled()) {
      total_length_autofilled_fields += field.value().size();
    }
    total_length += field.value().size();
  }

  if (total_length > 0) {
    automation_rate_ = total_length_autofilled_fields / total_length;
  }
}

void PasswordFormMetricsRecorder::CacheParsingResultInFillingMode(
    const PasswordForm& form) {
  username_rendered_id_ = form.username_element_renderer_id;
  password_rendered_id_ = form.password_element_renderer_id;
  new_password_rendered_id_ = form.new_password_element_renderer_id;
  confirmation_password_rendered_id_ =
      form.confirmation_password_element_renderer_id;
}

void PasswordFormMetricsRecorder::CalculateParsingDifferenceOnSavingAndFilling(
    const PasswordForm& form) {
  bool same_username =
      username_rendered_id_ == form.username_element_renderer_id;
  bool same_passwords =
      (password_rendered_id_ == form.password_element_renderer_id) &&
      (new_password_rendered_id_ == form.new_password_element_renderer_id) &&
      (confirmation_password_rendered_id_ ==
       form.confirmation_password_element_renderer_id);

  if (same_username) {
    parsing_diff_on_filling_and_saving_ =
        same_passwords ? ParsingDifference::kNone
                       : ParsingDifference::kPasswordDiff;
  } else {
    parsing_diff_on_filling_and_saving_ =
        same_passwords ? ParsingDifference::kUsernameDiff
                       : ParsingDifference::kUsernameAndPasswordDiff;
  }
}

void PasswordFormMetricsRecorder::RecordPasswordBubbleShown(
    metrics_util::CredentialSourceType credential_source_type,
    metrics_util::UIDisplayDisposition display_disposition) {
  if (credential_source_type == metrics_util::CredentialSourceType::kUnknown) {
    return;
  }
  DCHECK_EQ(CurrentBubbleOfInterest::kNone, current_bubble_);
  BubbleTrigger automatic_trigger_type =
      credential_source_type ==
              metrics_util::CredentialSourceType::kPasswordManager
          ? BubbleTrigger::kPasswordManagerSuggestionAutomatic
          : BubbleTrigger::kCredentialManagementAPIAutomatic;
  BubbleTrigger manual_trigger_type =
      credential_source_type ==
              metrics_util::CredentialSourceType::kPasswordManager
          ? BubbleTrigger::kPasswordManagerSuggestionManual
          : BubbleTrigger::kCredentialManagementAPIManual;

  switch (display_disposition) {
    // New credential cases:
    case metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING:
      current_bubble_ = CurrentBubbleOfInterest::kSaveBubble;
      save_prompt_shown_ = true;
      ukm_entry_builder_.SetSaving_Prompt_Trigger(
          static_cast<int64_t>(automatic_trigger_type));
      break;
    case metrics_util::MANUAL_WITH_PASSWORD_PENDING:
      current_bubble_ = CurrentBubbleOfInterest::kSaveBubble;
      save_prompt_shown_ = true;
      ukm_entry_builder_.SetSaving_Prompt_Trigger(
          static_cast<int64_t>(manual_trigger_type));
      break;

    // Update cases:
    case metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE:
      current_bubble_ = CurrentBubbleOfInterest::kUpdateBubble;
      update_prompt_shown_ = true;
      ukm_entry_builder_.SetUpdating_Prompt_Trigger(
          static_cast<int64_t>(automatic_trigger_type));
      break;
    case metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE:
      current_bubble_ = CurrentBubbleOfInterest::kUpdateBubble;
      update_prompt_shown_ = true;
      ukm_entry_builder_.SetUpdating_Prompt_Trigger(
          static_cast<int64_t>(manual_trigger_type));
      break;

    // Other reasons to show a bubble:
    // TODO(crbug.com/40123456): Decide how to collect metrics for this new UI.
    case metrics_util::AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY:
    case metrics_util::MANUAL_MANAGE_PASSWORDS:
    case metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION:
    case metrics_util::MANUAL_GENERATED_PASSWORD_CONFIRMATION:
    case metrics_util::AUTOMATIC_SIGNIN_TOAST:
    case metrics_util::AUTOMATIC_COMPROMISED_CREDENTIALS_REMINDER:
    case metrics_util::AUTOMATIC_MOVE_TO_ACCOUNT_STORE:
    case metrics_util::AUTOMATIC_BIOMETRIC_AUTHENTICATION_FOR_FILLING:
    case metrics_util::MANUAL_BIOMETRIC_AUTHENTICATION_FOR_FILLING:
    case metrics_util::AUTOMATIC_BIOMETRIC_AUTHENTICATION_CONFIRMATION:
    case metrics_util::AUTOMATIC_SHARED_PASSWORDS_NOTIFICATION:
    case metrics_util::AUTOMATIC_ADD_USERNAME_BUBBLE:
    case metrics_util::MANUAL_ADD_USERNAME_BUBBLE:
    case metrics_util::AUTOMATIC_RELAUNCH_CHROME_BUBBLE:
    case metrics_util::AUTOMATIC_DEFAULT_STORE_CHANGED_BUBBLE:
    case metrics_util::AUTOMATIC_PASSKEY_SAVED_CONFIRMATION:
    case metrics_util::AUTOMATIC_PASSKEY_DELETED_CONFIRMATION:
    case metrics_util::MANUAL_PASSKEY_DELETED_CONFIRMATION:
    case metrics_util::AUTOMATIC_PASSKEY_UPDATED_CONFIRMATION:
    case metrics_util::MANUAL_PASSKEY_UPDATED_CONFIRMATION:
    case metrics_util::AUTOMATIC_PASSKEY_NOT_ACCEPTED_BUBBLE:
    case metrics_util::MANUAL_PASSKEY_NOT_ACCEPTED_BUBBLE:
      // Do nothing.
      return;

    // Obsolte display dispositions:
    case metrics_util::MANUAL_BLOCKLISTED_OBSOLETE:
    case metrics_util::AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE:
    case metrics_util::NUM_DISPLAY_DISPOSITIONS:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void PasswordFormMetricsRecorder::RecordUIDismissalReason(
    metrics_util::UIDismissalReason ui_dismissal_reason) {
  if (current_bubble_ != CurrentBubbleOfInterest::kUpdateBubble &&
      current_bubble_ != CurrentBubbleOfInterest::kSaveBubble) {
    return;
  }
  auto bubble_dismissal_reason = GetBubbleDismissalReason(ui_dismissal_reason);
  if (bubble_dismissal_reason != BubbleDismissalReason::kUnknown) {
    if (current_bubble_ == CurrentBubbleOfInterest::kUpdateBubble) {
      ukm_entry_builder_.SetUpdating_Prompt_Interaction(
          static_cast<int64_t>(bubble_dismissal_reason));
    } else {
      ukm_entry_builder_.SetSaving_Prompt_Interaction(
          static_cast<int64_t>(bubble_dismissal_reason));
    }

    // Record saving on username first flow metric.
    if (possible_username_used_) {
      auto saving_on_username_first_flow = SavingOnUsernameFirstFlow::kNotSaved;
      if (bubble_dismissal_reason == BubbleDismissalReason::kAccepted) {
        saving_on_username_first_flow =
            username_updated_in_bubble_
                ? SavingOnUsernameFirstFlow::kSavedWithEditedUsername
                : SavingOnUsernameFirstFlow::kSaved;
      }
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.SavingOnUsernameFirstFlow",
                                saving_on_username_first_flow);
    }
  }

  current_bubble_ = CurrentBubbleOfInterest::kNone;
}

void PasswordFormMetricsRecorder::RecordFillEvent(ManagerAutofillEvent event) {
  ukm_entry_builder_.SetManagerFill_Action(event);
}

std::string
PasswordFormMetricsRecorder::FillingAssinstanceToHatsInProductDataString() {
  if (!absl::holds_alternative<FillingAssistance>(filling_assistance_)) {
    return std::string();
  }

  FillingAssistance filling_assistance =
      absl::get<FillingAssistance>(filling_assistance_);
  // These values are used for logging and should not be modified.
  switch (filling_assistance) {
    case FillingAssistance::kAutomatic:
      return "Credentials were filled automatically";
    case FillingAssistance::kManual:
      return "Credentials were filled manually, without typing";
    case FillingAssistance::kUsernameTypedPasswordFilled:
      return "Password was filled (automatically or manually), known username "
             "was typed";
    case FillingAssistance::kKnownPasswordTyped:
      return "Known password was typed";
    case FillingAssistance::kNewPasswordTypedWhileCredentialsExisted:
      return "Unknown password was typed while some credentials were stored.";
    case FillingAssistance::kNoSavedCredentials:
      return "No saved credentials.";
    case FillingAssistance::kNoUserInputNoFillingInPasswordFields:
      return "Neither user input nor filling.";
    case FillingAssistance::kNoSavedCredentialsAndBlocklisted:
      return "Domain is blocklisted and no other credentials exist.";
    case FillingAssistance::kNoSavedCredentialsAndBlocklistedBySmartBubble:
      return "No credentials exist and the user has ignored the save bubble "
             "too often, meaning that they won't be asked to save credentials "
             "anymore.";
  };
  NOTREACHED();
}

}  // namespace password_manager
