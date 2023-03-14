// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SHADOW_PREDICTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SHADOW_PREDICTION_METRICS_H_

#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These mirror the first entries of
// `AutofillPredictionsComparisonResult` in
// tools/metrics/histograms/metadata/autofill/histograms.xml
constexpr int kNoPrediction = 0;
constexpr int kSamePredictionValueAgrees = 1;
constexpr int kSamePredictionValueDisagrees = 2;
constexpr int kDifferentPredictionsValueAgreesWithOld = 3;
constexpr int kDifferentPredictionsValueAgreesWithNew = 4;
constexpr int kDifferentPredictionsValueAgreesWithNeither = 5;
constexpr int kDifferentPredictionsValueAgreesWithBoth = 6;

// Gets a 3-way comparison between
//  * the `current` prediction
//  * the `next` (shadow) prediction
//  * the types detected in the field `submitted_types` during submission
int GetShadowPrediction(ServerFieldType current,
                        ServerFieldType next,
                        const ServerFieldTypeSet& submitted_types);

// Logs Autofill.ShadowPredictions.* metrics by comparing the submitted
// values to the actual and hypothetical predictions.
void LogShadowPredictionComparison(const AutofillField& field);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SHADOW_PREDICTION_METRICS_H_
