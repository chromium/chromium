// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/facilitated_payments/core/mojom/pix_code_validator.mojom.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/facilitated_payments/core/validation/payment_link_validator.h"
#include "components/facilitated_payments/core/validation/pix_code_validator.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments::facilitated {
namespace {

static constexpr std::string_view kPixAccountLinkingHistogramPrefix =
    "FacilitatedPayments.Pix.AccountLinking.";

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

std::string ResultToString(bool result) {
  return result ? "Success" : "Failure";
}

std::string PaymentLinkFopSelectorTypesToString(
    PaymentLinkFopSelectorTypes payment_link_fop_selector_type) {
  switch (payment_link_fop_selector_type) {
    case PaymentLinkFopSelectorTypes::kEwalletOnly:
      return "EwalletOnly";
    case PaymentLinkFopSelectorTypes::kA2AOnly:
      return "A2AOnly";
    case PaymentLinkFopSelectorTypes::kEwalletAndA2A:
      return "EwalletAndA2A";
  }
}

std::string PixCodeValidationResultToString(PixCodeValidationResult result) {
  switch (result) {
    case PixCodeValidationResult::kDynamic:
      return "DynamicCode";
    case PixCodeValidationResult::kStatic:
      return "StaticCode";
    case PixCodeValidationResult::kInvalid:
      return "InvalidCode";
    case PixCodeValidationResult::kValidatorFailed:
      return "ValidatorFailed";
  }
}

}  // namespace

std::string SchemeToString(PaymentLinkValidator::Scheme scheme) {
  switch (scheme) {
    case PaymentLinkValidator::Scheme::kDuitNow:
      return "DuitNow";
    case PaymentLinkValidator::Scheme::kShopeePay:
      return "ShopeePay";
    case PaymentLinkValidator::Scheme::kTngd:
      return "Tngd";
    case PaymentLinkValidator::Scheme::kPromptPay:
      return "PromptPay";
    case PaymentLinkValidator::Scheme::kMomo:
      return "Momo";
    case PaymentLinkValidator::Scheme::kDana:
      return "Dana";
    case PaymentLinkValidator::Scheme::kInvalid:
      // This case can't happen because `kInvalid` causes an early return in the
      // PaymentLinkManager.
      NOTREACHED();
  }
}

void LogPixCodeCopied(ukm::SourceId ukm_source_id) {
  base::UmaHistogramBoolean("FacilitatedPayments.Pix.PixCodeCopied",
                            /*sample=*/true);
  ukm::builders::FacilitatedPayments_PixCodeCopied(ukm_source_id)
      .SetPixCodeCopied(true)
      .Record(ukm::UkmRecorder::Get());
}

void LogPaymentLinkDetected(ukm::SourceId ukm_source_id) {
  base::UmaHistogramBoolean("FacilitatedPayments.PaymentLinkDetected",
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

void LogPaymentCodeValidationResultAndLatency(PixCodeValidationResult result,
                                              base::TimeDelta duration) {
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Pix.PaymentCodeValidation.Result", result);
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.Pix.PaymentCodeValidation.",
                    PixCodeValidationResultToString(result), ".Latency"}),
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

void LogNonCardPaymentMethodsFopSelected(
    PaymentLinkFopSelectorTypes payment_link_fop_selector_fop_type,
    PaymentLinkFopSelectorAction payment_link_fop_selector_action,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  std::string payment_link_fop_selector_fop_type_string =
      PaymentLinkFopSelectorTypesToString(payment_link_fop_selector_fop_type);

  base::UmaHistogramEnumeration(
      base::StrCat({"FacilitatedPayments.",
                    payment_link_fop_selector_fop_type_string,
                    ".FopSelector.UserAction"}),
      payment_link_fop_selector_action);

  if (scheme.has_value() && *scheme != PaymentLinkValidator::Scheme::kInvalid) {
    base::UmaHistogramEnumeration(
        base::StrCat({"FacilitatedPayments.",
                      payment_link_fop_selector_fop_type_string,
                      ".FopSelector.UserAction.", SchemeToString(*scheme)}),
        payment_link_fop_selector_action);
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

void LogA2APayflowExitedReason(
    A2AFlowExitedReason reason,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramEnumeration("FacilitatedPayments.A2A.PayflowExitedReason",
                                reason);

  if (scheme.has_value() && *scheme != PaymentLinkValidator::Scheme::kInvalid) {
    base::UmaHistogramEnumeration(
        base::StrCat({"FacilitatedPayments.A2A.PayflowExitedReason.",
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

void LogPaymentLinkFopSelectorShownLatency(
    PaymentLinkFopSelectorTypes payment_link_fop_selector_type,
    base::TimeDelta latency,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  base::UmaHistogramLongTimes(
      base::StrCat(
          {"FacilitatedPayments.",
           PaymentLinkFopSelectorTypesToString(payment_link_fop_selector_type),
           ".FopSelectorShown.LatencyAfterDetectingPaymentLink"}),
      latency);

  if (scheme.has_value() && *scheme != PaymentLinkValidator::Scheme::kInvalid) {
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.",
                      PaymentLinkFopSelectorTypesToString(
                          payment_link_fop_selector_type),
                      ".FopSelectorShown.LatencyAfterDetectingPaymentLink.",
                      SchemeToString(*scheme)}),
        latency);
  }
}

void LogInvokePaymentAppResultAndLatency(
    bool result,
    base::TimeDelta latency,
    std::optional<PaymentLinkValidator::Scheme> scheme) {
  std::string result_string = ResultToString(result);
  base::UmaHistogramLongTimes(
      base::StrCat({"FacilitatedPayments.A2A.InvokePaymentApp.", result_string,
                    ".LatencyAfterDetectingPaymentLink"}),
      latency);

  if (scheme.has_value() && *scheme != PaymentLinkValidator::Scheme::kInvalid) {
    base::UmaHistogramLongTimes(
        base::StrCat({"FacilitatedPayments.A2A.InvokePaymentApp.",
                      result_string, ".LatencyAfterDetectingPaymentLink.",
                      SchemeToString(*scheme)}),
        latency);
  }
}

void LogPixAccountLinkingPromptAccepted() {
  base::UmaHistogramBoolean(
      base::StrCat({kPixAccountLinkingHistogramPrefix, "PromptAccepted"}),
      /*sample=*/true);
}

void LogPixAccountLinkingPromptShown() {
  base::UmaHistogramBoolean(
      base::StrCat({kPixAccountLinkingHistogramPrefix, "PromptShown"}),
      /*sample=*/true);
}

void LogGetDetailsForCreatePaymentInstrumentResultAndLatency(
    bool is_eligible,
    base::TimeDelta latency) {
  base::UmaHistogramBoolean(
      base::StrCat({kPixAccountLinkingHistogramPrefix,
                    "GetDetailsForCreatePaymentInstrument.Result"}),
      is_eligible);
  base::UmaHistogramLongTimes(
      base::StrCat({kPixAccountLinkingHistogramPrefix,
                    "GetDetailsForCreatePaymentInstrument.Latency"}),
      latency);
}

void LogPixAccountLinkingFlowExitedReason(
    PixAccountLinkingFlowExitedReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({kPixAccountLinkingHistogramPrefix, "FlowExitedReason"}),
      reason);
}

}  // namespace payments::facilitated
