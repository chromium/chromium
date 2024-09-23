// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/payments_window_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void LogVcn3dsFlowEvent(Vcn3dsFlowEvent flow_event,
                        bool user_consent_already_given) {
  base::UmaHistogramEnumeration("Autofill.Vcn3ds.FlowEvents", flow_event);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.Vcn3ds.FlowEvents", user_consent_already_given
                                                      ? ".ConsentAlreadyGiven"
                                                      : ".ConsentNotGivenYet"}),
      flow_event);
}

void LogPaymentsWindowUserConsentDialogResult(
    PaymentsWindowUserConsentDialogResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.Vcn3ds.PaymentsWindowUserConsentDialogResult", result);
}

void LogPaymentsWindowUserConsentDialogShown() {
  base::UmaHistogramBoolean(
      "Autofill.Vcn3ds.PaymentsWindowUserConsentDialogShown", /*sample=*/true);
}

// Logs the duration that a VCN 3DS auth flow took. `duration` is the time
// between when the pop-up was displayed to the user, and when the pop-up
// closed. `success` is whether the auth was a success or failure.
void LogVcn3dsAuthLatency(base::TimeDelta duration, bool success) {
  base::UmaHistogramLongTimes(base::StrCat({"Autofill.Vcn3ds.Latency.",
                                            success ? "Success" : "Failure"}),
                              duration);
}

}  // namespace autofill::autofill_metrics
