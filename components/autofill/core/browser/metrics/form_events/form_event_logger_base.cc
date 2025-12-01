// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/unique_ids.h"

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
  // First, check the prerequisites. The forms for which this classification is
  // applicable  must not run heuristics normally (i.e., their field count is
  // below `kMinRequiredFieldsForHeuristics`), but must be eligible for single
  // field form heuristics.
  if (ShouldRunHeuristics(form) || !ShouldRunHeuristicsForSingleFields(form)) {
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

FormEventLoggerBase::FormEventLoggerBase(std::string form_type_name,
                                         BrowserAutofillManager* owner)
    : form_type_name_(std::move(form_type_name)), owner_(*owner) {}

FormEventLoggerBase::~FormEventLoggerBase() {
  DCHECK(has_called_on_destroyed_);
}

AutofillClient& FormEventLoggerBase::client() {
  return owner_->client();
}

AutofillDriver& FormEventLoggerBase::driver() {
  return owner_->driver();
}

void FormEventLoggerBase::OnDidInteractWithAutofillableForm(
    const FormStructure& form) {
  if (!has_logged_interacted_) {
    has_logged_interacted_ = true;
    LogUkmInteractedWithForm(form.form_signature());
    Log(FORM_EVENT_INTERACTED_ONCE, form);
  }
}

void FormEventLoggerBase::OnDidIdentifyForm(
    const FormStructure& form,
    FormIdentificationTime identification_time) {
  DenseSet<FormTypeNameForLogging> form_types = GetFormTypesForLogging(form);
  CHECK(!form_types.empty());
  switch (identification_time) {
    case FormIdentificationTime::kAfterLocalHeuristics:
      identified_form_types_.insert_all(form_types);
      Log(FORM_EVENT_DID_PARSE_FORM, form);
      RecordParseForm();
      break;
    case FormIdentificationTime::kAfterServerPredictions:
      identified_form_types_.insert_all(form_types);
      break;
  }
}

void FormEventLoggerBase::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    FieldType field_type,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record,
    base::span<const Suggestion> suggestions) {
  client().GetFormInteractionsUkmLogger().LogSuggestionsShown(
      driver().GetPageUkmSourceId(), form, field, form_parsed_timestamp,
      off_the_record);

  Log(FORM_EVENT_SUGGESTIONS_SHOWN, form);
  if (!has_logged_suggestions_shown_) {
    has_logged_suggestions_shown_ = true;
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, form);
    OnSuggestionsShownOnce(form);
  }

  has_logged_autocomplete_off_ |= field.autocomplete_attribute() == "off";

  // Do not mark the field as shown if it was already accepted.
  if (!field_types_with_accepted_suggestions_.contains(field_type)) {
    field_types_with_shown_suggestions_.insert(field_type);
  }

  RecordShowSuggestions();
}

void FormEventLoggerBase::OnDidRefill(const FormStructure& form) {
  Log(FORM_EVENT_DID_DYNAMIC_REFILL, form);
}

void FormEventLoggerBase::SetAblationStatus(
    AblationGroup ablation_group,
    AblationGroup conditional_ablation_group) {
  ablation_group_ = ablation_group;
  // For each form, the ablation group should be stable (except in the rare
  // event that a day boundary is crossed). In practice, it is possible,
  // however, that the conditional_ablation_group is reported as kDefault
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

void FormEventLoggerBase::OnWillSubmitForm(const FormStructure& form) {
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_) {
    return;
  }

  // Not logging twice.
  if (has_logged_will_submit_)
    return;
  has_logged_will_submit_ = true;
  submitted_form_types_ = GetFormTypesForLogging(form);

  // Determine whether logging of email-heuristic only metrics is required.
  is_heuristic_only_email_form_ = (is_heuristic_only_email_form_ ||
                                   DetermineHeuristicOnlyEmailFormStatus(form));

  LogWillSubmitForm(form);

  if (has_logged_suggestions_shown_) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, form);
  }

  RecordUndoMetrics();
  base::RecordAction(base::UserMetricsAction("Autofill_OnWillSubmitForm"));
}

