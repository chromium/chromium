// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class AutofillClient;
class AutofillDriver;
class BrowserAutofillManager;
}  // namespace autofill

namespace autofill::autofill_metrics {

// Utility to log autofill form events in the relevant histograms depending on
// the presence of server and/or local data.
class FormEventLoggerBase {
 public:
  FormEventLoggerBase(std::string form_type_name,
                      BrowserAutofillManager* owner);

  void OnDidInteractWithAutofillableForm(const FormStructure& form);

  void OnDidPollSuggestions(FieldGlobalId field_id);

  void OnDidParseForm(const FormStructure& form);

  virtual void OnDidShowSuggestions(const FormStructure& form,
                                    const AutofillField& field,
                                    base::TimeTicks form_parsed_timestamp,
                                    bool off_the_record);

  void OnDidRefill(const FormStructure& form);

  void OnWillSubmitForm(const FormStructure& form);

  void OnFormSubmitted(const FormStructure& form);

  // Called when a field gets edited (the choice of the function depends on
  // whether the field was autofilled or not prior to the edit). This covers:
  // - User manual modifications of the value of text fields.
  // - User manual modifications of the value of select fields.
  // - JS modifications of select fields on frames with transient user
  //   activation (see blink::LocalFrame::HasTransientUserActivation).
  // Note that this means that any JS modification of text fields doesn't
  // trigger these methods.
  void OnEditedNonFilledField(FieldGlobalId field_id);
  void OnEditedAutofilledField(FieldGlobalId field_id);

  // Must be called right before the event logger is destroyed. It triggers the
  // logging of funnel and key metrics.
  // The function must not be called from the destructor, since this makes it
  // impossible to dispatch virtual functions into the derived classes.
  void OnDestroyed();

  // Adds the appropriate form types based on `type` to
  // `field_by_field_filled_form_types_` after a filling operation.
  void OnFilledByFieldByFieldFilling(SuggestionType type);

  // See BrowserAutofillManager::SuggestionContext for the definitions of the
  // AblationGroup parameters.
  void SetAblationStatus(AblationGroup ablation_group,
                         AblationGroup conditional_ablation_group);
  void SetTimeFromInteractionToSubmission(
      base::TimeDelta time_from_interaction_to_submission);

  void OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(
      const FormStructure& form);

  virtual void Log(FormEvent event, const FormStructure& form);

  void SetFastCheckoutRunId(int64_t run_id) { fast_checkout_run_id_ = run_id; }

  FormInteractionsUkmLogger::FormEventSet GetFormEvents(
      FormGlobalId form_global_id);

  const FormInteractionsFlowId& form_interactions_flow_id_for_test() const {
    return flow_id_;
  }

  const std::optional<int64_t> fast_checkout_run_id_for_test() const {
    return fast_checkout_run_id_;
  }

 protected:
  virtual ~FormEventLoggerBase();

  AutofillClient& client();
  AutofillDriver& driver();

  virtual void RecordPollSuggestions() = 0;
  virtual void RecordParseForm() = 0;
  virtual void RecordShowSuggestions() = 0;

  // Shared logic of `OnEdited[NonFilled|Autofilled]Field`, called irrespective
  // of the autofill state of the field represented by `field_global_id`.
  void OnEditedField(FieldGlobalId field_id);

  virtual void LogWillSubmitForm(const FormStructure& form);
  virtual void LogFormSubmitted(const FormStructure& form);

  // Only used for UKM backward compatibility since it depends on IsCreditCard.
  // TODO (crbug.com/925913): Remove IsCreditCard from UKM logs amd replace with
  // |form_type_name_|.
  virtual void LogUkmInteractedWithForm(FormSignature form_signature) = 0;

  virtual void OnSuggestionsShownOnce(const FormStructure& form) {}
  virtual void OnSuggestionsShownSubmittedOnce(const FormStructure& form) {}

  // Logs |event| in a histogram prefixed with |name| according to the
  // FormEventLogger type and |form|. For example, in the address context, it
  // may be useful to analyze metrics for forms (A) with only name and address
  // fields and (B) with only name and phone fields separately.
  virtual void OnLog(const std::string& name,
                     FormEvent event,
                     const FormStructure& form) const {}

  // Records UMA metrics on the funnel and writes logs to autofill-internals.
  void RecordFunnelMetrics();

  // For each funnel metric, a separate function is defined below.
  // `RecordFunnelMetrics()` checks the necessary pre-conditions for metrics to
  // be emitted and calls the relevant functions.
  void RecordInteractionAfterParsedAsType(LogBuffer& logs) const;
  void RecordSuggestionAfterInteraction(LogBuffer& logs) const;
  void RecordFillAfterSuggestion(LogBuffer& logs) const;
  void RecordSubmissionAfterFill(LogBuffer& logs) const;

