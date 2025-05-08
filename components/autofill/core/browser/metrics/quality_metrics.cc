// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/quality_metrics.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"
#include "components/autofill/core/browser/metrics/quality_metrics_filling.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "third_party/icu/source/common/unicode/uscript.h"

namespace autofill::autofill_metrics {

namespace {

void LogPerfectFillingMetric(const FormStructure& form) {
  // Denotes whether for a given FillingProduct, the form has a field which was
  // last filled with this product (and maybe user/JS edited afterwards).
  const base::flat_map<FillingProduct, bool> filling_product_was_used =
      base::MakeFlatMap<FillingProduct, bool>(
          base::span<const FillingProduct, 2>(
              {FillingProduct::kAddress, FillingProduct::kCreditCard}),
          {}, [&form](FillingProduct filling_product) {
            return std::make_pair(
                filling_product,
                std::ranges::any_of(
                    form, [&filling_product](const auto& field) {
                      return field->filling_product() == filling_product;
                    }));
          });
  // A perfectly filled form is submitted as it was filled from Autofill
  // without subsequent changes. This means that in a perfect filling
  // scenario, a field is either autofilled, empty, has value at page load or
  // has value set by JS.
  const bool perfect_filling =
      std::ranges::none_of(form, [](const auto& field) {
        return field->is_user_edited() && !field->is_autofilled();
      });
  // The perfect filling metric is only recorded if Autofill was used on at
  // least one field. This conditions this metric on Assistance, Readiness and
  // Acceptance. Perfect filling is recorded for addresses and credit cards
  // separately.
  if (filling_product_was_used.at(FillingProduct::kAddress)) {
    AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/true,
                                               perfect_filling);
  }
  if (filling_product_was_used.at(FillingProduct::kCreditCard)) {
    AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/false,
                                               perfect_filling);
  }
}

// Logs metrics related to how long it took the user from load/interaction time
// till form submission.
void LogDurationMetrics(const FormStructure& form,
                        base::TimeTicks load_time,
                        base::TimeTicks interaction_time,
                        base::TimeTicks submission_time) {
  size_t num_detected_field_types =
      std::ranges::count_if(form, &FieldHasMeaningfulPossibleFieldTypes,
                            &std::unique_ptr<AutofillField>::operator*);
  bool form_has_autofilled_fields = std::ranges::any_of(
      form, [](const auto& field) { return field->is_autofilled(); });
  bool has_observed_one_time_code_field =
      std::ranges::any_of(form, [](const auto& field) {
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

// Returns the character set of the submitted value for the alternative name
// field.
AutofillAlternativeNameFieldValueCharacterSet
GetAlternativeNameFieldValueCharacterSet(
    const std::u16string& submitted_value) {
  UErrorCode error = U_ZERO_ERROR;
  for (base::i18n::UTF16CharIterator iter(submitted_value); !iter.end();
       iter.Advance()) {
    if (uscript_getScript(iter.get(), &error) == USCRIPT_KATAKANA) {
      return AutofillAlternativeNameFieldValueCharacterSet::kKatakana;
    } else if (uscript_getScript(iter.get(), &error) == USCRIPT_HIRAGANA) {
      return AutofillAlternativeNameFieldValueCharacterSet::kHiragana;
    }
  }
  return AutofillAlternativeNameFieldValueCharacterSet::kOther;
}

// Records the character set of the submitted value for each alternative name
// field in the form.
void LogSubmittedAlternativeNameCharacterSetValues(const FormStructure& form) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return;
  }
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (IsAlternativeNameType(field->Type().GetStorableType()) &&
        !field->value().empty()) {
      base::UmaHistogramEnumeration(
          "Autofill.SubmittedAlternativeNameFieldValueCharacterSet",
          GetAlternativeNameFieldValueCharacterSet(field->value()));
    }
  }
}

void LogExtractionMetrics(const FormStructure& form) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    CHECK(!field->possible_types().empty());
    if (FieldHasMeaningfulPossibleFieldTypes(*field)) {
      base::UmaHistogramEnumeration(
          "Autofill.LabelInference.InferredLabelSource.AtSubmission2",
          field->label_source());
    }
  }
}

