// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {

void LogPixCodeCopied(ukm::SourceId ukm_source_id) {
  base::UmaHistogramBoolean("FacilitatedPayments.Pix.PixCodeCopied",
                            /*sample=*/true);
  ukm::builders::FacilitatedPayments_PixCodeCopied(ukm_source_id)
      .SetPixCodeCopied(true)
      .Record(ukm::UkmRecorder::Get());
}

void LogFopSelectorShownUkm(ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_FopSelectorShown(ukm_source_id)
      .SetShown(true)
      .Record(ukm::UkmRecorder::Get());
}

void LogFopSelectorResultUkm(bool accepted, ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_FopSelectorResult(ukm_source_id)
      .SetResult(accepted)
      .Record(ukm::UkmRecorder::Get());
}

void LogFopSelected() {
  // The histogram name should be in sync with
  // `FacilitatedPaymentsPaymentMethodsMediator.FOP_SELECTOR_USER_ACTION_HISTOGRAM`.
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      FopSelectorAction::kFopSelected);
}

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

void LogApiAvailabilityCheckResultAndLatency(bool result,
                                             base::TimeDelta duration) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.IsApiAvailable.",
                    result ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogLoadRiskDataResultAndLatency(bool was_successful,
                                     base::TimeDelta duration) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.LoadRiskData.",
                    was_successful ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogGetClientTokenResultAndLatency(bool result, base::TimeDelta duration) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.GetClientToken.",
                    result ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogPayflowExitedReason(PayflowExitedReason reason) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramEnumeration("FacilitatedPayments.Pix.PayflowExitedReason",
                                reason);
}

// TODO(crbug.com/367751320): Remove after new PayflowExitedReason histogram is
// finished.
void LogPaymentNotOfferedReason(PaymentNotOfferedReason reason) {
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason", reason);
}

void LogInitiatePaymentAttempt() {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramBoolean("FacilitatedPayments.Pix.InitiatePayment.Attempt",
                            /*sample=*/true);
}

void LogInitiatePaymentResultAndLatency(bool result, base::TimeDelta duration) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.InitiatePayment.",
                    result ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogInitiatePurchaseActionAttempt() {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramBoolean(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Attempt",
      /*sample=*/true);
}

void LogInitiatePurchaseActionResultAndLatency(const std::string& result,
                                               base::TimeDelta duration) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.", result,
                    ".Latency"}),
      duration);
}

void LogInitiatePurchaseActionResultUkm(const std::string& result,
                                        ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult(
      ukm_source_id)
      .SetResult(ConvertPurchaseActionResultToEnumValue(result))
      .Record(ukm::UkmRecorder::Get());
}

uint8_t ConvertPurchaseActionResultToEnumValue(const std::string& result) {
  if (result == "Failed") {
    return 0;  // See the definition of the enum
               // FacilitatedPayments.InitiatePurchaseActionResult.
  } else if (result == "Succeeded") {
    return 1;
  } else if (result == "Abandoned") {
    return 2;
  } else {
    NOTREACHED();
  }
}

void LogFopSelectorShown(bool shown) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.FopSelector.Shown", shown);
}

void LogTransactionResult(TransactionResult result,
                          TriggerSource trigger_source,
                          base::TimeDelta duration,
                          ukm::SourceId ukm_source_id) {
  // TODO(crbug.com/337929926): Remove hardcoding for Pix and use
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

void LogUiScreenShown(UiState ui_screen) {
  base::UmaHistogramEnumeration("FacilitatedPayments.Pix.UiScreenShown",
                                ui_screen);
}

void LogPixFopSelectorShownLatency(base::TimeDelta latency) {
  base::UmaHistogramLongTimes(
      "FacilitatedPayments.Pix.FopSelectorShown.LatencyAfterCopy", latency);
}

}  // namespace payments::facilitated