void FormEventLoggerBase::OnFormSubmitted(const FormStructure& form) {
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

void FormEventLoggerBase::OnEditedNonFilledField(FieldGlobalId field_id) {
  has_logged_edited_non_filled_field_ = true;
  OnEditedField(field_id);
}

void FormEventLoggerBase::OnEditedAutofilledField(FieldGlobalId field_id) {
  has_logged_edited_autofilled_field_ = true;
  OnEditedField(field_id);
}

void FormEventLoggerBase::OnDestroyed() {
  DCHECK(!has_called_on_destroyed_);
  has_called_on_destroyed_ = true;
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
  form_events_set_[form.global_id()].insert(event);
  for (FormTypeNameForLogging form_type : GetFormTypesForLogging(form)) {
    std::string name(
        base::StrCat({"Autofill.FormEvents.",
                      FormTypeNameForLoggingToStringView(form_type)}));
    base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);

    // Allow specialized types of logging, e.g. splitting metrics in useful
    // ways.
    OnLog(name, event, form);
  }

  // Log UKM metrics for only autofillable form events.
  if (IsAutofillable(form)) {
    client().GetFormInteractionsUkmLogger().LogFormEvent(
        driver().GetPageUkmSourceId(), event, GetFormTypesForLogging(form),
        form.form_parsed_timestamp());
  }
}

void FormEventLoggerBase::LogWillSubmitForm(const FormStructure& form) {
  if (!has_logged_form_filling_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, form);
  }
}

void FormEventLoggerBase::LogFormSubmitted(const FormStructure& form) {
  if (!has_logged_form_filling_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, form);
  }
}

void FormEventLoggerBase::RecordFunnelMetrics() {
  for (FormTypeNameForLogging form_type :
       GetSupportedFormTypeNamesForLogging()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.Funnel.ParsedAsType.",
                      FormTypeNameForLoggingToStringView(form_type)}),
        !identified_form_types_.empty() &&
            identified_form_types_.contains(form_type));
  }
  if (identified_form_types_.empty()) {
    return;
  }
  LogBuffer logs(IsLoggingActive(client().GetCurrentLogManager()));
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    LOG_AF(logs) << Tr{} << "Form Type: " << form_type;
  }

  RecordInteractionAfterParsedAsType(logs);
  if (has_logged_interacted_) {
    RecordSuggestionAfterInteraction(logs);
  }
  if (has_logged_interacted_ && has_logged_suggestions_shown_) {
    RecordFillAfterSuggestion(logs);
  }
  if (has_logged_interacted_ && has_logged_suggestions_shown_ &&
      has_logged_form_filling_suggestion_filled_) {
    RecordSubmissionAfterFill(logs);
  }

  LOG_AF(client().GetCurrentLogManager())
      << LoggingScope::kMetrics << LogMessage::kFunnelMetrics << Tag{"table"}
      << std::move(logs) << CTag{"table"};
}

void FormEventLoggerBase::RecordInteractionAfterParsedAsType(
    LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Autofill.Funnel.InteractionAfterParsedAsType.", form_type}),
        has_logged_interacted_);
  }
  LOG_AF(logs) << Tr{} << "InteractionAfterParsedAsType"
               << has_logged_interacted_;
}

void FormEventLoggerBase::RecordSuggestionAfterInteraction(
    LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Autofill.Funnel.SuggestionAfterInteraction.", form_type}),
        has_logged_suggestions_shown_);
  }
  LOG_AF(logs) << Tr{} << "SuggestionAfterInteraction"
               << has_logged_suggestions_shown_;
}

void FormEventLoggerBase::RecordFillAfterSuggestion(LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.Funnel.FillAfterSuggestion.", form_type}),
        has_logged_form_filling_suggestion_filled_);
  }
  LOG_AF(logs) << Tr{} << "FillAfterSuggestion"
               << has_logged_form_filling_suggestion_filled_;
}

