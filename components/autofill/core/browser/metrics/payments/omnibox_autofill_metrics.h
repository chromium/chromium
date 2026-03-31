// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OMNIBOX_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OMNIBOX_AUTOFILL_METRICS_H_

namespace autofill::autofill_metrics {

// This enum represents the first chunk of the decision of whether to show the
// omnibox autofill chip or not. Contains everything from the point field types
// were parsed to when the IntersectionObserver is started. After that point,
// the flow could drop off without warning, which is why it is split from
// OmniboxAutofillShowChipDecisionPart2.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(OmniboxAutofillShowChipDecisionPart1)
enum class OmniboxAutofillShowChipDecisionPart1 {
  // The Autofill payment methods policy pref (kAutofillCreditCardEnabled) was
  // disabled, regardless of if it was by the user, enterprise admin, or
  // extension.
  kAutofillPaymentMethodsPolicyDisabled = 0,

  // The user did not have any cards saved, so nothing can be autofilled.
  kNoCreditCardsSaved = 1,

  // Fetching the form via its FormGlobalId failed.
  kCouldNotFindCachedForm = 2,

  // The form did not have a credit card number and expiration date on it.
  kNotCompleteCreditCardForm = 3,

  // Expected remaining buckets:
  //  kCouldNotDeduceCardNumberField = 4,
  //  kNonAllowlistedIframe = 5,

  kSuccess = 6,

  kMaxValue = kSuccess,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:OmniboxAutofillShowChipDecisionPart1)

void LogOmniboxAutofillShowChipDecisionPart1(
    OmniboxAutofillShowChipDecisionPart1 metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OMNIBOX_AUTOFILL_METRICS_H_