void LogPredictionMetrics(
    const FormStructure& form,
    FormInteractionsUkmLogger& form_interactions_ukm_logger,
    ukm::SourceId source_id,
    bool observed_submission) {
  const QualityMetricType metric_type =
      observed_submission ? TYPE_SUBMISSION : TYPE_NO_SUBMISSION;
  for (const std::unique_ptr<AutofillField>& field : form) {
    LogHeuristicPredictionQualityMetrics(form_interactions_ukm_logger,
                                         source_id, form, *field, metric_type);
    LogServerPredictionQualityMetrics(form_interactions_ukm_logger, source_id,
                                      form, *field, metric_type);
    LogOverallPredictionQualityMetrics(form_interactions_ukm_logger, source_id,
                                       form, *field, metric_type);
    LogEmailFieldPredictionMetrics(*field);
    LogFieldPredictionOverlapMetrics(*field);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    // If ML predictions are the active heuristic source, don't record samples
    // as these would be redundant to the ".Heuristic" sub-metric of
    // `LogHeuristicPredictionQualityMetrics()`.
    if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
        GetActiveHeuristicSource() !=
            HeuristicSource::kAutofillMachineLearning) {
      LogMlPredictionQualityMetrics(form_interactions_ukm_logger, source_id,
                                    form, *field, metric_type);
    }
#endif
  }
}

void LogFillingMetrics(const FormStructure& form,
                       FormInteractionsUkmLogger& form_interactions_ukm_logger,
                       ukm::SourceId source_id,
                       bool observed_submission) {
  const QualityMetricType metric_type =
      observed_submission ? TYPE_SUBMISSION : TYPE_NO_SUBMISSION;
  for (const std::unique_ptr<AutofillField>& field : form) {
    form_interactions_ukm_logger.LogFieldFillStatus(source_id, form, *field,
                                                    metric_type);
  }
  if (!observed_submission) {
    return;
  }
  LogPerfectFillingMetric(form);
  LogFieldFillingStatsAndScore(form);
  LogFillingQualityMetrics(form);

  FieldTypeSet autofilled_field_types;
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (field->is_autofilled() || field->previously_autofilled()) {
      AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
          form_interactions_ukm_logger, source_id, form, *field);
    }
    if (FieldHasMeaningfulPossibleFieldTypes(*field) &&
        field->is_autofilled()) {
      autofilled_field_types.insert(field->Type().GetStorableType());
    }
  }
  if (base::Contains(form.GetFormTypes(), FormType::kCreditCardForm)) {
    AutofillMetrics::LogCreditCardSeamlessnessAtSubmissionTime(
        autofilled_field_types);
  }
}

}  // namespace

void LogQualityMetrics(const FormStructure& form_structure,
                       base::TimeTicks load_time,
                       base::TimeTicks interaction_time,
                       base::TimeTicks submission_time,
                       FormInteractionsUkmLogger& form_interactions_ukm_logger,
                       ukm::SourceId source_id,
                       bool observed_submission) {
  // Use the same timestamp on UKM Metrics generated within this method's scope.
  UkmTimestampPin timestamp_pin(&form_interactions_ukm_logger);

  LogPredictionMetrics(form_structure, form_interactions_ukm_logger, source_id,
                       observed_submission);
  LogFillingMetrics(form_structure, form_interactions_ukm_logger, source_id,
                    observed_submission);
  if (observed_submission) {
    // TODO(crbug.com/359768803): Remove this metric once the feature is
    // launched.
    LogSubmittedAlternativeNameCharacterSetValues(form_structure);
    LogExtractionMetrics(form_structure);
    LogDurationMetrics(form_structure, load_time, interaction_time,
                       submission_time);
  }
}

// Log the quality of the heuristics and server predictions for this form
// structure, if autocomplete attributes are present on the fields (since the
// autocomplete attribute takes precedence over other type predictions).
void LogQualityMetricsBasedOnAutocomplete(
    const FormStructure& form_structure,
    FormInteractionsUkmLogger& form_interactions_ukm_logger,
    ukm::SourceId source_id) {
  const QualityMetricType metric_type = TYPE_AUTOCOMPLETE_BASED;
  for (const auto& field : form_structure) {
    if (field->html_type() != HtmlFieldType::kUnspecified &&
        field->html_type() != HtmlFieldType::kUnrecognized) {
      LogHeuristicPredictionQualityMetrics(form_interactions_ukm_logger,
                                           source_id, form_structure, *field,
                                           metric_type);
      LogServerPredictionQualityMetrics(form_interactions_ukm_logger, source_id,
                                        form_structure, *field, metric_type);
    }
  }
}

}  // namespace autofill::autofill_metrics