void FormEventLoggerBase::RecordSubmissionAfterFill(LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.Funnel.SubmissionAfterFill.", form_type}),
        has_logged_will_submit_);
  }
  LOG_AF(logs) << Tr{} << "SubmissionAfterFill" << has_logged_will_submit_;
}

void FormEventLoggerBase::RecordKeyMetrics() {
  if (identified_form_types_.empty()) {
    return;
  }

  LogBuffer logs(IsLoggingActive(client().GetCurrentLogManager()));
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    LOG_AF(logs) << Tr{} << "Form Type: " << form_type;
  }

  // Log key success metrics, always preconditioned on a form submission (except
  // for the Autofill.KeyMetrics.FormSubmission metrics which measure whether
  // a submission happens).
  if (has_logged_will_submit_) {
    RecordFillingReadiness(logs);
    if (has_logged_suggestions_shown_) {
      RecordFillingAcceptance(logs);
    }
    if (has_logged_form_filling_suggestion_filled_) {
      RecordFillingCorrectness(logs);
    }
    RecordFillingAssistance(logs);
    client().GetFormInteractionsUkmLogger().LogKeyMetrics(
        driver().GetPageUkmSourceId(), submitted_form_types_,
        HasLoggedDataToFillAvailable(), has_logged_suggestions_shown_,
        has_logged_edited_autofilled_field_,
        has_logged_form_filling_suggestion_filled_, form_interaction_counts_,
        flow_id_, fast_checkout_run_id_);
  }
  if (has_logged_edited_non_filled_field_ ||
      has_logged_form_filling_suggestion_filled_) {
    RecordFormSubmission(logs);
  }

  LOG_AF(client().GetCurrentLogManager())
      << LoggingScope::kMetrics << LogMessage::kKeyMetrics << Tag{"table"}
      << std::move(logs) << CTag{"table"};
}

void FormEventLoggerBase::RecordFillingReadiness(LogBuffer& logs) const {
  const bool has_logged_data_to_fill_available = HasLoggedDataToFillAvailable();
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.KeyMetrics.FillingReadiness.", form_type}),
        has_logged_data_to_fill_available);
  }
  LOG_AF(logs) << Tr{} << "FillingReadiness"
               << has_logged_data_to_fill_available;
}

void FormEventLoggerBase::RecordFillingAcceptance(LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.KeyMetrics.FillingAcceptance.", form_type}),
        has_logged_form_filling_suggestion_filled_);
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.Autocomplete.",
                      (has_logged_autocomplete_off_ ? "Off" : "NotOff"),
                      ".FillingAcceptance.", form_type}),
        has_logged_form_filling_suggestion_filled_);
  }
  LOG_AF(logs) << Tr{} << "FillingAcceptance"
               << has_logged_form_filling_suggestion_filled_;
  // Note that `is_heuristic_only_email_form_` will only be true when the form
  // meets the requirements expressed in
  // `DetermineHeuristicOnlyEmailFormStatus`.
  if (is_heuristic_only_email_form_) {
    base::UmaHistogramBoolean("Autofill.EmailHeuristicOnlyAcceptance",
                              has_logged_form_filling_suggestion_filled_);
  }

  static constexpr char acceptance_by_focused_field_type_histogram[] =
      "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType";
  for (auto field_type : field_types_with_shown_suggestions_) {
    base::UmaHistogramSparse(acceptance_by_focused_field_type_histogram,
                             GetBucketForAcceptanceMetricsGroupedByFieldType(
                                 field_type, /*suggestion_accepted=*/false));
  }
  for (auto field_type : field_types_with_accepted_suggestions_) {
    base::UmaHistogramSparse(acceptance_by_focused_field_type_histogram,
                             GetBucketForAcceptanceMetricsGroupedByFieldType(
                                 field_type, /*suggestion_accepted=*/true));
  }
}

void FormEventLoggerBase::RecordFillingCorrectness(LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.KeyMetrics.FillingCorrectness.", form_type}),
        !has_logged_edited_autofilled_field_);
  }
  LOG_AF(logs) << Tr{} << "FillingCorrectness"
               << !has_logged_edited_autofilled_field_;
}

