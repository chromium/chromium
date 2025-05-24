// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void LogAmountExtractionComponentInstallationResult(
    AmountExtractionComponentInstallationResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.AmountExtraction.HeuristicRegexesComponentInstallationResult",
      result);
}

void LogAmountExtractionLatency(base::TimeDelta latency, bool is_successful) {
  base::UmaHistogramTimes(base::StrCat({"Autofill.AmountExtraction.Latency.",
                                        is_successful ? "Success" : "Failure"}),
                          latency);
  base::UmaHistogramTimes("Autofill.AmountExtraction.Latency", latency);
}

void LogAmountExtractionResult(AmountExtractionResult result) {
  base::UmaHistogramEnumeration("Autofill.AmountExtraction.Result", result);
}

}  // namespace autofill::autofill_metrics
