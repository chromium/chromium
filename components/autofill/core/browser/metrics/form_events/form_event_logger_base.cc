// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"

using base::UmaHistogramBoolean;

namespace autofill::autofill_metrics {

namespace {

// The default group is mapped to nullptr, as this does not get logged as a
// histogram sample.
const char* AblationGroupToString(AblationGroup ablation_group) {
  switch (ablation_group) {
    case AblationGroup::kAblation:
      return "Ablation";
    case AblationGroup::kControl:
      return "Control";
    case AblationGroup::kDefault:
      return nullptr;
  }
  return nullptr;
}

bool DetermineHeuristicOnlyEmailFormStatus(const FormStructure& form) {
  // First, check the prerequisites.
  // Without the feature being enabled, such forms are not parsed.
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableEmailHeuristicOnlyAddressForms)) {
    return false;
  }
  // When the feature is enabled, the forms for which this classification is
  // applicable must be inside a form tag, must not run heuristics normally
  // (i.e., their field count is below `kMinRequiredFieldsForHeuristics`), but
  // must be eligible for single field form heuristics.
  if (!form.is_form_tag() || form.ShouldRunHeuristics() ||
      !form.ShouldRunHeuristicsForSingleFieldForms()) {
    return false;
  }
  // Having met the prerequisites, now determine if there's a field whose
  // heuristic type is email.
  for (const auto& field : form.fields()) {
    if (field && field->heuristic_type() == EMAIL_ADDRESS &&
        field->server_type() == NO_SERVER_DATA) {
      return true;
    }
  }
  // No email fields, therefore this is not a heuristic-only email form.
  return false;
}

}  // namespace

FormEventLoggerBase::FormEventLoggerBase(
    const std::string& form_type_name,
    bool is_in_any_main_frame,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    AutofillClient* client)
    : form_type_name_(form_type_name),
      is_in_any_main_frame_(is_in_any_main_frame),
      form_interactions_ukm_logger_(form_interactions_ukm_logger),
      client_(*client) {}

FormEventLoggerBase::~FormEventLoggerBase() {
  DCHECK(has_called_on_destoryed_);
}

void FormEventLoggerBase::OnDidInteractWithAutofillableForm(
    const FormStructure& form,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
  signin_state_for_metrics_ = signin_state_for_metrics;
  if (!has_logged_interacted_) {
    has_logged_interacted_ = true;
    LogUkmInteractedWithForm(form.form_signature());
    Log(FORM_EVENT_INTERACTED_ONCE, form);
  }
}

void FormEventLoggerBase::OnDidPollSuggestions(
    const FormFieldData& field,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
  signin_state_for_metrics_ = signin_state_for_metrics;
  // Record only one poll user action for consecutive polls of the same field.
  // This is to avoid recording too many poll actions (for example when a user
  // types in a field, triggering multiple queries) to make the analysis more
  // simple.
  if (!field.SameFieldAs(last_polled_field_)) {
    RecordPollSuggestions();
    last_polled_field_ = field;
  }
}

void FormEventLoggerBase::OnDidParseForm(const FormStructure& form) {
  Log(FORM_EVENT_DID_PARSE_FORM, form);
  RecordParseForm();
  has_parsed_form_ = true;
}

void FormEventLoggerBase::OnUserHideSuggestions(const FormStructure& form,
                                                const AutofillField& field) {
  Log(FORM_EVENT_USER_HIDE_SUGGESTIONS, form);
  if (!has_logged_user_hide_suggestions_) {
    has_logged_user_hide_suggestions_ = true;
    Log(FORM_EVENT_USER_HIDE_SUGGESTIONS_ONCE, form);
  }
}

void FormEventLoggerBase::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    bool off_the_record) {
  signin_state_for_metrics_ = signin_state_for_metrics;
  form_interactions_ukm_logger_->LogSuggestionsShown(
      form, field, form_parsed_timestamp, off_the_record);

  Log(FORM_EVENT_SUGGESTIONS_SHOWN, form);
  if (!has_logged_suggestions_shown_) {
    has_logged_suggestions_shown_ = true;
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, form);
    OnSuggestionsShownOnce(form);
  }

  has_logged_autocomplete_off_ |= field.autocomplete_attribute == "off";

  RecordShowSuggestions();
}

void FormEventLoggerBase::SetAblationStatus(
    AblationGroup ablation_group,
    AblationGroup conditional_ablation_group) {
  ablation_group_ = ablation_group;
  // For each form, the ablation group should be stable (except in the rare
  // event that a day boundary is crossed). In practice, it is possible,
  // however, that a the condtional_ablation_group is reported as kDefault
  // because the user has typed a prefix into an input element that filtered
  // all filling options. In this case, we should still consider this an
  // ablation experience if suggestions were available when the field was empty.
  if (conditional_ablation_group != AblationGroup::kDefault)
    conditional_ablation_group_ = conditional_ablation_group;
}