  // Records UMA metrics on key metrics and writes logs to autofill-internals.
  // Similar to the funnel metrics, a separate function for each key metric is
  // defined below.
  void RecordKeyMetrics();

  // Whether for a submitted form, Chrome had data stored that could be
  // filled.
  virtual void RecordFillingReadiness(LogBuffer& logs) const;

  // Whether a user accepted a filling suggestion they saw for a form that
  // was later submitted.
  void RecordFillingAcceptance(LogBuffer& logs) const;

  // Whether a filled form and submitted form required no fixes to filled
  // fields.
  virtual void RecordFillingCorrectness(LogBuffer& logs) const;

  // Whether a submitted form was filled.
  virtual void RecordFillingAssistance(LogBuffer& logs) const;

  // Whether a (non-)autofilled form was submitted.
  void RecordFormSubmission(LogBuffer& logs) const;

  // Records UMA metrics if this form submission happened as part of an ablation
  // study or the corresponding control group. This is not virtual because it is
  // called in the destructor.
  void RecordAblationMetrics() const;

  // Records UMA metrics related to the Undo Autofill feature.
  void RecordUndoMetrics() const;

  void UpdateFlowId();

  // Returns whether the logger was notified that any data to fill is available.
  // This is used to emit the readiness key metric.
  virtual bool HasLoggedDataToFillAvailable() const = 0;

  // Returns the set of all the form types the form event logger should log.
  // This is to avoid the credit card form event logger from logging address
  // related form types.
  virtual DenseSet<FormTypeNameForLogging> GetSupportedFormTypeNamesForLogging()
      const = 0;

  // Returns the set of all form types the form event logger should log for
  // `form.`
  virtual DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
      const FormStructure& form) const = 0;

  // Returns a vector of strings for all parsed form types.
  std::vector<std::string_view> GetParsedFormTypesAsStringViews() const;

  // Returns a set of all parsed form types and form types of field-by-field
  // filling operations.
  DenseSet<FormTypeNameForLogging> GetParsedAndFieldByFieldFormTypes() const;

  // Constructor parameters.
  std::string form_type_name_;

  // State variables.
  bool has_parsed_form_ = false;
  bool has_logged_interacted_ = false;
  bool has_logged_user_hide_suggestions_ = false;
  bool has_logged_suggestions_shown_ = false;
  bool has_logged_form_filling_suggestion_filled_ = false;
  bool has_logged_undo_after_fill_ = false;
  bool has_logged_autocomplete_off_ = false;
  bool has_logged_will_submit_ = false;
  bool has_logged_submitted_ = false;
  bool has_logged_edited_non_filled_field_ = false;
  bool has_logged_edited_autofilled_field_ = false;
  bool has_logged_autofilled_field_was_cleared_by_javascript_after_fill_ =
      false;
  bool has_called_on_destroyed_ = false;
  bool is_heuristic_only_email_form_ = false;
  AblationGroup ablation_group_ = AblationGroup::kDefault;
  AblationGroup conditional_ablation_group_ = AblationGroup::kDefault;
  std::optional<base::TimeDelta> time_from_interaction_to_submission_;

  // The ID of the last field that was polled for suggestions.
  FieldGlobalId last_polled_field_id_;

  // Used to count consecutive modifications on the same field as one change.
  FieldGlobalId last_field_global_id_modified_by_user_;
  // Keeps counts of Autofill fills and form elements that were modified by the
  // user.
  FormInteractionCounts form_interaction_counts_ = {};
  // Unique random id that is set on the first form interaction and identical
  // during the flow.
  FormInteractionsFlowId flow_id_;
  // Unique ID of a Fast Checkout run. Used for metrics.
  std::optional<int64_t> fast_checkout_run_id_;

  // Form types of the parsed forms for logging purposes.
  DenseSet<FormTypeNameForLogging> parsed_form_types_;

  // Form types of the submitted form.
  DenseSet<FormTypeNameForLogging> submitted_form_types_;

  // Form types of field-by-field filling operations.
  DenseSet<FormTypeNameForLogging> field_by_field_filled_form_types_;

  // A list of field types for which suggestions were shown and not accepted so
  // far. At any time, no field should be in both
  // `field_types_with_shown_suggestions_` and
  // `field_types_with_accepted_suggestions_`.
  FieldTypeSet field_types_with_shown_suggestions_;

  // A list of field types for which suggestions were accepted. At any time, no
  // field should be in both `field_types_with_shown_suggestions_` and
  // `field_types_with_accepted_suggestions_`.
  FieldTypeSet field_types_with_accepted_suggestions_;

  // A map of the form's global id and its form events.
  std::map<FormGlobalId, FormInteractionsUkmLogger::FormEventSet>
      form_events_set_;

  // Weak reference.
  const raw_ref<BrowserAutofillManager> owner_;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
