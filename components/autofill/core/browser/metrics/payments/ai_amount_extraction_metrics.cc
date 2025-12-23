// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/ai_amount_extraction_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill::autofill_metrics {

void LogAiAmountExtractionResult(AiAmountExtractionResult result,
                                 std::optional<base::TimeDelta> latency,
                                 ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration("Autofill.AiAmountExtraction.Result", result);

  ukm::builders::Autofill_AiAmountExtraction_Result ukm_builder(ukm_source_id);
  CHECK(latency.has_value() || result == AiAmountExtractionResult::kTimeout);
  ukm_builder.SetResult(static_cast<int64_t>(result));
  if (latency) {
    switch (result) {
      case AiAmountExtractionResult::kSuccess:
        ukm_builder.SetSuccessLatencyInMillis(latency->InMilliseconds());
        break;
      case AiAmountExtractionResult::kFailed:
        ukm_builder.SetFailureLatencyInMillis(latency->InMilliseconds());
        break;
      case AiAmountExtractionResult::kInvalidResponse:
        ukm_builder.SetInvalidResponseLatencyInMillis(
            latency->InMilliseconds());
        break;
      case AiAmountExtractionResult::kTimeout:
        // No latency metric is logged for a timeout.
        NOTREACHED();
    }
  }
  ukm_builder.Record(ukm::UkmRecorder::Get());
}

void LogAiAmountExtractedInIssuerRange(bool is_within_range,
                                       BnplIssuer::IssuerId issuer_id,
                                       ukm::SourceId ukm_source_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.AiAmountExtraction.AmountInIssuerRange.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramBoolean(histogram_name, is_within_range);

  ukm::builders::Autofill_Bnpl_AiAmountExtraction_AmountInIssuerRange(
      ukm_source_id)
      .SetIssuer(static_cast<int64_t>(issuer_id))
      .SetIsWithinRange(is_within_range)
      .Record(ukm::UkmRecorder::Get());
}

void LogAiAmountExtractionApcFetchResult(bool success,
                                         ukm::SourceId ukm_source_id) {
  base::UmaHistogramBoolean("Autofill.AiAmountExtraction.ApcFetchResult",
                            success);

  ukm::builders::Autofill_AiAmountExtraction_ApcFetchResult(ukm_source_id)
      .SetApcFetchResult(success)
      .Record(ukm::UkmRecorder::Get());
}

void LogAiAmountExtractionInvalidResponseReason(
    payments::AiAmountExtractionResult::Error error) {
  AiAmountExtractionInvalidResponseReason reason;

  switch (error) {
    case payments::AiAmountExtractionResult::Error::kNegativeAmount:
      reason = AiAmountExtractionInvalidResponseReason::kNegativeAmount;
      break;
    case payments::AiAmountExtractionResult::Error::kAmountMissing:
      reason = AiAmountExtractionInvalidResponseReason::kAmountMissing;
      break;
    case payments::AiAmountExtractionResult::Error::kUnsupportedCurrency:
      reason = AiAmountExtractionInvalidResponseReason::kUnsupportedCurrency;
      break;
    case payments::AiAmountExtractionResult::Error::kMissingCurrency:
      reason = AiAmountExtractionInvalidResponseReason::kCurrencyCodeMissing;
      break;
    case payments::AiAmountExtractionResult::Error::kFailureToGenerateApc:
    case payments::AiAmountExtractionResult::Error::kMissingServerResponse:
    case payments::AiAmountExtractionResult::Error::kTimeout:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration(
      "Autofill.AiAmountExtraction.InvalidResponseReason", reason);
}

}  // namespace autofill::autofill_metrics
