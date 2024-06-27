// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

namespace autofill::autofill_metrics {

// Card unmasking CVC authentication-related metrics.
// CVC authentication-related events.
enum class CvcAuthEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Unknown result. Should not happen.
  kUnknown = 0,
  // The CVC auth succeeded.
  kSuccess = 1,
  // The CVC auth failed because the flow was cancelled.
  kFlowCancelled = 2,
  // The CVC auth failed because the UnmaskCard request failed due to
  // authentication errors.
  kUnmaskCardAuthError = 3,
  // The CVC auth failed because the UnmaskCard request failed due to virtual
  // card retrieval errors.
  kUnmaskCardVirtualCardRetrievalError = 4,
  // The flow failed for technical reasons, such as closing the page or lack of
  // network connection.
  kGenericError = 5,
  // The CVC auth failed temporarily because the CVC didn't match the
  // expected value. This is a retryable error.
  kTemporaryErrorCvcMismatch = 6,
  // The CVC auth failed temporarily because the card used was expired. This is
  // a retryable error.
  kTemporaryErrorExpiredCard = 7,
  kMaxValue = kTemporaryErrorExpiredCard
};

// Logs when a CVC authentication starts.
void LogCvcAuthAttempt(CreditCard::RecordType card_type);

// Logs when a CVC authentication finishes.
void LogCvcAuthResult(CreditCard::RecordType card_type, CvcAuthEvent event);

// Logs when a retryable error occurs in the CVC authentication flow.
void LogCvcAuthRetryableError(CreditCard::RecordType card_type,
                              CvcAuthEvent event);

// Card unmasking OTP authentication-related metrics.
// OTP authentication-related events.
enum class OtpAuthEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Unknown results. Should not happen.
  kUnknown = 0,
  // The OTP auth succeeded.
  kSuccess = 1,
  // The OTP auth failed because the flow was cancelled.
  kFlowCancelled = 2,
  // The OTP auth failed because the SelectedChallengeOption request failed
  // due to generic errors.
  kSelectedChallengeOptionGenericError = 3,
  // The OTP auth failed because the SelectedChallengeOption request failed
  // due to virtual card retrieval errors.
  kSelectedChallengeOptionVirtualCardRetrievalError = 4,
  // The OTP auth failed because the UnmaskCard request failed due to
  // authentication errors.
  kUnmaskCardAuthError = 5,
  // The OTP auth failed because the UnmaskCard request failed due to virtual
  // card retrieval errors.
  kUnmaskCardVirtualCardRetrievalError = 6,
  // The OTP auth failed temporarily because the OTP was expired.
  kOtpExpired = 7,
  // The OTP auth failed temporarily because the OTP didn't match the expected
  // value.
  kOtpMismatch = 8,
  kMaxValue = kOtpMismatch
};

// The result of how the OTP input dialog was closed. This dialog is used for
// users to type in the received OTP value for card verification.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OtpInputDialogResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Unknown event, should not happen.
  kUnknown = 0,
  // The dialog was closed before the user entered any OTP and clicked the OK
  // button. This includes closing the dialog in an error state after a failed
  // unmask attempt.
  kDialogCancelledByUserBeforeConfirmation = 1,
  // The dialog was closed after the user entered a valid OTP and clicked the
  // OK button, and when the dialog was in a pending state.
  kDialogCancelledByUserAfterConfirmation = 2,
  // The dialog closed automatically after the OTP verification succeeded.
  kDialogClosedAfterVerificationSucceeded = 3,
  // The dialog closed automatically after a server failure response.
  kDialogClosedAfterVerificationFailed = 4,
  kMaxValue = kDialogClosedAfterVerificationFailed,
};

// The type of error message shown in the card unmask OTP input dialog.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OtpInputDialogError {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Unknown type, should not be used.
  kUnknown = 0,
  // The error indicating that the OTP is expired.
  kOtpExpiredError = 1,
  // The error indicating that the OTP is incorrect.
  kOtpMismatchError = 2,
  kMaxValue = kOtpMismatchError,
};

// Logs when an OTP authentication starts.
void LogOtpAuthAttempt(CardUnmaskChallengeOptionType type);

// Logs the final reason the OTP authentication dialog is closed, even if
// there were prior failures like OTP mismatch, and is done once per Attempt.
void LogOtpAuthResult(OtpAuthEvent event, CardUnmaskChallengeOptionType type);

// Logged every time a retriable error occurs, which could potentially be
// several times in the same flow (mismatch then mismatch then cancel, etc.).
void LogOtpAuthRetriableError(OtpAuthEvent event,
                              CardUnmaskChallengeOptionType type);

// Logs the roundtrip latency for UnmaskCardRequest sent by OTP
// authentication.
void LogOtpAuthUnmaskCardRequestLatency(base::TimeDelta latency,
                                        CardUnmaskChallengeOptionType type);

// Logs the roundtrip latency for SelectChallengeOptionRequest sent by OTP
// authentication.
void LogOtpAuthSelectChallengeOptionRequestLatency(
    base::TimeDelta latency,
    CardUnmaskChallengeOptionType type);

// Logs whenever the OTP input dialog is triggered and it is shown.
void LogOtpInputDialogShown(CardUnmaskChallengeOptionType type);

// Logs the result of how the dialog is dismissed.
void LogOtpInputDialogResult(OtpInputDialogResult result,
                             bool temporary_error_shown,
                             CardUnmaskChallengeOptionType type);

// Logs when the temporary error shown in the dialog.
void LogOtpInputDialogErrorMessageShown(OtpInputDialogError error,
                                        CardUnmaskChallengeOptionType type);

// Logs when the "Get New Code" button in the dialog is clicked and user is
// requesting a new OTP.
void LogOtpInputDialogNewOtpRequested(CardUnmaskChallengeOptionType type);

// Generate the OTP auth type string according to the challenge option type.
// This is used as a helper function for LogOtp methods.
std::string GetOtpAuthType(CardUnmaskChallengeOptionType type);

// Card unmasking risk-based authentication-related metrics.
// Risk-based authentication-related events.
enum class RiskBasedAuthEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // No further authentication is required.
  kNoAuthenticationRequired = 0,
  // The user needs to complete further authentication to retrieve the card.
  kAuthenticationRequired = 1,
  // The risk-based auth failed because the authentication was cancelled.
  kAuthenticationCancelled = 2,
  // The risk-based auth failed due to unexpected errors.
  kUnexpectedError = 3,
  kMaxValue = kUnexpectedError
};

// Logs when a risk-based authentication starts.
void LogRiskBasedAuthAttempt(CreditCard::RecordType card_type);

// Logs when a risk-based authentication finishes.
void LogRiskBasedAuthResult(CreditCard::RecordType card_type,
                            RiskBasedAuthEvent event);

// Logs the roundtrip latency for UnmaskCardRequest sent by risk-based
// authentication.
void LogRiskBasedAuthLatency(base::TimeDelta duration,
                             CreditCard::RecordType card_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_
