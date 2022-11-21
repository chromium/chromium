// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"

namespace autofill {

// Utility to log autofill form events in the relevant histograms depending on
// the presence of server and/or local data.
class FormEventLoggerBase {
 public:
  FormEventLoggerBase(
      const std::string& form_type_name,
      bool is_in_any_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      AutofillClient* client);

  inline void set_server_record_type_count(size_t server_record_type_count) {
    server_record_type_count_ = server_record_type_count;
  }

  inline void set_local_record_type_count(size_t local_record_type_count) {
    local_record_type_count_ = local_record_type_count;
  }

  void OnDidInteractWithAutofillableForm(const FormStructure& form,
                                         AutofillSyncSigninState sync_state);

  void OnDidPollSuggestions(const FormFieldData& field,
                            AutofillSyncSigninState sync_state);

  void OnDidParseForm(const FormStructure& form);

  void OnPopupSuppressed(const FormStructure& form, const AutofillField& field);

  void OnUserHideSuggestions(const FormStructure& form,
                             const AutofillField& field);

  virtual void OnDidShowSuggestions(
      const FormStructure& form,
      const AutofillField& field,
      const base::TimeTicks& form_parsed_timestamp,
      AutofillSyncSigninState sync_state,
      bool off_the_record);

  void OnWillSubmitForm(AutofillSyncSigninState sync_state,
                        const FormStructure& form);

  void OnFormSubmitted(AutofillSyncSigninState sync_state,
                       const FormStructure& form);

  void OnTypedIntoNonFilledField();
  void OnEditedAutofilledField();

  // See BrowserAutofillManager::SuggestionContext for the definitions of the
  // AblationGroup parameters.
  void SetAblationStatus(AblationGroup ablation_group,
                         AblationGroup conditional_ablation_group);
  void SetTimeFromInteractionToSubmission(
      base::TimeDelta time_from_interaction_to_submission);

  void OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(
      const FormStructure& form);

  void Log(FormEvent event, const FormStructure& form) const;

  void OnTextFieldDidChange(const FieldGlobalId& field_global_id);

  const FormInteractionCounts& form_interaction_counts() const {
    return form_interaction_counts_;
  }

  const FormInteractionsFlowId& form_interactions_flow_id_for_test() const {
    return flow_id_;
  }

 protected:
  virtual ~FormEventLoggerBase();

  virtual void RecordPollSuggestions() = 0;
  virtual void RecordParseForm() = 0;
  virtual void RecordShowSuggestions() = 0;

  virtual void LogWillSubmitForm(const FormStructure& form);
  virtual void LogFormSubmitted(const FormStructure& form);

  // This is a temporary analysis for crbug.com/1352826. We apply local
  // heuristics to forms if >= 3 fields are discovered by local heuristics. The
  // working hypothesis is that we should change this to ">= 3 distinct field
  // types are discovered by local heuristics". To test this hypothesis we want
  // to calculate the FillingAcceptance for forms for which the stricter
  // rule would make a difference.
  // TODO(crbug.com/1352826): Remove this after investigating the impact.
  void LogImpactOfHeuristicsThreshold(const FormStructure& form);

  // Only used for UKM backward compatibility since it depends on IsCreditCard.
  // TODO (crbug.com/925913): Remove IsCreditCard from UKM logs amd replace with
  // |form_type_name_|.
  virtual void LogUkmInteractedWithForm(FormSignature form_signature);

  virtual void OnSuggestionsShownOnce(const FormStructure& form) {}
  virtual void OnSuggestionsShownSubmittedOnce(const FormStructure& form) {}

  // Logs |event| in a histogram prefixed with |name| according to the
  // FormEventLogger type and |form|. For example, in the address context, it
  // may be useful to analyze metrics for forms (A) with only name and address
  // fields and (B) with only name and phone fields separately.
  virtual void OnLog(const std::string& name,
                     FormEvent event,
                     const FormStructure& form) const {}

  // Records UMA metrics on the funnel and key metrics. This is not virtual
  // because it is called in the destructor.
  void RecordFunnelAndKeyMetrics();

  // Records UMA metrics if this form submission happened as part of an ablation
  // study or the corresponding control group. This is not virtual because it is
  // called in the destructor.
  void RecordAblationMetrics();

  void UpdateFlowId();

  // Constructor parameters.
  std::string form_type_name_;
  bool is_in_any_main_frame_;

  // State variables.
  size_t server_record_type_count_ = 0;
  size_t local_record_type_count_ = 0;
  bool has_parsed_form_ = false;
  bool has_logged_interacted_ = false;
  bool has_logged_popup_suppressed_ = false;
  bool has_logged_user_hide_suggestions_ = false;
  bool has_logged_suggestions_shown_ = false;
  bool has_logged_suggestion_filled_ = false;
  bool has_logged_autocomplete_off_ = false;
  bool has_logged_will_submit_ = false;
  bool has_logged_submitted_ = false;
  bool logged_suggestion_filled_was_server_data_ = false;
  bool has_logged_typed_into_non_filled_field_ = false;
  bool has_logged_edited_autofilled_field_ = false;
  bool has_logged_autofilled_field_was_cleared_by_javascript_after_fill_ =
      false;
  AblationGroup ablation_group_ = AblationGroup::kDefault;
  AblationGroup conditional_ablation_group_ = AblationGroup::kDefault;
  absl::optional<base::TimeDelta> time_from_interaction_to_submission_;

  // The last field that was polled for suggestions.
  FormFieldData last_polled_field_;

  // Used to count consecutive modifications on the same field as one change.
  FieldGlobalId last_field_global_id_modified_by_user_;
  // Keeps counts of Autofill fills and form elements that were modified by the
  // user.
  FormInteractionCounts form_interaction_counts_ = {};
  // Unique random id that is set on the first form interaction and identical
  // during the flow.
  FormInteractionsFlowId flow_id_;

  // Form types of the submitted form
  DenseSet<FormType> submitted_form_types_;

  // Weak reference.
  raw_ptr<AutofillMetrics::FormInteractionsUkmLogger>
      form_interactions_ukm_logger_;

  // Weak reference.
  const raw_ref<AutofillClient> client_;

  AutofillSyncSigninState sync_state_ = AutofillSyncSigninState::kNumSyncStates;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
