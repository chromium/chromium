// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogAmountExtractionComponentInstallationResult(
    AmountExtractionComponentInstallationResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.AmountExtraction.HeuristicRegexesComponentInstallationResult",
      result);
}

}  // namespace autofill::autofill_metrics
