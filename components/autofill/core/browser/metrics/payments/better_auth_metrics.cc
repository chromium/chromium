// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogCardUnmaskDurationAfterWebauthn(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
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

void LogCardUnmaskPreflightDuration(const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes("Autofill.BetterAuth.CardUnmaskPreflightDuration",
                              duration);
}

void LogCardUnmaskTypeDecision(CardUnmaskTypeDecisionMetric metric) {
  base::UmaHistogramEnumeration("Autofill.BetterAuth.CardUnmaskTypeDecision",
                                metric);
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

void LogUserVerifiabilityCheckDuration(const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(
      "Autofill.BetterAuth.UserVerifiabilityCheckDuration", duration);
}

void LogWebauthnOptChangeCalled(bool request_to_opt_in,
                                bool is_checkout_flow,
                                WebauthnOptInParameters metric) {
  if (!request_to_opt_in) {
    DCHECK(!is_checkout_flow);
    base::UmaHistogramBoolean(
        "Autofill.BetterAuth.OptOutCalled.FromSettingsPage", true);
    return;
  }

  std::string histogram_name = "Autofill.BetterAuth.OptInCalled.";
  histogram_name += is_checkout_flow ? "FromCheckoutFlow" : "FromSettingsPage";
  base::UmaHistogramEnumeration(histogram_name, metric);
}

void LogWebauthnOptInPromoNotOfferedReason(
    WebauthnOptInPromoNotOfferedReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.OptInPromoNotOfferedReason", reason);
}

void LogWebauthnOptInPromoShown(bool is_checkout_flow) {
  std::string suffix =
      is_checkout_flow ? "FromCheckoutFlow" : "FromSettingsPage";
  base::UmaHistogramBoolean("Autofill.BetterAuth.OptInPromoShown." + suffix,
                            true);
}

void LogWebauthnOptInPromoUserDecision(
    bool is_checkout_flow,
    WebauthnOptInPromoUserDecisionMetric metric) {
  std::string suffix =
      (is_checkout_flow ? "FromCheckoutFlow" : "FromSettingsPage");
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.OptInPromoUserDecision." + suffix, metric);
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
      histogram_name += "SettingsPageOptIn";
      break;
  }
  base::UmaHistogramEnumeration(histogram_name, metric);
}

}  // namespace autofill::autofill_metrics
