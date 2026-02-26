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

void LogAiAmountExtractionResult(
    payments::AiAmountExtractionResult::ResultType raw_result,
    std::optional<base::TimeDelta> latency,
    ukm::SourceId ukm_source_id) {
  ukm::builders::Autofill_AiAmountExtraction_Result ukm_builder(ukm_source_id);

  AiAmountExtractionResult result;
  std::optional<AiAmountExtractionInvalidResponseReason>
      invalid_response_reason;
  if (raw_result.has_value()) {
    result = AiAmountExtractionResult::kSuccess;
  } else {
    switch (raw_result.error()) {
      case payments::AiAmountExtractionResult::Error::kMissingServerResponse:
        result = AiAmountExtractionResult::kFailed;
        break;
      case payments::AiAmountExtractionResult::Error::kTimeout:
        result = AiAmountExtractionResult::kTimeout;
        break;
      case payments::AiAmountExtractionResult::Error::kNegativeAmount:
        result = AiAmountExtractionResult::kInvalidResponse;
        invalid_response_reason =
            AiAmountExtractionInvalidResponseReason::kNegativeAmount;
        break;
      case payments::AiAmountExtractionResult::Error::kAmountMissing:
        result = AiAmountExtractionResult::kInvalidResponse;
        invalid_response_reason =
            AiAmountExtractionInvalidResponseReason::kAmountMissing;
        break;
      case payments::AiAmountExtractionResult::Error::kMissingCurrency:
        result = AiAmountExtractionResult::kInvalidResponse;
        invalid_response_reason =
            AiAmountExtractionInvalidResponseReason::kCurrencyCodeMissing;
        break;
      case payments::AiAmountExtractionResult::Error::kUnsupportedCurrency:
        result = AiAmountExtractionResult::kInvalidResponse;
        invalid_response_reason =
            AiAmountExtractionInvalidResponseReason::kUnsupportedCurrency;
        break;
      case payments::AiAmountExtractionResult::Error::kFailureToGenerateApc:
        NOTREACHED();
    }
  }

  base::UmaHistogramEnumeration("Autofill.AiAmountExtraction.Result", result);
  ukm_builder.SetResult(static_cast<int64_t>(result));

  if (invalid_response_reason.has_value()) {
    base::UmaHistogramEnumeration(
        "Autofill.AiAmountExtraction.InvalidResponseReason",
        invalid_response_reason.value());
    ukm_builder.SetInvalidResponseReason(
        static_cast<int64_t>(invalid_response_reason.value()));
  }

  CHECK(latency.has_value() || result == AiAmountExtractionResult::kTimeout);
  if (latency) {
    switch (result) {
      case AiAmountExtractionResult::kSuccess:
        base::UmaHistogramTimes("Autofill.AiAmountExtraction.Latency.Success",
                                latency.value());
        ukm_builder.SetSuccessLatencyInMillis(latency->InMilliseconds());
        break;
      case AiAmountExtractionResult::kFailed:
        base::UmaHistogramTimes("Autofill.AiAmountExtraction.Latency.Failure",
                                latency.value());
        ukm_builder.SetFailureLatencyInMillis(latency->InMilliseconds());
        break;
      case AiAmountExtractionResult::kInvalidResponse:
        base::UmaHistogramTimes(
            "Autofill.AiAmountExtraction.Latency.InvalidResponse",
            latency.value());
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

}  // namespace autofill::autofill_metrics
