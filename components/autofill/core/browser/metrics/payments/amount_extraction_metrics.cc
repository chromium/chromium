// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill::autofill_metrics {

void LogAmountExtractionComponentInstallationResult(
    AmountExtractionComponentInstallationResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.AmountExtraction.HeuristicRegexesComponentInstallationResult",
      result);
}

void LogAmountExtractionResult(std::optional<base::TimeDelta> latency,
                               AmountExtractionResult result,
                               ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration("Autofill.AmountExtraction.Result2", result);
  const bool is_successful = result == AmountExtractionResult::kSuccessful;
  ukm::builders::Autofill_AmountExtractionComplete ukm_builder(ukm_source_id);
  CHECK(latency.has_value() || result == AmountExtractionResult::kTimeout);
  if (latency.has_value()) {
    base::UmaHistogramTimes(
        base::StrCat({"Autofill.AmountExtraction.Latency2.",
                      is_successful ? "Success" : "Failure"}),
        latency.value());
    base::UmaHistogramTimes("Autofill.AmountExtraction.Latency2",
                            latency.value());
    if (is_successful) {
      ukm_builder.SetSuccessLatencyInMillis(latency.value().InMilliseconds());
    } else {
      ukm_builder.SetFailureLatencyInMillis(latency.value().InMilliseconds());
    }
  }
  ukm_builder.SetResult(static_cast<uint8_t>(result))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace autofill::autofill_metrics
