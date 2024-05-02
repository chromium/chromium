// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/quality_metrics.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics_utils.h"
#include "components/autofill/core/browser/metrics/placeholder_metrics.h"
#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

namespace {

void LogNumericQuantityMetrics(const FormStructure& form) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (field->heuristic_type() != NUMERIC_QUANTITY) {
      continue;
    }
    // For every field that has a heuristics prediction for a
    // NUMERIC_QUANTITY, log if there was a colliding server
    // prediction and if the NUMERIC_QUANTITY was a false-positive prediction.
    // The latter is true when the field was correctly filled. This can
    // only be recorded when the feature to grant precedence to
    // NUMERIC_QUANTITY predictions is disabled.
    bool field_has_non_empty_server_prediction =
        field->server_type() != UNKNOWN_TYPE &&
        field->server_type() != NO_SERVER_DATA;
    // Log if there was a colliding server prediction.
    AutofillMetrics::LogNumericQuantityCollidesWithServerPrediction(
        field_has_non_empty_server_prediction);
    // If there was a collision, log if the NUMERIC_QUANTITY was a false
    // positive since the field was correctly filled.
    if ((field->is_autofilled() || field->previously_autofilled()) &&
        field_has_non_empty_server_prediction &&
        !base::FeatureList::IsEnabled(
            features::kAutofillGivePrecedenceToNumericQuantities)) {
      AutofillMetrics::
          LogAcceptedFilledFieldWithNumericQuantityHeuristicPrediction(
              !field->previously_autofilled());
    }
  }
}

void LogPerfectFillingMetric(const FormStructure& form) {
  bool form_has_autofilled_fields = base::ranges::any_of(
      form, [](const auto& field) { return field->is_autofilled(); });
  bool form_has_previously_autofilled_fields = base::ranges::any_of(
      form, [](const auto& field) { return field->previously_autofilled(); });
  // The perfect filling metric is only recorded if Autofill was used on at
  // least one field. This conditions this metric on Assistance, Readiness and
  // Acceptance.
  if (form_has_autofilled_fields || form_has_previously_autofilled_fields) {
    // A perfectly filled form is submitted as it was filled from Autofill
    // without subsequent changes. This means that in a perfect filling
    // scenario, a field is either autofilled, empty, has value at page load or
    // has value set by JS.
    bool perfect_filling = base::ranges::none_of(form, [](const auto& field) {
      return field->is_user_edited() && !field->is_autofilled();
    });
    // Perfect filling is recorded for addresses and credit cards separately.
    // Note that a form can be both an address and a credit card form
    // simultaneously.
    DenseSet<FormType> form_types = form.GetFormTypes();
    if (base::Contains(form_types, FormType::kAddressForm)) {
      AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/true,
                                                 perfect_filling);
    }
    if (base::Contains(form_types, FormType::kCreditCardForm)) {
      AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/false,
                                                 perfect_filling);
    }
  }
}

void LogPreFillMetrics(const FormStructure& form) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    const FormType form_type_of_field =
        FieldTypeGroupToFormType(field->Type().group());
    const bool is_address_form_field =
        form_type_of_field == FormType::kAddressForm;
    const bool credit_card_form_field =
        form_type_of_field == FormType::kCreditCardForm;
    if (is_address_form_field || credit_card_form_field) {
      const std::string_view form_type_name =
          FormTypeToStringView(form_type_of_field);
      LogPreFilledFieldStatus(form_type_name, field->initial_value_changed(),
                              field->Type().GetStorableType());
      LogPreFilledValueChanged(
          form_type_name, field->initial_value_changed(), field->value(),
          field->field_log_events(), field->possible_types(),
          field->Type().GetStorableType(), field->is_autofilled());
      LogPreFilledFieldClassifications(form_type_name,
                                       field->initial_value_changed(),
                                       field->may_use_prefilled_placeholder());
    }
  }
}

