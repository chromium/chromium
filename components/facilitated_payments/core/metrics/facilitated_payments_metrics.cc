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
std::string GetPurchaseActionResultString(PurchaseActionResult result) {
  switch (result) {
    case PurchaseActionResult::kResultOk:
      return "Succeeded";
    case PurchaseActionResult::kCouldNotInvoke:
      return "Failed";
    case PurchaseActionResult::kResultCanceled:
      return "Abandoned";
  }
}

std::string AvailableEwalletsConfigurationToString(
    AvailableEwalletsConfiguration ewallet_type) {
  switch (ewallet_type) {
    case AvailableEwalletsConfiguration::kSingleBoundEwallet:
      return "SingleBoundEwallet";
    case AvailableEwalletsConfiguration::kSingleUnboundEwallet:
      return "SingleUnboundEwallet";
    case AvailableEwalletsConfiguration::kMultipleEwallets:
      return "MultipleEwallets";
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

std::string PaymentTypeToFopSelectorLatencyString(
    FacilitatedPaymentsType payment_type) {
  switch (payment_type) {
    case FacilitatedPaymentsType::kPix:
      return "LatencyAfterCopy";
    case FacilitatedPaymentsType::kEwallet:
      return "LatencyAfterDetectingPaymentLink";
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

void LogPaymentLinkDetected(ukm::SourceId ukm_source_id) {
  base::UmaHistogramBoolean("FacilitatedPayments.Ewallet.PaymentLinkDetected",
                            /*sample=*/true);
  ukm::builders::FacilitatedPayments_PaymentLinkDetected(ukm_source_id)
      .SetPaymentLinkDetected(true)
      .Record(ukm::UkmRecorder::Get());
}

void LogEwalletFopSelectorShownUkm(ukm::SourceId ukm_source_id,
                                   PaymentLinkValidator::Scheme scheme) {
  ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown(ukm_source_id)
      .SetShown(true)
      .SetScheme(static_cast<uint8_t>(scheme))
      .Record(ukm::UkmRecorder::Get());
}

void LogPixFopSelectorShownUkm(ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_FopSelectorShown(ukm_source_id)
      .SetShown(true)
      .Record(ukm::UkmRecorder::Get());
}

void LogPixFopSelectorResultUkm(bool accepted, ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_FopSelectorResult(ukm_source_id)
      .SetResult(accepted)
      .Record(ukm::UkmRecorder::Get());
}

void LogEwalletFopSelectorResultUkm(bool accepted,
                                    ukm::SourceId ukm_source_id,
                                    PaymentLinkValidator::Scheme scheme) {
  ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult(ukm_source_id)
      .SetScheme(static_cast<uint8_t>(scheme))
      .SetResult(accepted)
      .Record(ukm::UkmRecorder::Get());
}

void LogPixFopSelectedAndLatency(base::TimeDelta duration) {
  // The histogram name should be in sync with
  // `FacilitatedPaymentsPaymentMethodsMediator.PIX_FOP_SELECTOR_USER_ACTION_HISTOGRAM`.
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      FopSelectorAction::kFopSelected);

  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.FopSelected.Latency",
                              duration);
}

void LogEwalletFopSelected(AvailableEwalletsConfiguration type) {
  // The histogram name should be in sync with
  // `FacilitatedPaymentsPaymentMethodsMediator.EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM`.
  base::UmaHistogramEnumeration(
      base::StrCat({"FacilitatedPayments.Ewallet.FopSelector.UserAction.",
                    AvailableEwalletsConfigurationToString(type)}),
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

void LogLoadRiskDataResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool was_successful,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".LoadRiskData.", ResultToString(was_successful),
                    ".Latency"}),
      duration);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.Ewallet.LoadRiskData.",
                      ResultToString(was_successful), ".Latency.",
                      SchemeToString(*scheme)}),
        duration);
  }
}

void LogGetClientTokenResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool result,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".GetClientToken.", ResultToString(result), ".Latency"}),
      duration);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.Ewallet.GetClientToken.",
                      ResultToString(result), ".Latency.",
                      SchemeToString(*scheme)}),
        duration);
  }
}

