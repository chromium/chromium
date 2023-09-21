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
    ServerFieldType current,
    ServerFieldType next,
    const ServerFieldTypeSet& submitted_types) {
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

}  // namespace

int GetShadowPrediction(ServerFieldType current,
                        ServerFieldType next,
                        const ServerFieldTypeSet& submitted_types) {
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

void LogShadowPredictionComparison(const AutofillField& field) {
  const auto& submitted_types = field.possible_types();

  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer",
      GetShadowPrediction(field.heuristic_type(), field.server_type(),
                          submitted_types));

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.ExperimentalToDefault",
      GetShadowPrediction(field.heuristic_type(HeuristicSource::kDefault),
                          field.heuristic_type(HeuristicSource::kExperimental),
                          submitted_types));

  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.NextGenToDefault",
      GetShadowPrediction(field.heuristic_type(HeuristicSource::kDefault),
                          field.heuristic_type(HeuristicSource::kNextGen),
                          submitted_types));

  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.NextGenToExperimental",
      GetShadowPrediction(field.heuristic_type(HeuristicSource::kExperimental),
                          field.heuristic_type(HeuristicSource::kNextGen),
                          submitted_types));
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions)) {
    base::UmaHistogramSparse(
        "Autofill.ShadowPredictions.DefaultServerToMLModel",
        GetShadowPrediction(
            field.server_type(),
            field.heuristic_type(HeuristicSource::kMachineLearning),
            submitted_types));
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    base::UmaHistogramSparse(
        "Autofill.ShadowPredictions.LegacyPatternSourceToMLModel",
        GetShadowPrediction(
            field.heuristic_type(HeuristicSource::kLegacy),
            field.heuristic_type(HeuristicSource::kMachineLearning),
            submitted_types));
#else
    base::UmaHistogramSparse(
        "Autofill.ShadowPredictions.DefaultPatternSourceToMLModel",
        GetShadowPrediction(
            field.heuristic_type(HeuristicSource::kDefault),
            field.heuristic_type(HeuristicSource::kMachineLearning),
            submitted_types));
#endif
  }
#endif
}

}  // namespace autofill::autofill_metrics
