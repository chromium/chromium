// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PREDICTION_QUALITY_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PREDICTION_QUALITY_METRICS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

class FormInteractionsUkmLogger;

// Metrics measuring how well we predict field types.  These metric values are
// logged for each field in a submitted form for:
//     - the heuristic prediction
//     - the crowd-sourced (server) prediction
//     - for the overall prediction
//
// For each of these prediction types, these metrics are also logged by
// actual and predicted field type.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FieldTypeQualityMetric {
  // The field was found to be of type T, which matches the predicted type.
  // i.e. actual_type == predicted type == T
  //
  // This is captured as a type-specific log entry for T. Is is also captured
  // as an aggregate (non-type-specific) log entry.
  TRUE_POSITIVE = 0,

  // The field type is AMBIGUOUS and autofill made no prediction.
  // i.e. actual_type == AMBIGUOUS,predicted type == UNKNOWN|NO_SERVER_DATA.
  //
  // This is captured as an aggregate (non-type-specific) log entry. It is
  // NOT captured by type-specific logging.
  TRUE_NEGATIVE_AMBIGUOUS = 1,

  // The field type is UNKNOWN and autofill made no prediction.
  // i.e. actual_type == UNKNOWN and predicted type == UNKNOWN|NO_SERVER_DATA.
  //
  // This is captured as an aggregate (non-type-specific) log entry. It is
  // NOT captured by type-specific logging.
  TRUE_NEGATIVE_UNKNOWN = 2,

  // The field type is EMPTY and autofill predicted UNKNOWN
  // i.e. actual_type == EMPTY and predicted type == UNKNOWN|NO_SERVER_DATA.
  //
  // This is captured as an aggregate (non-type-specific) log entry. It is
  // NOT captured by type-specific logging.
  TRUE_NEGATIVE_EMPTY = 3,

  // Autofill predicted type T, but the field actually had a different type.
  // i.e., actual_type == T, predicted_type = U, T != U,
  //       UNKNOWN not in (T,U).
  //
  // This is captured as a type-specific log entry for U. It is NOT captured
  // as an aggregate (non-type-specific) entry as this would double count with
  // FALSE_NEGATIVE_MISMATCH logging captured for T.
  FALSE_POSITIVE_MISMATCH = 4,

  // Autofill predicted type T, but the field actually matched multiple
  // pieces of autofill data, none of which are T.
  // i.e., predicted_type == T, actual_type = {U, V, ...),
  //       T not in {U, V, ...}.
  //
  // This is captured as a type-specific log entry for T. It is also captured
  // as an aggregate (non-type-specific) log entry.
  FALSE_POSITIVE_AMBIGUOUS = 5,

  // The field type is UNKNOWN, but autofill predicted it to be of type T.
  // i.e., actual_type == UNKNOWN, predicted_type = T, T != UNKNOWN
  //
  // This is captured as a type-specific log entry for T. Is is also captured
  // as an aggregate (non-type-specific) log entry.
  FALSE_POSITIVE_UNKNOWN = 6,

  // The field type is EMPTY, but autofill predicted it to be of type T.
  // i.e., actual_type == EMPTY, predicted_type = T, T != UNKNOWN
  //
  // This is captured as a type-specific log entry for T. Is is also captured
  // as an aggregate (non-type-specific) log entry.
  FALSE_POSITIVE_EMPTY = 7,

  // The field is of type T, but autofill did not make a type prediction.
  // i.e., actual_type == T, predicted_type = UNKNOWN, T != UNKNOWN.
  //
  // This is captured as a type-specific log entry for T. Is is also captured
  // as an aggregate (non-type-specific) log entry.
  FALSE_NEGATIVE_UNKNOWN = 8,

  // The field is of type T, but autofill predicted it to be of type U.
  // i.e., actual_type == T, predicted_type = U, T != U,
  //       UNKNOWN not in (T,U).
  //
  // This is captured as a type-specific log entry for T. Is is also captured
  // as an aggregate (non-type-specific) log entry.
  FALSE_NEGATIVE_MISMATCH = 9,

  // This must be last.
  NUM_FIELD_TYPE_QUALITY_METRICS
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum QualityMetricPredictionSource {
  // Not used. The prediction source is unknown.
  PREDICTION_SOURCE_UNKNOWN = 0,
  // Local heuristic field-type prediction.
  PREDICTION_SOURCE_HEURISTIC = 1,
  // Crowd-sourced server field type prediction.
  PREDICTION_SOURCE_SERVER = 2,
  // Overall field-type prediction seen by user.
  PREDICTION_SOURCE_OVERALL = 3,
  // ML based field-type predictions. Only reported separately if the ML model
  // is evaluated in shadow mode (i.e. it is not the active heuristic).
  PREDICTION_SOURCE_ML_PREDICTIONS = 4,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum QualityMetricType {
  // Logged based on user's submitted data.
  TYPE_SUBMISSION = 0,
  // Logged based on user's entered data.
  TYPE_NO_SUBMISSION = 1,
  // Logged based on the value of autocomplete attr.
  TYPE_AUTOCOMPLETE_BASED = 2,
  NUM_QUALITY_METRIC_TYPES,
};

// Defines email prediction confusion matrix enums used by UMA records.
// Entries should not be renumbered and numeric values should never be reused.
// Please update "EmailPredictionConfusionMatrix" in
// `tools/metrics/histograms/enums.xml` when new enums are added.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailPredictionConfusionMatrix {
  kTruePositive = 0,
  kFalsePositive = 1,
  kTrueNegative = 2,
  kFalseNegative = 3,
  // Required by UMA histogram macro.
  kMaxValue = kFalseNegative
};

// Metrics measuring how well rationalization has performed given user's
// actual input.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum RationalizationQualityMetric {
  // Rationalization did make it better for the user. Most commonly, user
  // have left it empty as rationalization predicted.
  RATIONALIZATION_GOOD = 0,

  // Rationalization did not make it better or worse. Meaning user have
  // input some value that would not be filled correctly automatically.
  RATIONALIZATION_OK = 1,

  // Rationalization did make it worse, user has to fill
  // in a value that would have been automatically filled
  // if there was no rationalization at all.
  RATIONALIZATION_BAD = 2,

  // This must be last.
  NUM_RATIONALIZATION_QUALITY_METRICS
};

void LogHeuristicPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type);

void LogHeuristicPredictionQualityPerLabelSourceMetric(
    const AutofillField& field);

void LogMlPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type);

void LogServerPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type);

void LogOverallPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type);

void LogEmailFieldPredictionMetrics(const AutofillField& field);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PREDICTION_QUALITY_METRICS_H_
