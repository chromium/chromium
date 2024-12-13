// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/facilitated_payments/core/validation/payment_link_validator.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {
namespace {

// Helper to convert `PurchaseActionResult` to a string for logging.
std::string GetInitiatePurchaseActionResultString(PurchaseActionResult result) {
  switch (result) {
    case PurchaseActionResult::kResultOk:
      return "Succeeded";
    case PurchaseActionResult::kCouldNotInvoke:
      return "Failed";
    case PurchaseActionResult::kResultCanceled:
      return "Abandoned";
  }
}

std::string PaymentTypeToString(FacilitatedPaymentsType payment_type) {
  switch (payment_type) {
    case FacilitatedPaymentsType::kPix:
      return "Pix";
    case FacilitatedPaymentsType::kEwallet:
      return "Ewallet";
  }
}

std::string SchemeToString(PaymentLinkValidator::Scheme scheme) {
  switch (scheme) {
    case PaymentLinkValidator::Scheme::kDuitNow:
      return "DuitNow";
    case PaymentLinkValidator::Scheme::kShopeePay:
      return "ShopeePay";
    case PaymentLinkValidator::Scheme::kTngd:
      return "Tngd";
    case PaymentLinkValidator::Scheme::kInvalid:
      // This case can't happen because `kInvalid` causes an early return in
      // eWallet manager.
      NOTREACHED();
  }
}

std::string ResultToString(bool result) {
  return result ? "Success" : "Failure";
}

}  // namespace

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

void LogApiAvailabilityCheckResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool result,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".IsApiAvailable.", ResultToString(result), ".Latency"}),
      duration);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.Ewallet.IsApiAvailable.",
                      ResultToString(result), ".Latency.",
                      SchemeToString(*scheme)}),
        duration);
  }
}

void LogLoadRiskDataResultAndLatency(bool was_successful,
                                     base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.LoadRiskData.",
                    was_successful ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogGetClientTokenResultAndLatency(bool result, base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.GetClientToken.",
                    result ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogPayflowExitedReason(PayflowExitedReason reason) {
  base::UmaHistogramEnumeration("FacilitatedPayments.Pix.PayflowExitedReason",
                                reason);
}

void LogInitiatePaymentAttempt() {
  base::UmaHistogramBoolean("FacilitatedPayments.Pix.InitiatePayment.Attempt",
                            /*sample=*/true);
}

void LogInitiatePaymentResultAndLatency(bool result, base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.InitiatePayment.",
                    result ? "Success" : "Failure", ".Latency"}),
      duration);
}

void LogInitiatePurchaseActionAttempt() {
  base::UmaHistogramBoolean(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Attempt",
      /*sample=*/true);
}

void LogInitiatePurchaseActionResultAndLatency(PurchaseActionResult result,
                                               base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.",
                    GetInitiatePurchaseActionResultString(result), ".Latency"}),
      duration);
}

void LogInitiatePurchaseActionResultUkm(PurchaseActionResult result,
                                        ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult(
      ukm_source_id)
      .SetResult(static_cast<uint8_t>(result))
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