void FormEventLoggerBase::SetTimeFromInteractionToSubmission(
    base::TimeDelta time_from_interaction_to_submission) {
  time_from_interaction_to_submission_ = time_from_interaction_to_submission;
}

void FormEventLoggerBase::OnWillSubmitForm(
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    const FormStructure& form) {
  signin_state_for_metrics_ = signin_state_for_metrics;
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_will_submit_)
    return;
  has_logged_will_submit_ = true;
  submitted_form_types_ = form.GetFormTypes();

  // Determine whether logging of email-heuristic only metrics is required.
  is_heuristic_only_email_form_ = (is_heuristic_only_email_form_ ||
                                   DetermineHeuristicOnlyEmailFormStatus(form));

  LogWillSubmitForm(form);

  if (has_logged_suggestions_shown_) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, form);
  }

  base::RecordAction(base::UserMetricsAction("Autofill_OnWillSubmitForm"));
}

void FormEventLoggerBase::OnFormSubmitted(
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    const FormStructure& form) {
  signin_state_for_metrics_ = signin_state_for_metrics;
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_submitted_)
    return;
  has_logged_submitted_ = true;

  LogFormSubmitted(form);

  if (has_logged_suggestions_shown_) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, form);
    OnSuggestionsShownSubmittedOnce(form);
  }
}

void FormEventLoggerBase::OnTypedIntoNonFilledField() {
  has_logged_typed_into_non_filled_field_ = true;
}

void FormEventLoggerBase::OnEditedAutofilledField() {
  has_logged_edited_autofilled_field_ = true;
}

void FormEventLoggerBase::OnDestroyed() {
  DCHECK(!has_called_on_destoryed_);
  has_called_on_destoryed_ = true;
  // Don't record Funnel and Key metrics for the ablation group as they don't
  // represent the true quality metrics.
  if (ablation_group_ != AblationGroup::kAblation) {
    RecordFunnelMetrics();
    RecordKeyMetrics();
  }
  RecordAblationMetrics();
}

void FormEventLoggerBase::
    OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(
        const FormStructure& form) {
  if (has_logged_autofilled_field_was_cleared_by_javascript_after_fill_)
    return;
  Log(FORM_EVENT_AUTOFILLED_FIELD_CLEARED_BY_JAVASCRIPT_AFTER_FILL_ONCE, form);
  has_logged_autofilled_field_was_cleared_by_javascript_after_fill_ = true;
}

void FormEventLoggerBase::Log(FormEvent event, const FormStructure& form) {
  DCHECK_LT(event, NUM_FORM_EVENTS);
  std::string name("Autofill.FormEvents." + form_type_name_);
  form_events_set_[form.global_id()].insert(event);
  base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);

  // Log UKM metrics for only autofillable form events.
  if (form.IsAutofillable()) {
    form_interactions_ukm_logger_->LogFormEvent(event, form.GetFormTypes(),
                                                form.form_parsed_timestamp());
  }

  // Log again in a different histogram so that iframes can be analyzed on
  // their own.
  base::UmaHistogramEnumeration(
      name + (is_in_any_main_frame_ ? ".IsInMainFrame" : ".IsInIFrame"), event,
      NUM_FORM_EVENTS);

  // Allow specialized types of logging, e.g. splitting metrics in useful ways.
  OnLog(name, event, form);

  // Logging again in a different histogram for segmentation purposes.
  if (server_record_type_count_ == 0 && local_record_type_count_ == 0)
    name += ".WithNoData";
  else if (server_record_type_count_ > 0 && local_record_type_count_ == 0)
    name += ".WithOnlyServerData";
  else if (server_record_type_count_ == 0 && local_record_type_count_ > 0)
    name += ".WithOnlyLocalData";
  else
    name += ".WithBothServerAndLocalData";
  base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);
  base::UmaHistogramEnumeration(
      name +
          AutofillMetrics::GetMetricsSyncStateSuffix(signin_state_for_metrics_),
      event, NUM_FORM_EVENTS);
}

void FormEventLoggerBase::LogWillSubmitForm(const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, form);
  }
}

void FormEventLoggerBase::LogFormSubmitted(const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, form);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, form);
  }
}

void FormEventLoggerBase::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  form_interactions_ukm_logger_->LogInteractedWithForm(
      /*is_for_credit_card=*/false, local_record_type_count_,
      server_record_type_count_, form_signature);
}