void FormEventLoggerBase::RecordFillingAssistance(LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.KeyMetrics.FillingAssistance.", form_type}),
        has_logged_form_filling_suggestion_filled_);
  }
  LOG_AF(logs) << Tr{} << "FillingAssistance"
               << has_logged_form_filling_suggestion_filled_;
}

void FormEventLoggerBase::RecordFormSubmission(LogBuffer& logs) const {
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Autofill.KeyMetrics.FormSubmission.",
             (has_logged_form_filling_suggestion_filled_ ? "Autofilled."
                                                         : "NotAutofilled."),
             form_type}),
        has_logged_will_submit_);
  }
  LOG_AF(logs) << Tr{} << "FormSubmission.Autofilled"
               << has_logged_form_filling_suggestion_filled_;
  LOG_AF(logs) << Tr{} << "FormSubmission.Submission"
               << has_logged_will_submit_;
}

void FormEventLoggerBase::RecordAblationMetrics() const {
  if (!has_logged_interacted_)
    return;

  // Record whether the form was submitted.

  const char* conditional_ablation_group_string =
      AblationGroupToString(conditional_ablation_group_);
  const char* ablation_group_string = AblationGroupToString(ablation_group_);
  for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
    // AblationGroup::kDefault is mapped to nullptr.
    if (conditional_ablation_group_string) {
      base::UmaHistogramBoolean(
          base::StrCat({"Autofill.Ablation.FormSubmissionAfterInteraction.",
                        form_type, ".Conditional",
                        conditional_ablation_group_string}),
          has_logged_will_submit_);
    }

    // AblationGroup::kDefault is mapped to nullptr.
    if (ablation_group_string) {
      base::UmaHistogramBoolean(
          base::StrCat({"Autofill.Ablation.FormSubmissionAfterInteraction.",
                        form_type, ".Unconditional", ablation_group_string}),
          has_logged_will_submit_);
    }

    // Record the submission time since interaction.
    if (time_from_interaction_to_submission_) {
      if (conditional_ablation_group_string) {
        base::UmaHistogramCustomTimes(
            base::StrCat({"Autofill.Ablation.FillDurationSinceInteraction.",
                          form_type, ".Conditional",
                          conditional_ablation_group_string}),
            *time_from_interaction_to_submission_, base::Milliseconds(100),
            base::Minutes(10), 50);
      }
      if (ablation_group_string) {
        base::UmaHistogramCustomTimes(
            base::StrCat({"Autofill.Ablation.FillDurationSinceInteraction.",
                          form_type, ".Unconditional", ablation_group_string}),
            *time_from_interaction_to_submission_, base::Milliseconds(100),
            base::Minutes(10), 50);
      }
    }
  }
}

void FormEventLoggerBase::RecordUndoMetrics() const {
  if (has_logged_form_filling_suggestion_filled_) {
    for (std::string_view form_type : GetParsedFormTypesAsStringViews()) {
      base::UmaHistogramBoolean(
          base::StrCat({"Autofill.UndoAfterFill.", form_type}),
          has_logged_undo_after_fill_);
    }
  }
}

void FormEventLoggerBase::OnEditedField(FieldGlobalId field_id) {
  if (field_id != last_field_global_id_modified_by_user_) {
    ++form_interaction_counts_.form_element_user_modifications;
    last_field_global_id_modified_by_user_ = field_id;
    UpdateFlowId();
  }
}

void FormEventLoggerBase::UpdateFlowId() {
  flow_id_ = client().GetCurrentFormInteractionsFlowId();
}

FormInteractionsUkmLogger::FormEventSet FormEventLoggerBase::GetFormEvents(
    FormGlobalId form_global_id) {
  return form_events_set_[form_global_id];
}

std::vector<std::string_view>
FormEventLoggerBase::GetParsedFormTypesAsStringViews() const {
  std::vector<std::string_view> result;
  for (FormTypeNameForLogging form_type : identified_form_types_) {
    result.push_back(FormTypeNameForLoggingToStringView(form_type));
  }
  return result;
}

}  // namespace autofill::autofill_metrics
