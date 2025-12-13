// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AI_AMOUNT_EXTRACTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AI_AMOUNT_EXTRACTION_METRICS_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. `AiAmountExtractionResult` in
// tools/metrics/histograms/enums.xml should also be updated when changed
// here.
enum class AiAmountExtractionResult {
  // The amount extraction succeeded.
  kSuccess = 0,
  // The amount extraction failed.
  kFailed = 1,
  // The amount extraction returned an invalid response.
  kInvalidResponse = 2,
  // The amount extraction timed out.
  kTimeout = 3,
  kMaxValue = kTimeout,
};

// Logs the result of the AI-based amount extraction process. Logs to both UMA
// and UKM
void LogAiAmountExtractionResult(AiAmountExtractionResult result,
                                 ukm::SourceId ukm_source_id);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AI_AMOUNT_EXTRACTION_METRICS_H_