void FormEventLoggerBase::RecordFunnelMetrics() const {
  UmaHistogramBoolean("Autofill.Funnel.ParsedAsType." + form_type_name_,
                      has_parsed_form_);
  if (!has_parsed_form_) {
    return;
  }
  LogBuffer logs(IsLoggingActive(client_->GetLogManager()));
  LOG_AF(logs) << Tr{} << "Form Type: " << form_type_name_;

  RecordInteractionAfterParsedAsType(logs);
  if (has_logged_interacted_) {
    RecordSuggestionAfterInteraction(logs);
  }
  if (has_logged_interacted_ && has_logged_suggestions_shown_) {
    RecordFillAfterSuggestion(logs);
  }
  if (has_logged_interacted_ && has_logged_suggestions_shown_ &&
      has_logged_suggestion_filled_) {
    RecordSubmissionAfterFill(logs);
  }

  LOG_AF(client_->GetLogManager())
      << LoggingScope::kMetrics << LogMessage::kFunnelMetrics << Tag{"table"}
      << std::move(logs) << CTag{"table"};
}

void FormEventLoggerBase::RecordInteractionAfterParsedAsType(
    LogBuffer& logs) const {
  UmaHistogramBoolean(
      "Autofill.Funnel.InteractionAfterParsedAsType." + form_type_name_,
      has_logged_interacted_);
  LOG_AF(logs) << Tr{} << "InteractionAfterParsedAsType"
               << has_logged_interacted_;
}

void FormEventLoggerBase::RecordSuggestionAfterInteraction(
    LogBuffer& logs) const {
  UmaHistogramBoolean(
      "Autofill.Funnel.SuggestionAfterInteraction." + form_type_name_,
      has_logged_suggestions_shown_);
  LOG_AF(logs) << Tr{} << "SuggestionAfterInteraction"
               << has_logged_suggestions_shown_;
}

void FormEventLoggerBase::RecordFillAfterSuggestion(LogBuffer& logs) const {
  UmaHistogramBoolean("Autofill.Funnel.FillAfterSuggestion." + form_type_name_,
                      has_logged_suggestion_filled_);
  LOG_AF(logs) << Tr{} << "FillAfterSuggestion"
               << has_logged_suggestion_filled_;
}

void FormEventLoggerBase::RecordSubmissionAfterFill(LogBuffer& logs) const {
  UmaHistogramBoolean("Autofill.Funnel.SubmissionAfterFill." + form_type_name_,
                      has_logged_will_submit_);
  LOG_AF(logs) << Tr{} << "SubmissionAfterFill" << has_logged_will_submit_;
}

void FormEventLoggerBase::RecordKeyMetrics() const {
  if (!has_parsed_form_) {
    return;
  }

  LogBuffer logs(IsLoggingActive(client_->GetLogManager()));
  LOG_AF(logs) << Tr{} << "Form Type: " << form_type_name_;

  // Log key success metrics, always preconditioned on a form submission (except
  // for the Autofill.KeyMetrics.FormSubmission metrics which measure whether
  // a submission happens).
  if (has_logged_will_submit_) {
    RecordFillingReadiness(logs);
    if (has_logged_suggestions_shown_) {
      RecordFillingAcceptance(logs);
    }
    if (has_logged_suggestion_filled_) {
      RecordFillingCorrectness(logs);
    }
    RecordFillingAssistance(logs);
    if (form_interactions_ukm_logger_) {
      bool has_logged_data_to_fill_available =
          server_record_type_count_ + local_record_type_count_ > 0;
      form_interactions_ukm_logger_->LogKeyMetrics(
          submitted_form_types_, has_logged_data_to_fill_available,
          has_logged_suggestions_shown_, has_logged_edited_autofilled_field_,
          has_logged_suggestion_filled_, form_interaction_counts_, flow_id_,
          fast_checkout_run_id_);
    }
  }
  if (has_logged_typed_into_non_filled_field_ ||
      has_logged_suggestion_filled_) {
    RecordFormSubmission(logs);
  }

  LOG_AF(client_->GetLogManager())
      << LoggingScope::kMetrics << LogMessage::kKeyMetrics << Tag{"table"}
      << std::move(logs) << CTag{"table"};
}

void FormEventLoggerBase::RecordFillingReadiness(LogBuffer& logs) const {
  bool has_logged_data_to_fill_available =
      server_record_type_count_ + local_record_type_count_ > 0;
  UmaHistogramBoolean("Autofill.KeyMetrics.FillingReadiness." + form_type_name_,
                      has_logged_data_to_fill_available);
  LOG_AF(logs) << Tr{} << "FillingReadiness"
               << has_logged_data_to_fill_available;
}

