// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BETTER_AUTH_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BETTER_AUTH_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// Metric for tracking which authentication method was used for a user with
// FIDO authentication enabled.
enum class CardUnmaskTypeDecisionMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Only WebAuthn prompt was shown.
  kFidoOnly = 0,
  // CVC authentication was required in addition to WebAuthn.
  kCvcThenFido = 1,
  kMaxValue = kCvcThenFido,
};

// Events related to user-perceived latency due to GetDetailsForGetRealPan
// call.
enum class PreflightCallEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Returned before card chosen.
  kPreflightCallReturnedBeforeCardChosen = 0,
  // Did not return before card was chosen. When opted-in, this means
  // the UI had to wait for the call to return. When opted-out, this means we
  // did not offer to opt-in.
  kCardChosenBeforePreflightCallReturned = 1,
  // Preflight call was irrelevant; skipped waiting.
  kDidNotChooseMaskedCard = 2,
  kMaxValue = kDidNotChooseMaskedCard,
};

// Possible scenarios where a WebAuthn prompt may show.
enum class WebauthnFlowEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // WebAuthn is immediately prompted for unmasking.
  kImmediateAuthentication = 0,
  // WebAuthn is prompted after a CVC check.
  kAuthenticationAfterCvc = 1,
  // WebAuthn is prompted after being offered to opt-in from a checkout flow.
  kCheckoutOptIn = 2,
  // WebAuthn is prompted after being offered to opt-in from the settings
  // page.
  kSettingsPageOptIn = 3,
  kMaxValue = kSettingsPageOptIn,
};

// The parameters with which opt change was called.
enum class WebauthnOptInParameters {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Call made to fetch a challenge.
  kFetchingChallenge = 0,
  // Call made with signature of creation challenge.
  kWithCreationChallenge = 1,
  // Call made with signature of request challenge.
  kWithRequestChallenge = 2,
  kMaxValue = kWithRequestChallenge,
};

// The user decision for the WebAuthn opt-in promo.
enum class WebauthnOptInPromoUserDecisionMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // User accepted promo.
  kAccepted = 0,
  // User immediately declined promo.
  kDeclinedImmediately = 1,
  // Once user accepts the dialog, a round-trip call to Payments is sent,
  // which is required for user authentication. The user has the option to
  // cancel the dialog before the round-trip call is returned.
  kDeclinedAfterAccepting = 2,
  kMaxValue = kDeclinedAfterAccepting,
};

// The result of a WebAuthn user-verification prompt.
enum class WebauthnResultMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // User-verification succeeded.
  kSuccess = 0,
  // Other checks failed (e.g. invalid domain, algorithm unsupported, etc.)
  kOtherError = 1,
  // User either failed verification or cancelled.
  kNotAllowedError = 2,
  kMaxValue = kNotAllowedError,
};

// Logs the card fetch latency |duration| after a WebAuthn prompt. |result|
// indicates whether the unmasking request was successful or not. |card_type|
// indicates the type of the credit card that the request fetched.
void LogCardUnmaskDurationAfterWebauthn(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type);

// Logs the number of times that we initiate the card unmask preflight flow.
// This will log both when the user is verifiable as well as when the user is
// not verifiable, as it is logged before we check whether the user is
// verifiable.
void LogCardUnmaskPreflightInitiated();

// Logs the count of calls to PaymentsClient::GetUnmaskDetails() (aka
// GetDetailsForGetRealPan). If `is_user_opted_in` is true, then the user is
// opted-in to FIDO auth, and if the user is not opted-in to FIDO auth then
// `is_user_opted_in` is false.
void LogCardUnmaskPreflightCalled(bool is_user_opted_in);

// Logs the duration of the PaymentsClient::GetUnmaskDetails() call (aka
// GetDetailsForGetRealPan).
void LogCardUnmaskPreflightDuration(const base::TimeDelta& duration);

// Logs which unmask type was used for a user with FIDO authentication
// enabled.
void LogCardUnmaskTypeDecision(CardUnmaskTypeDecisionMetric metric);

// Logs the existence of any user-perceived latency between selecting a Google
// Payments server card and seeing a card unmask prompt.
void LogUserPerceivedLatencyOnCardSelection(PreflightCallEvent event,
                                            bool fido_auth_enabled);

// Logs the duration of any user-perceived latency between selecting a Google
// Payments server card and seeing a card unmask prompt (CVC or FIDO).
void LogUserPerceivedLatencyOnCardSelectionDuration(
    const base::TimeDelta duration);

// Logs whether or not the verifying pending dialog timed out between
// selecting a Google Payments server card and seeing a card unmask prompt.
void LogUserPerceivedLatencyOnCardSelectionTimedOut(bool did_time_out);

// Logs the duration of WebAuthn's
// IsUserVerifiablePlatformAuthenticatorAvailable() call. It is supposedly an
// extremely quick IPC.
void LogUserVerifiabilityCheckDuration(const base::TimeDelta& duration);

// Logs the count of calls to PaymentsClient::OptChange() (aka
// UpdateAutofillUserPreference).
void LogWebauthnOptChangeCalled(bool request_to_opt_in,
                                bool is_checkout_flow,
                                WebauthnOptInParameters metric);

// Logs the number of times the opt-in promo for enabling FIDO authentication
// for card unmasking has been shown.
void LogWebauthnOptInPromoShown(bool is_checkout_flow);

// Logs the user response to the opt-in promo for enabling FIDO authentication
// for card unmasking.
void LogWebauthnOptInPromoUserDecision(
    bool is_checkout_flow,
    WebauthnOptInPromoUserDecisionMetric metric);

// Logs the result of a WebAuthn prompt.
void LogWebauthnResult(WebauthnFlowEvent event, WebauthnResultMetric metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BETTER_AUTH_METRICS_H_
