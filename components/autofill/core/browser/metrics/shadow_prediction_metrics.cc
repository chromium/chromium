// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

namespace autofill::autofill_metrics {

namespace {

// Get the comparison between the predictions, without the prediction type being
// encoded in the returned value.
ShadowPredictionComparison GetBaseComparison(
    FieldType current,
    FieldType next,
    const FieldTypeSet& submitted_types) {
  if (current == NO_SERVER_DATA || next == NO_SERVER_DATA) {
    return ShadowPredictionComparison::kNoPrediction;
  } else if (current == next) {
    return submitted_types.contains(current)
               ? ShadowPredictionComparison::kSamePredictionValueAgrees
               : ShadowPredictionComparison::kSamePredictionValueDisagrees;
  } else if (submitted_types.contains_all({current, next})) {
    return ShadowPredictionComparison::kDifferentPredictionsValueAgreesWithBoth;
  } else if (submitted_types.contains(current)) {
    return ShadowPredictionComparison::kDifferentPredictionsValueAgreesWithOld;
  } else if (submitted_types.contains(next)) {
    return ShadowPredictionComparison::kDifferentPredictionsValueAgreesWithNew;
  } else {
    return ShadowPredictionComparison::
        kDifferentPredictionsValueAgreesWithNeither;
  }
}

void LogRegexShadowPredictions(const AutofillField& field,
                               HeuristicSource active_source) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  if (active_source == HeuristicSource::kDefaultRegexes) {
    base::UmaHistogramSparse(
        "Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer",
        GetShadowPrediction(field.heuristic_type(), field.server_type(),
                            field.possible_types()));
  }

  // If the experimental source is active, emit shadow predictions against the
  // default patterns. `FormStructure::DetermineNonActiveHeuristicTypes()`
  // ensures that they were computed.
  if (active_source == HeuristicSource::kExperimentalRegexes) {
    base::UmaHistogramSparse(
        "Autofill.ShadowPredictions.ExperimentalToDefault",
        GetShadowPrediction(
            field.heuristic_type(HeuristicSource::kDefaultRegexes),
            field.heuristic_type(HeuristicSource::kExperimentalRegexes),
            field.possible_types()));
  }
#endif
}

void LogMlShadowPredictions(const AutofillField& field) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!base::FeatureList::IsEnabled(features::kAutofillModelPredictions)) {
    return;
  }
  const FieldTypeSet& submitted_types = field.possible_types();
  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.DefaultServerToMLModel",
      GetShadowPrediction(
          field.server_type(),
          field.heuristic_type(HeuristicSource::kMachineLearning),
          submitted_types));
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.DefaultPatternSourceToMLModel",
      GetShadowPrediction(
          field.heuristic_type(HeuristicSource::kDefaultRegexes),
          field.heuristic_type(HeuristicSource::kMachineLearning),
          submitted_types));
#endif  // BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

}  // namespace

int GetShadowPrediction(FieldType current,
                        FieldType next,
                        const FieldTypeSet& submitted_types) {
  ShadowPredictionComparison comparison =
      GetBaseComparison(current, next, submitted_types);
  // Encode the `current` type and `comparison` into an int.
  int encoding = static_cast<int>(comparison);
  if (comparison != ShadowPredictionComparison::kNoPrediction) {
    // Multiplying by `kMaxValue` (instead of `kMaxValue + 1`) is fine, because
    // the `kNoPrediction` case is ignored.
    // When `comparison == kMaxValue`, the base-`kMaxValue` representation of
    // the `encoding` "overflows" into `static_cast<int>(current) + 1`, but
    // since `kNoPrediction` is skipped, this doesn't cause an overlap.
    encoding += static_cast<int>(current) *
                static_cast<int>(ShadowPredictionComparison::kMaxValue);
  }
  return encoding;
}

void LogShadowPredictionComparison(const AutofillField& field,
                                   HeuristicSource active_source) {
  LogRegexShadowPredictions(field, active_source);
  LogMlShadowPredictions(field);
}

}  // namespace autofill::autofill_metrics
