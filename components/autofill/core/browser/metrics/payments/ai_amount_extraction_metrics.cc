// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/ai_amount_extraction_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill::autofill_metrics {

void LogAiAmountExtractionResult(AiAmountExtractionResult result,
                                 ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration("Autofill.AiAmountExtraction.Result", result);

  ukm::builders::Autofill_AiAmountExtractionComplete(ukm_source_id)
      .SetResult(static_cast<int64_t>(result))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace autofill::autofill_metrics