void LogFieldFillingStatsAndScoreMetrics(const FormStructure& form) {
  // Tracks how many fields are filled, unfilled or corrected.
  autofill_metrics::FormGroupFillingStats address_field_stats;
  autofill_metrics::FormGroupFillingStats cc_field_stats;
  autofill_metrics::FormGroupFillingStats ac_unrecognized_address_field_stats;
  // Same as above, but keyed by `FillingMethod`.
  base::flat_map<FillingMethod, autofill_metrics::FormGroupFillingStats>
      address_field_stats_by_filling_method;
  for (const std::unique_ptr<AutofillField>& field : form) {
    // For any field that belongs to either an address or a credit card form,
    // collect the type-unspecific field filling statistics.
    // Those are only emitted when autofill was used on at least one field of
    // the form.
    const FormType form_type_of_field =
        FieldTypeGroupToFormType(field->Type().group());
    const bool is_address_form_field =
        form_type_of_field == FormType::kAddressForm;
    const bool credit_card_form_field =
        form_type_of_field == FormType::kCreditCardForm;
    if (!is_address_form_field && !credit_card_form_field) {
      continue;
    }
    // Address and credit cards fields are mutually exclusive.
    autofill_metrics::FormGroupFillingStats& group_stats =
        is_address_form_field ? address_field_stats : cc_field_stats;
    // Get the filling status of this field and add it to the form group
    // counter.
    group_stats.AddFieldFillingStatus(
        autofill_metrics::GetFieldFillingStatus(*field));
    if (is_address_form_field &&
        field->ShouldSuppressSuggestionsAndFillingByDefault()) {
      ac_unrecognized_address_field_stats.AddFieldFillingStatus(
          autofill_metrics::GetFieldFillingStatus(*field));
    }
    // For address forms we want to emit filling stats metrics per
    // `FillingMethod`. Therefore, the stats generated are added to
    // a map keyed by `FillingMethod`, so that later, metrics can
    // emitted for each method used.
    if (base::FeatureList::IsEnabled(
            features::kAutofillGranularFillingAvailable) &
        is_address_form_field) {
      AddFillingStatsForFillingMethod(*field,
                                      address_field_stats_by_filling_method);
    }
  }
  // Log the field filling statistics if autofill was used.
  // The metrics are only emitted if there was at least one field in the
  // corresponding form group that is or was filled by autofill.
  // TODO(crbug.com/40274514): Remove this metric on cleanup.
  autofill_metrics::LogFieldFillingStatsAndScore(
      address_field_stats, cc_field_stats, ac_unrecognized_address_field_stats);
  LogAddressFieldFillingStatsAndScoreByFillingMethod(
      address_field_stats_by_filling_method);
}

// Logs metrics related to how long it took the user from load/interaction time
// till form submission.
void LogDurationMetrics(const FormStructure& form,
                        const base::TimeTicks& load_time,
                        const base::TimeTicks& interaction_time,
                        const base::TimeTicks& submission_time) {
  size_t num_detected_field_types =
      base::ranges::count_if(form, &FieldHasMeaningfulPossibleFieldTypes,
                             &std::unique_ptr<AutofillField>::operator*);
  bool form_has_autofilled_fields = base::ranges::any_of(
      form, [](const auto& field) { return field->is_autofilled(); });
  bool has_observed_one_time_code_field =
      base::ranges::any_of(form, [](const auto& field) {
        return field->Type().html_type() == HtmlFieldType::kOneTimeCode;
      });
  if (num_detected_field_types >= kMinRequiredFieldsForHeuristics ||
      num_detected_field_types >= kMinRequiredFieldsForQuery) {
    // `submission_time` should always be available.
    CHECK(!submission_time.is_null());
    // The |load_time| might be unset, in the case that the form was
    // dynamically added to the DOM.
    // Submission should chronologically follow form load, however
    // this might not be true in case of a timezone change. Therefore make
    // sure to log the elapsed time between submission time and load time only
    // if it is positive. Same is applied below.
    if (!load_time.is_null() && submission_time >= load_time) {
      base::TimeDelta elapsed = submission_time - load_time;
      if (form_has_autofilled_fields) {
        AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(elapsed);
      } else {
        AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(elapsed);
      }
    }
    // The |interaction_time| might be unset, in the case that the user
    // submitted a blank form.
    if (!interaction_time.is_null() && submission_time >= interaction_time) {
      base::TimeDelta elapsed = submission_time - interaction_time;
      AutofillMetrics::LogFormFillDurationFromInteraction(
          form.GetFormTypes(), form_has_autofilled_fields, elapsed);
    }
  }
  if (has_observed_one_time_code_field) {
    if (!load_time.is_null() && submission_time >= load_time) {
      base::TimeDelta elapsed = submission_time - load_time;
      AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(elapsed);
    }
    if (!interaction_time.is_null() && submission_time >= interaction_time) {
      base::TimeDelta elapsed = submission_time - interaction_time;
      AutofillMetrics::LogFormFillDurationFromInteractionForOneTimeCode(
          elapsed);
    }
  }
}

void LogPredictionMetrics(
    const FormStructure& form,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    bool observed_submission) {
  const AutofillMetrics::QualityMetricType metric_type =
      observed_submission ? AutofillMetrics::TYPE_SUBMISSION
                          : AutofillMetrics::TYPE_NO_SUBMISSION;
  for (const std::unique_ptr<AutofillField>& field : form) {
    AutofillMetrics::LogHeuristicPredictionQualityMetrics(
        form_interactions_ukm_logger, form, *field, metric_type);
    AutofillMetrics::LogServerPredictionQualityMetrics(
        form_interactions_ukm_logger, form, *field, metric_type);
    AutofillMetrics::LogOverallPredictionQualityMetrics(
        form_interactions_ukm_logger, form, *field, metric_type);
    AutofillMetrics::LogEmailFieldPredictionMetrics(*field);
    autofill_metrics::LogShadowPredictionComparison(*field);
  }
}

}  // namespace

