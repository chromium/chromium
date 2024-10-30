// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_INTERACTIONS_UKM_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_INTERACTIONS_UKM_LOGGER_H_

#include <optional>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm::builders {
class Autofill_CreditCardFill;
}

namespace autofill::autofill_metrics {

// Utility to log URL keyed form interaction events.
class FormInteractionsUkmLogger {
 public:
  FormInteractionsUkmLogger(AutofillClient* autofill_client,
                            ukm::UkmRecorder* ukm_recorder);

  bool has_pinned_timestamp() const { return !pinned_timestamp_.is_null(); }
  void set_pinned_timestamp(base::TimeTicks t) { pinned_timestamp_ = t; }

  ukm::builders::Autofill_CreditCardFill CreateCreditCardFillBuilder();
  void Record(ukm::builders::Autofill_CreditCardFill&& builder);

  // Initializes this logger with a source_id. Unless forms is parsed no
  // autofill UKM is recorded. However due to autofill_manager resets,
  // it is possible to have the UKM being recorded after the forms were
  // parsed. So, rely on autofill_client to pass correct source_id
  // However during some cases there is a race for setting AutofillClient
  // and generation of new source_id (by UKM) as they are both observing tab
  // navigation. Ideally we need to refactor ownership of this logger
  // so as not to rely on OnFormsParsed to record the metrics correctly.
  // TODO(nikunjb): Refactor the logger to be owned by AutofillClient.
  void OnFormsParsed(const ukm::SourceId source_id);
  void LogInteractedWithForm(bool is_for_credit_card,
                             size_t local_record_type_count,
                             size_t server_record_type_count,
                             FormSignature form_signature);
  void LogSuggestionsShown(const FormStructure& form,
                           const AutofillField& field,
                           base::TimeTicks form_parsed_timestamp,
                           bool off_the_record);
  // For address suggestions, the `record_type` is irrelevant.
  void LogDidFillSuggestion(
      const FormStructure& form,
      const AutofillField& field,
      std::optional<CreditCard::RecordType> record_type = std::nullopt);
  void LogTextFieldDidChange(const FormStructure& form,
                             const AutofillField& field);
  void LogEditedAutofilledFieldAtSubmission(const FormStructure& form,
                                            const AutofillField& field);
  void LogFieldFillStatus(const FormStructure& form,
                          const AutofillField& field,
                          AutofillMetrics::QualityMetricType metric_type);
  void LogFieldType(
      base::TimeTicks form_parsed_timestamp,
      FormSignature form_signature,
      FieldSignature field_signature,
      AutofillMetrics::QualityMetricPredictionSource prediction_source,
      AutofillMetrics::QualityMetricType metric_type,
      FieldType predicted_type,
      FieldType actual_type);
  void LogAutofillFieldInfoAtFormRemove(
      const FormStructure& form,
      const AutofillField& field,
      AutofillMetrics::AutocompleteState autocomplete_state);
  void LogAutofillFormSummaryAtFormRemove(
      const FormStructure& form_structure,
      AutofillMetrics::FormEventSet form_events,
      base::TimeTicks initial_interaction_timestamp,
      base::TimeTicks form_submitted_timestamp);
  void LogAutofillFormWithExperimentalFieldsCountAtFormRemove(
      const FormStructure& form_structure);
  void LogFocusedComplexFormAtFormRemove(
      const FormStructure& form_structure,
      AutofillMetrics::FormEventSet form_events,
      base::TimeTicks initial_interaction_timestamp,
      base::TimeTicks form_submitted_timestamp);
  void LogKeyMetrics(const DenseSet<FormTypeNameForLogging>& form_types,
                     bool data_to_fill_available,
                     bool suggestions_shown,
                     bool edited_autofilled_field,
                     bool suggestion_filled,
                     const FormInteractionCounts& form_interaction_counts,
                     const FormInteractionsFlowId& flow_id,
                     std::optional<int64_t> fast_checkout_run_id);
  void LogFormEvent(autofill_metrics::FormEvent form_event,
                    const DenseSet<FormTypeNameForLogging>& form_types,
                    base::TimeTicks form_parsed_timestamp);

  // Logs whether the autofill decided to skip or to fill each
  // hidden/representational field.
  void LogHiddenRepresentationalFieldSkipDecision(const FormStructure& form,
                                                  const AutofillField& field,
                                                  bool is_skipped);

  // Logs the fields for which the autofill decided to rationalize the server
  // type predictions due to repetition of the type.
  void LogRepeatedServerTypePredictionRationalized(
      const FormSignature form_signature,
      const AutofillField& field,
      FieldType old_type);

  // Logs a hash of the `sectioning_signature` for a specific
  // `form_signature`. This is useful for detecting sites where different
  // sectioning algorithms yield different results. Emitted every time
  // sectioning is performed and only when
  // `AutofillUseParameterizedSectioning` is enabled.
  void LogSectioningHash(FormSignature form_signature,
                         uint32_t sectioning_signature);

 private:
  bool CanLog() const;
  int64_t MillisecondsSinceFormParsed(
      base::TimeTicks form_parsed_timestamp) const;

  ukm::SourceId GetSourceId();

  // These objects outlive.
  raw_ptr<AutofillClient> autofill_client_;
  raw_ptr<ukm::UkmRecorder> ukm_recorder_;

  std::optional<ukm::SourceId> source_id_;
  base::TimeTicks pinned_timestamp_;
};

// This defines a second-to-minute-scale prioritized set of buckets for
// recording user interaction time with forms. Pure exponential bucketing is
// generally not appropriate for analyzing interactions at this time scale, as
// we tend not to care about durations at the millisecond level, while small
// changes at the 2-3 minute scale may be invisible with exponential buckets.
// This set of buckets contains a large linear section between 1 and 30s, and
// between 30s and 10m, after which it proceeds in the same way as
// ukm::GetSemanticBucketMinForDurationTiming
int64_t GetSemanticBucketMinForAutofillDurationTiming(int64_t sample);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_INTERACTIONS_UKM_LOGGER_H_
