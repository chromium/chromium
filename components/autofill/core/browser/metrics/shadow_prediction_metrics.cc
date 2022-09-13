// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"

namespace autofill::metrics {

namespace {

// The number of basic comparison results (i.e. without offsetting to encode the
// field type).
constexpr int kBaseComparisonRange = 6;

// Encode `prediction` into `comparison_base`.
int GetTypeSpecificComparison(ServerFieldType prediction, int comparison_base) {
  DCHECK_LE(comparison_base, kDifferentPredictionsValueAgreesWithBoth);
  DCHECK_NE(comparison_base, kNoPrediction);

  return static_cast<int>(prediction) * kBaseComparisonRange + comparison_base;
}

// Get the comparison between the predictions, without the prediction type being
// encoded in the returned value. The returned value is in the range [0,6]
// inclusive.
int GetBaseComparison(ServerFieldType current,
                      ServerFieldType next,
                      const ServerFieldTypeSet& submitted_types) {
  if (current == NO_SERVER_DATA || next == NO_SERVER_DATA) {
    return kNoPrediction;
  } else if (current == next) {
    return submitted_types.contains(current) ? kSamePredictionValueAgrees
                                             : kSamePredictionValueDisagrees;
  } else if (submitted_types.contains_all({current, next})) {
    return kDifferentPredictionsValueAgreesWithBoth;
  } else if (submitted_types.contains(current)) {
    return kDifferentPredictionsValueAgreesWithOld;
  } else if (submitted_types.contains(next)) {
    return kDifferentPredictionsValueAgreesWithNew;
  } else {
    return kDifferentPredictionsValueAgreesWithNeither;
  }
}

}  // namespace

int GetShadowPrediction(ServerFieldType current,
                        ServerFieldType next,
                        const ServerFieldTypeSet& submitted_types) {
  // `NO_SERVER_DATA` means that we didn't actually run any heuristics.
  if (current == NO_SERVER_DATA || next == NO_SERVER_DATA)
    return kNoPrediction;

  int comparison = GetBaseComparison(current, next, submitted_types);

  // If we compared the predictions, offset them by the field type, so that the
  // type is included in the enum.
  if (comparison != kNoPrediction)
    comparison = GetTypeSpecificComparison(current, comparison);

  return comparison;
}

void LogShadowPredictionComparison(const AutofillField& field) {
  const auto& submitted_types = field.possible_types();

  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer",
      GetShadowPrediction(field.heuristic_type(), field.server_type(),
                          submitted_types));

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_HEADERS)
  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.ExperimentalToDefault",
      GetShadowPrediction(field.heuristic_type(PatternSource::kDefault),
                          field.heuristic_type(PatternSource::kExperimental),
                          submitted_types));

  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.NextGenToDefault",
      GetShadowPrediction(field.heuristic_type(PatternSource::kDefault),
                          field.heuristic_type(PatternSource::kNextGen),
                          submitted_types));

  base::UmaHistogramSparse(
      "Autofill.ShadowPredictions.NextGenToExperimental",
      GetShadowPrediction(field.heuristic_type(PatternSource::kExperimental),
                          field.heuristic_type(PatternSource::kNextGen),
                          submitted_types));
#endif
}

}  // namespace autofill::metrics
