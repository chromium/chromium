// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_

#include <optional>

#include "base/types/expected.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/facilitated_payments/core/validation/payment_link_validator.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}

namespace payments::facilitated {

// A payment system that is currently running.
enum class FacilitatedPaymentsType {
  kEwallet = 0,
  kPix = 1,
};

// Available ewallet accounts type for this current profile. Indicates the count
// of available eWallets and whether they’re device bound or not.
enum class AvailableEwalletsConfiguration {
  // Only one eWallet is available, and it is already bound to the device.
  kSingleBoundEwallet = 0,
  // Only one eWallet is available, and it’s not bound to the device.
  kSingleUnboundEwallet = 1,
  // Mutilple eWallets are available..
  kMultipleEwallets = 2,
};

// Reasons for why the eWallet payflow was exited early. These only include the
// reasons after the renderer has detected a valid payment link and sent the
// signal to the browser process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(EwalletFlowExitedReason)
enum class EwalletFlowExitedReason {
  // The code for the payflow is not valid.
  kLinkIsInvalid = 0,
  // The user has opted out of the payflow.
  kUserOptedOut = 1,
  // The user has no supported accounts available for the payflow.
  kNoSupportedEwallet = 2,
  // The device is in landscape orientation when payflow was to be triggered.
  kLandscapeScreenOrientation = 3,
  // The domain for the payment link is not allowlisted.
  kNotInAllowlist = 4,
  // The API Client is not available when the payflow was to be triggered.
  kApiClientNotAvailable = 5,
  // The risk data needed to send the server request is not available.
  kRiskDataEmpty = 6,
  // The client token needed to send the server request is not available.
  kClientTokenNotAvailable = 7,
  // The InitiatePayment response indicated a failure.
  kInitiatePaymentFailed = 8,
  // The action token returned in the InitiatePayment response is not available.
  kActionTokenNotAvailable = 9,
  // The user has logged out after selecting a payment method.
  kUserLoggedOut = 10,
  // The FOP selector either wasn't shown, or was dismissed not as a result of a
  // user action.
  kFopSelectorClosedNotByUser = 11,
  // The FOP selector was dismissed by a user action e.g., swiping down, tapping
  // on the webpage behind the FOP selector, or tapping on the omnibox.
  kFopSelectorClosedByUser = 12,
  // The device is a foldable device which we don't support yet.
  kFoldableDevice = 13,
  kMaxStrikes = 14,
  kMaxValue = kMaxStrikes
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/facilitated_payments/enums.xml:FacilitatedPayments.EwalletFlowExitedReason)

// Reasons for why the Pix payflow was exited early. These only include the
// reasons after the renderer has detected a valid code and sent the signal to
// the browser process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PixFlowExitedReason)
enum class PixFlowExitedReason {
  // The code validator encountered an error.
  kCodeValidatorFailed = 0,
  // The code for the payflow is not valid.
  kInvalidCode = 1,
  // The user has opted out of the payflow.
  kUserOptedOut = 2,
  // The user has no linked accounts available for the payflow.
  kNoLinkedAccount = 3,
  // The device is in landscape orientation when payflow was to be triggered.
  kLandscapeScreenOrientation = 4,
  // The API Client is not available when the payflow was to be triggered.
  kApiClientNotAvailable = 5,
  // The risk data needed to send the server request is not available.
  kRiskDataNotAvailable = 6,
  // The client token needed to send the server request is not available.
  kClientTokenNotAvailable = 7,
  // The InitiatePayment response indicated a failure.
  kInitiatePaymentFailed = 8,
  // The action token returned in the InitiatePayment response is not available.
  kActionTokenNotAvailable = 9,
  // The user has logged out after selecting a payment method.
  kUserLoggedOut = 10,
  // The FOP selector either wasn't shown, or was dismissed not as a result of a
  // user action.
  kFopSelectorClosedNotByUser = 11,
  // The FOP selector was dismissed by a user action e.g., swiping down, tapping
  // on the webpage behind the FOP selector, or tapping on the omnibox.
  kFopSelectorClosedByUser = 12,
  // Chrome attempted, but was unable to invoke purchase action.
  kPurchaseActionCouldNotBeInvoked = 13,
  kMaxValue = kPurchaseActionCouldNotBeInvoked
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/facilitated_payments/enums.xml:FacilitatedPayments.PixFlowExitedReason)

// Log when a Pix code is copied to the clippboard on an allowlisted merchant
// website.
void LogPixCodeCopied(ukm::SourceId ukm_source_id);

// Log when a given payment link in a certain page for an eWallet push payment
// flow is detected.
void LogPaymentLinkDetected(ukm::SourceId ukm_source_id);

// Log when the eWallet FOP selector UI is shown.
void LogEwalletFopSelectorShownUkm(ukm::SourceId ukm_source_id,
                                   PaymentLinkValidator::Scheme scheme);

// Log when the Pix FOP selector UI is shown.
void LogPixFopSelectorShownUkm(ukm::SourceId ukm_source_id);

// Log after user accepts / rejects the Pix UI. The `accepted` will be false
// if the user rejects the UI, and it will be true if the user accepts the
// selector UI and selects a FoP to use.
void LogPixFopSelectorResultUkm(bool accepted, ukm::SourceId ukm_source_id);

// Log after user accepts / rejects the eWallet UI. The `accepted` will be false
// if the user rejects the UI, and it will be true if the user accepts the
// selector UI and selects a FoP to use.
void LogEwalletFopSelectorResultUkm(bool accepted,
                                    ukm::SourceId ukm_source_id,
                                    PaymentLinkValidator::Scheme scheme);

// Logs that the user has selected a Pix FOP to pay with. Also logs the time
// taken by the user to select the Pix account after the FOP selector is shown.
void LogPixFopSelectedAndLatency(base::TimeDelta duration);

// Log when user selects an eWallet FOP to pay with.
void LogEwalletFopSelected(AvailableEwalletsConfiguration type);

// Log the result and latency for validating a payment code using
// `data_decoder::DataDecoder`.
void LogPaymentCodeValidationResultAndLatency(
    base::expected<bool, std::string> result,
    base::TimeDelta duration);

// Log the result of whether the facilitated payments is available or not and
// the check's latency.
// `payment_type` must be either `kEwallet` or `kPix`.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogApiAvailabilityCheckResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool result,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Logs the result and latency for fetching the risk data. If the risk data was
// fetched successfully, `was_successful` is true. The call took `duration` to
// complete.
// `payment_type` must be either `kEwallet` or `kPix`.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogLoadRiskDataResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool was_successful,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Log the result and the latency of the GetClientToken call made to api client.
// `payment_type` must be either `kEwallet` or `kPix`.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogGetClientTokenResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool result,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Log the reason for the eWallet flow was exited early. This includes all the
// reasons after receiving a signal from the renderer process that a valid
// payment link has been found.
void LogEwalletFlowExitedReason(
    EwalletFlowExitedReason reason,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Log the reason for the Pix flow was exited early. This includes all the
// reasons after receiving a signal from the renderer process that a valid code
// has been found.
void LogPixFlowExitedReason(PixFlowExitedReason reason);

// Log the attempt to send the call to the InitiatePayment backend endpoint.
// `payment_type` must be either `kEwallet` or `kPix`.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogInitiatePaymentAttempt(
    FacilitatedPaymentsType payment_type,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Log the result and latency for the InitiatePayment backend endpoint.
// `payment_type` must be either `kEwallet` or `kPix`.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogInitiatePaymentResultAndLatency(
    FacilitatedPaymentsType payment_type,
    bool result,
    base::TimeDelta duration,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Log the attempt to send the call to the InitiatePurchaseAction backend
// endpoint.
// `payment_type` must be either `kEwallet` or `kPix`.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogInitiatePurchaseActionAttempt(
    FacilitatedPaymentsType payment_type,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Log the result and latency for the InitiatePurchaseAction call made to the
// payments platform (client) during Pix payflow.
void LogPixInitiatePurchaseActionResultAndLatency(PurchaseActionResult result,
                                                  base::TimeDelta duration);

// Logs the result and the overall latency for the Pix transaction. The latency
// is measured between the time when the Pix code was copied to the time when
// Chrome receives `PurchaseActionResult` from the payments backend.
void LogPixTransactionResultAndLatency(PurchaseActionResult result,
                                       base::TimeDelta duration);

// Log the result and latency for the InitiatePurchaseAction call made to the
// payments platform (client) during eWallet payflow.
void LogEwalletInitiatePurchaseActionResultAndLatency(
    PurchaseActionResult result,
    base::TimeDelta duration,
    PaymentLinkValidator::Scheme scheme,
    bool is_device_bound);

// Log the UKM for the InitiatePurchaseAction result.
void LogInitiatePurchaseActionResultUkm(PurchaseActionResult result,
                                        ukm::SourceId ukm_source_id);

// Logs showing a new UI screen.
// The `scheme` parameter is required for the 'kEwallet' payment type and should
// not be `kInvalid`.
void LogUiScreenShown(
    FacilitatedPaymentsType payment_type,
    UiState ui_screen,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

// Logs the latency for displaying the FOP selector
// - Pix: Measures latency from Pix payment code copy to FOP selector display.
// - Ewallet: Measures latency from payment link detection to FOP selector
// display. The `scheme` parameter is required for the 'kEwallet' payment type
// and should not be `kInvalid`.
void LogFopSelectorShownLatency(
    FacilitatedPaymentsType payment_type,
    base::TimeDelta latency,
    std::optional<PaymentLinkValidator::Scheme> scheme = std::nullopt);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