void LogEwalletFlowExitedReason(
    EwalletFlowExitedReason reason,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Ewallet.PayflowExitedReason", reason);
  // `scheme` might be empty during some invalid cases (payment url not in
  // allowlist).
  if (scheme.has_value() && *scheme != PaymentLinkValidator::Scheme::kInvalid) {
    base::UmaHistogramEnumeration(
        base::StrCat({"FacilitatedPayments.Ewallet.PayflowExitedReason.",
                      SchemeToString(*scheme)}),
        reason);
  }
}

void LogPixFlowExitedReason(PixFlowExitedReason reason) {
  base::UmaHistogramEnumeration("FacilitatedPayments.Pix.PayflowExitedReason",
                                reason);
}

void LogInitiatePaymentAttempt(
    FacilitatedPaymentsType payment_type,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramBoolean(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".InitiatePayment.Attempt"}),
      /*sample=*/true);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramBoolean(
        base::StrCat({"FacilitatedPayments.Ewallet.InitiatePayment.Attempt.",
                      SchemeToString(*scheme)}),
        /*sample=*/true);
  }
}

void LogInitiatePaymentResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool result,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".InitiatePayment.", ResultToString(result), ".Latency"}),
      duration);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                      ".InitiatePayment.", ResultToString(result), ".Latency.",
                      SchemeToString(*scheme)}),
        duration);
  }
}

void LogInitiatePurchaseActionAttempt(
    FacilitatedPaymentsType payment_type,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramBoolean(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".InitiatePurchaseAction.Attempt"}),
      /*sample=*/true);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramBoolean(
        base::StrCat(
            {"FacilitatedPayments.Ewallet.InitiatePurchaseAction.Attempt.",
             SchemeToString(*scheme)}),
        /*sample=*/true);
  }
}

void LogPixInitiatePurchaseActionResultAndLatency(PurchaseActionResult result,
                                                  base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.",
                    GetPurchaseActionResultString(result), ".Latency"}),
      duration);
}

void LogPixTransactionResultAndLatency(PurchaseActionResult result,
                                       base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.Transaction.",
                    GetPurchaseActionResultString(result), ".Latency"}),
      duration);
}

void LogEwalletInitiatePurchaseActionResultAndLatency(
    PurchaseActionResult result,
    base::TimeDelta duration,
    PaymentLinkValidator::Scheme scheme,
    bool is_device_bound) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                    GetPurchaseActionResultString(result), ".Latency",
                    is_device_bound ? ".DeviceBound" : ".DeviceNotBound"}),
      duration);
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                    GetPurchaseActionResultString(result), ".Latency.",
                    SchemeToString(scheme),
                    is_device_bound ? ".DeviceBound" : ".DeviceNotBound"}),
      duration);
}

void LogInitiatePurchaseActionResultUkm(PurchaseActionResult result,
                                        ukm::SourceId ukm_source_id) {
  ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult(
      ukm_source_id)
      .SetResult(static_cast<uint8_t>(result))
      .Record(ukm::UkmRecorder::Get());
}

void LogUiScreenShown(FacilitatedPaymentsType payment_type,
                      UiState ui_screen,
                      std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramEnumeration(base::StrCat({
                                    "FacilitatedPayments.",
                                    PaymentTypeToString(payment_type),
                                    ".UiScreenShown",
                                }),
                                ui_screen);
  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramEnumeration(
        base::StrCat({"FacilitatedPayments.Ewallet.UiScreenShown.",
                      SchemeToString(*scheme)}),
        ui_screen);
  }
}

void LogFopSelectorShownLatency(
    FacilitatedPaymentsType payment_type,
    base::TimeDelta latency,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.", PaymentTypeToString(payment_type),
                    ".FopSelectorShown.",
                    PaymentTypeToFopSelectorLatencyString(payment_type)}),
      latency);

  if (payment_type == FacilitatedPaymentsType::kEwallet) {
    CHECK(scheme.has_value());
    CHECK_NE(PaymentLinkValidator::Scheme::kInvalid, *scheme);
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.Ewallet.FopSelectorShown."
                      "LatencyAfterDetectingPaymentLink.",
                      SchemeToString(*scheme)}),
        latency);
  }
}

}  // namespace payments::facilitated
