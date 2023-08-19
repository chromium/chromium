// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/default_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/statistics_table.h"
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
    case metrics_util::CLICKED_PASSWORDS_DASHBOARD:
    case metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT:
      break;

    // These should not reach here:
    case metrics_util::CLICKED_DONE_OBSOLETE:
    case metrics_util::CLICKED_OK_OBSOLETE:
    case metrics_util::CLICKED_UNBLOCKLIST_OBSOLETE:
    case metrics_util::CLICKED_CREDENTIAL_OBSOLETE:
    case metrics_util::AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE:
    case metrics_util::CLICKED_BRAND_NAME_OBSOLETE:
    case metrics_util::NUM_UI_RESPONSES:
      NOTREACHED();
      break;
  }
  return BubbleDismissalReason::kUnknown;
}

bool HasGeneratedPassword(
    absl::optional<PasswordFormMetricsRecorder::GeneratedPasswordStatus>
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

  bool password_exists_in_profile_store = false;
  bool password_exists_in_account_store = false;

  bool IsPasswordFilled() {
    return password_automatically_filled || password_manually_filled;
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

  for (const FormFieldData& field : submitted_form.fields) {
    const std::u16string& value =
        field.user_input.empty() ? field.value : field.user_input;

    bool user_typed = field.properties_mask & FieldPropertiesFlags::kUserTyped;
    bool manually_filled =
        field.properties_mask & FieldPropertiesFlags::kAutofilledOnUserTrigger;
    bool automatically_filled =
        field.properties_mask & FieldPropertiesFlags::kAutofilledOnPageLoad;

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

    bool field_has_password_type = field.form_control_type == "password";

    if (is_possibly_saved_username &&
        (!is_possibly_saved_password || !field_has_password_type)) {
      result.saved_username_typed |= user_typed;
      result.username_manually_filled |= manually_filled;
      result.username_automatically_filled |= automatically_filled;
    } else if (is_possibly_saved_password &&
               (!is_possibly_saved_username || field_has_password_type)) {
      result.saved_password_typed |= user_typed;
      result.password_manually_filled |= manually_filled;
      result.password_automatically_filled |= automatically_filled;
      result.password_exists_in_profile_store |=
          is_possibly_saved_password_in_profile_store;
      result.password_exists_in_account_store |=
          is_possibly_saved_password_in_account_store;
    } else if (user_typed && field_has_password_type) {
      result.unknown_password_typed = true;
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
  for (const FormFieldData& field : submitted_form.fields) {
    const std::u16string& value =
        field.user_input.empty() ? field.value : field.user_input;
    for (const InteractionsStats& stat : interactions_stats) {
      if (stat.username_value == value &&
          stat.dismissal_count >= show_threshold)
        return true;
    }
  }
  return false;
}

PasswordFormMetricsRecorder::FillingSource ComputeFillingSource(
    bool filled_from_profile_store,
    bool filled_from_account_store) {
  using FillingSource = PasswordFormMetricsRecorder::FillingSource;
  if (filled_from_profile_store) {
    if (filled_from_account_store)
      return FillingSource::kFilledFromBothStores;
    return FillingSource::kFilledFromProfileStore;
  }
  if (filled_from_account_store)
    return FillingSource::kFilledFromAccountStore;
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

  if (submit_result_ != SubmitResult::kNotSubmitted && submitted_form_type_) {
    base::UmaHistogramEnumeration("PasswordManager.SubmittedFormType2",
                                  submitted_form_type_.value());
    ukm_entry_builder_.SetSubmission_SubmittedFormType2(
        static_cast<int64_t>(submitted_form_type_.value()));
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
        NOTREACHED();
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

  if (submit_result_ == SubmitResult::kPassed && filling_assistance_) {
    FillingAssistance filling_assistance = *filling_assistance_;
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

  ukm_entry_builder_.Record(ukm::UkmRecorder::Get());
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
  if (!form_changes_bitmask_)
    form_changes_bitmask_ = bitmask;
  else
    *form_changes_bitmask_ |= bitmask;
}

void PasswordFormMetricsRecorder::RecordFirstFillingResult(int32_t result) {
  if (recorded_first_filling_result_)
    return;
  ukm_entry_builder_.SetFill_FirstFillingResultInRenderer(result);
  recorded_first_filling_result_ = true;
}

void PasswordFormMetricsRecorder::RecordFirstWaitForUsernameReason(
    WaitForUsernameReason reason) {
  if (recorded_wait_for_username_reason_)
    return;
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FirstWaitForUsernameReason",
                            reason);
  ukm_entry_builder_.SetFill_FirstWaitForUsernameReason(
      static_cast<int64_t>(reason));
  recorded_wait_for_username_reason_ = true;
}

void PasswordFormMetricsRecorder::RecordMatchedFormType(MatchedFormType type) {
  if (!std::exchange(recorded_preferred_matched_password_type, true)) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.MatchedFormType", type);
  }
}

void PasswordFormMetricsRecorder::CalculateFillingAssistanceMetric(
    const FormData& submitted_form,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_usernames,
    const std::set<std::pair<std::u16string, PasswordForm::Store>>&
        saved_passwords,
    bool is_blocklisted,
    const std::vector<InteractionsStats>& interactions_stats,
    metrics_util::PasswordAccountStorageUsageLevel
        account_storage_usage_level) {
  CalculateJsOnlyInput(submitted_form);
  if (is_main_frame_secure_ && submitted_form.action.is_valid() &&
      !submitted_form.is_action_empty &&
      !submitted_form.action.SchemeIsCryptographic()) {
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
  NOTREACHED();
}

void PasswordFormMetricsRecorder::CalculateJsOnlyInput(
    const FormData& submitted_form) {
  bool had_focus = false;
  bool had_user_input_or_autofill_on_password = false;
  for (const auto& field : submitted_form.fields) {
    if (field.HadFocus())
      had_focus = true;
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

void PasswordFormMetricsRecorder::RecordPasswordBubbleShown(
    metrics_util::CredentialSourceType credential_source_type,
    metrics_util::UIDisplayDisposition display_disposition) {
  if (credential_source_type == metrics_util::CredentialSourceType::kUnknown)
    return;
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
    // TODO(crbug.com/1063853): Decide how to collect metrics for this new UI.
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
      // Do nothing.
      return;

    // Obsolte display dispositions:
    case metrics_util::MANUAL_BLOCKLISTED_OBSOLETE:
    case metrics_util::AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE:
    case metrics_util::NUM_DISPLAY_DISPOSITIONS:
      NOTREACHED();
      return;
  }
}

void PasswordFormMetricsRecorder::RecordUIDismissalReason(
    metrics_util::UIDismissalReason ui_dismissal_reason) {
  if (current_bubble_ != CurrentBubbleOfInterest::kUpdateBubble &&
      current_bubble_ != CurrentBubbleOfInterest::kSaveBubble)
    return;
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

}  // namespace password_manager
