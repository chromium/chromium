// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AI_AMOUNT_EXTRACTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AI_AMOUNT_EXTRACTION_METRICS_H_

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// `AiAmountExtractionInvalidResponseReason` in
// tools/metrics/histograms/enums.xml should also be updated when changed
// here.
enum class AiAmountExtractionInvalidResponseReason {
  // The amount extracted was negative.
  kNegativeAmount = 0,
  // The amount field was missing from the response.
  kAmountMissing = 1,
  // The currency is not supported.
  kUnsupportedCurrency = 2,
  // The currency code field was missing from the response.
  kCurrencyCodeMissing = 3,
  kMaxValue = kCurrencyCodeMissing,
};

// Logs the result of the AI-based amount extraction process. Logs to both UMA
// and UKM
void LogAiAmountExtractionResult(AiAmountExtractionResult result,
                                 ukm::SourceId ukm_source_id);

// Logs if the amount extracted is within or outside the issuer's supported
// range.
void LogAiAmountExtractedInIssuerRange(bool is_within_range,
                                       BnplIssuer::IssuerId issuer_id);

// Logs the result (success/failure) of fetching the Annotated Page Content
// (APC).
void LogAiAmountExtractionApcFetchResult(bool success);

// Logs the reason why the AI-based amount extraction response was invalid,
// specifically when a response is present and it is an error.
void LogAiAmountExtractionInvalidResponseReason(
    payments::AiAmountExtractionResult::Error error);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AI_AMOUNT_EXTRACTION_METRICS_H_