void FormEventLoggerBase::RecordFillingAcceptance(LogBuffer& logs) const {
  UmaHistogramBoolean(
      "Autofill.KeyMetrics.FillingAcceptance." + form_type_name_,
      has_logged_suggestion_filled_);
  LOG_AF(logs) << Tr{} << "FillingAcceptance" << has_logged_suggestion_filled_;
  UmaHistogramBoolean(
      base::StrCat({"Autofill.Autocomplete.",
                    (has_logged_autocomplete_off_ ? "Off" : "NotOff"),
                    ".FillingAcceptance.", form_type_name_.c_str()}),
      has_logged_suggestion_filled_);
  // Note that `is_heuristic_only_email_form_` will only be true when the
  // `kAutofillEnableEmailHeuristicOnlyAddressForms` feature is enabled and the
  // form meets the requirements expressed in
  // `DetermineHeuristicOnlyEmailFormStatus`.
  if (is_heuristic_only_email_form_) {
    UmaHistogramBoolean("Autofill.EmailHeuristicOnlyAcceptance",
                        has_logged_suggestion_filled_);
  }
}

void FormEventLoggerBase::RecordFillingCorrectness(LogBuffer& logs) const {
  UmaHistogramBoolean(
      "Autofill.KeyMetrics.FillingCorrectness." + form_type_name_,
      !has_logged_edited_autofilled_field_);
  LOG_AF(logs) << Tr{} << "FillingCorrectness"
               << !has_logged_edited_autofilled_field_;
}

void FormEventLoggerBase::RecordFillingAssistance(LogBuffer& logs) const {
  UmaHistogramBoolean(
      "Autofill.KeyMetrics.FillingAssistance." + form_type_name_,
      has_logged_suggestion_filled_);
  LOG_AF(logs) << Tr{} << "FillingAssistance" << has_logged_suggestion_filled_;
}

void FormEventLoggerBase::RecordFormSubmission(LogBuffer& logs) const {
  UmaHistogramBoolean(
      base::StrCat(
          {"Autofill.KeyMetrics.FormSubmission.",
           (has_logged_suggestion_filled_ ? "Autofilled." : "NotAutofilled."),
           form_type_name_}),
      has_logged_will_submit_);
  LOG_AF(logs) << Tr{} << "FormSubmission.Autofilled"
               << has_logged_suggestion_filled_;
  LOG_AF(logs) << Tr{} << "FormSubmission.Submission"
               << has_logged_will_submit_;
}

void FormEventLoggerBase::RecordAblationMetrics() const {
  if (!has_logged_interacted_)
    return;

  // Record whether the form was submitted.

  // AblationGroup::kDefault is mapped to nullptr.
  const char* conditional_ablation_group_string =
      AblationGroupToString(conditional_ablation_group_);
  if (conditional_ablation_group_string) {
    UmaHistogramBoolean(
        base::StrCat({"Autofill.Ablation.FormSubmissionAfterInteraction.",
                      form_type_name_.c_str(), ".Conditional",
                      conditional_ablation_group_string}),
        has_logged_will_submit_);
  }

  // AblationGroup::kDefault is mapped to nullptr.
  const char* ablation_group_string = AblationGroupToString(ablation_group_);
  if (ablation_group_string) {
    UmaHistogramBoolean(
        base::StrCat({"Autofill.Ablation.FormSubmissionAfterInteraction.",
                      form_type_name_.c_str(), ".Unconditional",
                      ablation_group_string}),
        has_logged_will_submit_);
  }

  // Record the submission time since interaction.
  if (time_from_interaction_to_submission_) {
    if (conditional_ablation_group_string) {
      base::UmaHistogramCustomTimes(
          base::StrCat({"Autofill.Ablation.FillDurationSinceInteraction.",
                        form_type_name_.c_str(), ".Conditional",
                        conditional_ablation_group_string}),
          *time_from_interaction_to_submission_, base::Milliseconds(100),
          base::Minutes(10), 50);
    }
    if (ablation_group_string) {
      base::UmaHistogramCustomTimes(
          base::StrCat({"Autofill.Ablation.FillDurationSinceInteraction.",
                        form_type_name_.c_str(), ".Unconditional",
                        ablation_group_string}),
          *time_from_interaction_to_submission_, base::Milliseconds(100),
          base::Minutes(10), 50);
    }
  }
}

void FormEventLoggerBase::OnTextFieldDidChange(
    const FieldGlobalId& field_global_id) {
  if (field_global_id != last_field_global_id_modified_by_user_) {
    ++form_interaction_counts_.form_element_user_modifications;
    last_field_global_id_modified_by_user_ = field_global_id;
    UpdateFlowId();
  }
}

void FormEventLoggerBase::UpdateFlowId() {
  flow_id_ = client_->GetCurrentFormInteractionsFlowId();
}

AutofillMetrics::FormEventSet FormEventLoggerBase::GetFormEvents(
    FormGlobalId form_global_id) {
  return form_events_set_[form_global_id];
}

}  // namespace autofill::autofill_metrics
