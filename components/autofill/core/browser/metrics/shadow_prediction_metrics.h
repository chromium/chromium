// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SHADOW_PREDICTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SHADOW_PREDICTION_METRICS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::autofill_metrics {

// Describes how actual predictions, shadow predictions and submitted types (the
// type derived from the value entered into the field) (dis)agree with each
// other. See `GetShadowPrediction()` below.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These mirror the first entries of
// `AutofillPredictionsComparisonResult` in
// tools/metrics/histograms/metadata/autofill/histograms.xml
enum class ShadowPredictionComparison {
  kNoPrediction = 0,
  kSamePredictionValueAgrees = 1,
  kSamePredictionValueDisagrees = 2,
  kDifferentPredictionsValueAgreesWithOld = 3,
  kDifferentPredictionsValueAgreesWithNew = 4,
  kDifferentPredictionsValueAgreesWithNeither = 5,
  kDifferentPredictionsValueAgreesWithBoth = 6,
  kMaxValue = kDifferentPredictionsValueAgreesWithBoth
};

// Gets a 3-way comparison between
//  * the `current` prediction
//  * the `next` (shadow) prediction
//  * the types detected in the field `submitted_types` during submission
// The result is a pair of (`current`, `ShadowPredictionComparison`), encoded as
// an int. This is used to emit a type-specific UMA metric, encoding how well
// the shadow predictions perform compared to the `current` predictions.
int GetShadowPrediction(FieldType current,
                        FieldType next,
                        const FieldTypeSet& submitted_types);

// Logs Autofill.ShadowPredictions.* metrics by comparing the submitted
// values to the actual and hypothetical predictions.
// Unfortunately, this code cannot rely on `GetActiveHeuristicSource()` because
// tests need to simulate that experimental patterns are active.
void LogShadowPredictionComparison(const AutofillField& field,
                                   HeuristicSource active_source);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SHADOW_PREDICTION_METRICS_H_