void LogQualityMetrics(
    const FormStructure& form_structure,
    const base::TimeTicks& load_time,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    bool observed_submission) {
  // Use the same timestamp on UKM Metrics generated within this method's scope.
  AutofillMetrics::UkmTimestampPin timestamp_pin(form_interactions_ukm_logger);

  LogPredictionMetrics(form_structure, form_interactions_ukm_logger,
                       observed_submission);
  if (observed_submission) {
    LogNumericQuantityMetrics(form_structure);
    LogDurationMetrics(form_structure, load_time, interaction_time,
                       submission_time);
    LogPerfectFillingMetric(form_structure);
    LogPreFillMetrics(form_structure);
    LogFieldFillingStatsAndScoreMetrics(form_structure);
  }

  FieldTypeSet autofilled_field_types;

  // Determine the correct suffix for the metric, depending on whether or
  // not a submission was observed.
  const AutofillMetrics::QualityMetricType metric_type =
      observed_submission ? AutofillMetrics::TYPE_SUBMISSION
                          : AutofillMetrics::TYPE_NO_SUBMISSION;

  for (auto& field : form_structure) {
    CHECK(field);
    form_interactions_ukm_logger->LogFieldFillStatus(form_structure, *field,
                                                     metric_type);
    // Field filling statistics that are only emitted if the form was submitted
    // but independent of the existence of a possible type.
    if (observed_submission) {
      // If the field was either autofilled and accepted or corrected, emit the
      // FieldWiseCorrectness metric.
      if (field->is_autofilled() || field->previously_autofilled()) {
        AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
            form_interactions_ukm_logger, form_structure, *field);
      }
    }
    ///////////////////////////////////////////////////////////////////////////
    /// WARNING: Everything below this line is conditioned on having a possible
    /// field type. This means the field must contain a value that can be found
    /// in one of the stored Autofill profiles.
    ///////////////////////////////////////////////////////////////////////////
    const FieldTypeSet& field_types = field->possible_types();
    CHECK(!field_types.empty());
    // Skip all remaining metrics if there wasn't a single possible field type
    // detected.
    if (!FieldHasMeaningfulPossibleFieldTypes(*field)) {
      continue;
    }
    if (field->is_autofilled()) {
      autofilled_field_types.insert(field->Type().GetStorableType());
    }
    if (observed_submission) {
      base::UmaHistogramEnumeration(
          "Autofill.LabelInference.InferredLabelSource.AtSubmission2",
          field->label_source());
    }
  }
  // We log "submission" and duration metrics if we are here after observing a
  // submission event.
  if (observed_submission) {
    if (base::Contains(form_structure.GetFormTypes(),
                       FormType::kCreditCardForm)) {
      AutofillMetrics::LogCreditCardSeamlessnessAtSubmissionTime(
          autofilled_field_types);
    }
  }
}

// Log the quality of the heuristics and server predictions for this form
// structure, if autocomplete attributes are present on the fields (since the
// autocomplete attribute takes precedence over other type predictions).
void LogQualityMetricsBasedOnAutocomplete(
    const FormStructure& form_structure,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  const AutofillMetrics::QualityMetricType metric_type =
      AutofillMetrics::TYPE_AUTOCOMPLETE_BASED;
  for (const auto& field : form_structure) {
    if (field->html_type() != HtmlFieldType::kUnspecified &&
        field->html_type() != HtmlFieldType::kUnrecognized) {
      AutofillMetrics::LogHeuristicPredictionQualityMetrics(
          form_interactions_ukm_logger, form_structure, *field, metric_type);
      AutofillMetrics::LogServerPredictionQualityMetrics(
          form_interactions_ukm_logger, form_structure, *field, metric_type);
    }
  }
}

autofill_metrics::FormGroupFillingStats GetAddressFormFillingStats(
    const FormStructure& form_structure) {
  autofill_metrics::FormGroupFillingStats address_field_stats;

  for (auto& field : form_structure) {
    if (FieldTypeGroupToFormType(field->Type().group()) !=
        FormType::kAddressForm) {
      continue;
    }
    address_field_stats.AddFieldFillingStatus(
        autofill_metrics::GetFieldFillingStatus(*field));
  }
  return address_field_stats;
}

}  // namespace autofill::autofill_metrics
