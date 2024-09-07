// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {

void LogPaymentCodeValidationResultAndLatency(
    base::expected<bool, std::string> result,
    base::TimeDelta duration) {
  std::string payment_code_validation_result_type;
  if (!result.has_value()) {
    payment_code_validation_result_type = "ValidatorFailed";
  } else if (!result.value()) {
    payment_code_validation_result_type = "InvalidCode";
  } else {
    payment_code_validation_result_type = "ValidCode";
  }
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.PaymentCodeValidation.",
                    payment_code_validation_result_type, ".Latency"}),
      duration);
}

void LogIsApiAvailableResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.IsApiAvailable.Result",
                        result);
  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.IsApiAvailable.Latency",
                              duration);
}

void LogLoadRiskDataResultAndLatency(bool was_successful,
                                     base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.LoadRiskData.",
                    was_successful ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogGetClientTokenResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.GetClientToken.Result",
                        result);
  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.GetClientToken.Latency",
                              duration);
}

void LogPaymentNotOfferedReason(PaymentNotOfferedReason reason) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason", reason);
}

void LogInitiatePaymentResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.InitiatePayment.Result",
                        result);
  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.InitiatePayment.Latency",
                              duration);
}

void LogInitiatePurchaseActionResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.InitiatePurchaseAction.Result",
                        result);
  base::UmaHistogramLongTimes(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Latency", duration);
}

void LogFopSelectorShown(bool shown) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.FopSelector.Shown", shown);
}

void LogTransactionResult(TransactionResult result,
                          TriggerSource trigger_source,
                          base::TimeDelta duration,
                          ukm::SourceId ukm_source_id) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramEnumeration("FacilitatedPayments.Pix.Transaction.Result",
                                result);
  std::string latency_histogram_transaction_result_type;
  switch (result) {
    case TransactionResult::kSuccess:
      latency_histogram_transaction_result_type = "Success";
      break;
    case TransactionResult::kAbandoned:
      latency_histogram_transaction_result_type = "Abandoned";
      break;
    case TransactionResult::kFailed:
      latency_histogram_transaction_result_type = "Failed";
      break;
  }
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.Transaction.",
                    latency_histogram_transaction_result_type, ".Latency"}),
      duration);
  ukm::builders::FacilitatedPayments_Pix_Transaction(ukm_source_id)
      .SetResult(static_cast<uint8_t>(result))
      .SetTriggerSource(static_cast<uint8_t>(trigger_source))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace payments::facilitated
