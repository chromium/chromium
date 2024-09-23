// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogCardUnmaskDurationAfterWebauthn(
    base::TimeDelta duration,
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    payments::PaymentsAutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramLongTimes("Autofill.BetterAuth.CardUnmaskDuration.Fido",
                              duration);
  base::UmaHistogramLongTimes(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          PaymentsRpcResultToMetricsSuffix(result),
      duration);
}

void LogCardUnmaskPreflightInitiated() {
  base::UmaHistogramBoolean("Autofill.BetterAuth.CardUnmaskPreflightInitiated",
                            true);
}

void LogCardUnmaskPreflightCalled(bool is_user_opted_in) {
  base::UmaHistogramBoolean(
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus",
      is_user_opted_in);
}

void LogCardUnmaskPreflightDuration(base::TimeDelta duration) {
  base::UmaHistogramLongTimes("Autofill.BetterAuth.CardUnmaskPreflightDuration",
                              duration);
}

void LogCardUnmaskTypeDecision(CardUnmaskTypeDecisionMetric metric) {
  base::UmaHistogramEnumeration("Autofill.BetterAuth.CardUnmaskTypeDecision",
                                metric);
}

void LogPreflightCallResponseReceivedOnCardSelection(
    PreflightCallEvent event,
    bool fido_opted_in,
    CreditCard::RecordType record_type) {
  std::string histogram_name =
      "Autofill.BetterAuth.PreflightCallResponseReceivedOnCardSelection.";
  histogram_name += fido_opted_in ? "OptedIn" : "OptedOut";
  histogram_name += AutofillMetrics::GetHistogramStringForCardType(record_type);
  base::UmaHistogramEnumeration(histogram_name, event);
}

void LogUserPerceivedLatencyOnCardSelection(PreflightCallEvent event,
                                            bool fido_auth_enabled) {
  std::string histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.";
  histogram_name += fido_auth_enabled ? "OptedIn" : "OptedOut";
  base::UmaHistogramEnumeration(histogram_name, event);
}

void LogUserPerceivedLatencyOnCardSelectionDuration(
    const base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "Duration",
      duration);
}

void LogUserPerceivedLatencyOnCardSelectionTimedOut(bool did_time_out) {
  base::UmaHistogramBoolean(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "TimedOutCvcFallback",
      did_time_out);
}

void LogUserVerifiabilityCheckDuration(base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      "Autofill.BetterAuth.UserVerifiabilityCheckDuration", duration);
}

void LogWebauthnOptChangeCalled(WebauthnOptInParameters metric) {
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow", metric);
}

void LogWebauthnOptInPromoNotOfferedReason(
    WebauthnOptInPromoNotOfferedReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.OptInPromoNotOfferedReason", reason);
}

void LogWebauthnEnrollmentPromptOffered(bool offered) {
  base::UmaHistogramBoolean("Autofill.BetterAuth.EnrollmentPromptOffered",
                            /*sample=*/offered);
}

void LogWebauthnOptInPromoShown() {
  base::UmaHistogramBoolean(
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow", true);
}

void LogWebauthnOptInPromoUserDecision(
    WebauthnOptInPromoUserDecisionMetric metric) {
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow", metric);
}

void LogWebauthnResult(WebauthnFlowEvent event, WebauthnResultMetric metric) {
  std::string histogram_name = "Autofill.BetterAuth.WebauthnResult.";
  switch (event) {
    case WebauthnFlowEvent::kImmediateAuthentication:
      histogram_name += "ImmediateAuthentication";
      break;
    case WebauthnFlowEvent::kAuthenticationAfterCvc:
      histogram_name += "AuthenticationAfterCVC";
      break;
    case WebauthnFlowEvent::kCheckoutOptIn:
      histogram_name += "CheckoutOptIn";
      break;
    case WebauthnFlowEvent::kSettingsPageOptIn:
      // TODO(crbug.com/345008736): Remove logic related to the settings page
      // FIDO opt-in flow.
      return;
  }
  base::UmaHistogramEnumeration(histogram_name, metric);
}

}  // namespace autofill::autofill_metrics
