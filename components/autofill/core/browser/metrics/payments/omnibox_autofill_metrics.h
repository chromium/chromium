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
// Note that this histogram is recorded once per call to
// `OnFieldTypesDetermined(~)` which is generally once per detected form, *not*
// once per page load or once per Omnibox Autofill flow.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(OmniboxAutofillShowChipDecisionPart1)
enum class OmniboxAutofillShowChipDecisionPart1 {
  // Was not the active, outermost main frame's BrowserAutofillManager.
  kNotActiveOutermostMainFrameBam = 0,

  // The Autofill payment methods policy pref (kAutofillCreditCardEnabled) was
  // disabled, regardless of if it was by the user, enterprise admin, or
  // extension.
  kAutofillPaymentMethodsPolicyDisabled = 1,

  // The user did not have any cards saved, so nothing can be autofilled.
  kNoCreditCardsSaved = 2,

  // Fetching the form via its FormGlobalId failed.
  kCouldNotFindCachedForm = 3,

  // The form did not have a credit card number and expiration date on it.
  kNotCompleteCreditCardForm = 4,

  // The form or client context was not secure, such as being HTTP.
  kFormOrClientContextNotSecure = 5,

  // The form contained more than one relevant CREDIT_CARD_NUMBER field.
  kFoundMultipleCreditCardNumberFields = 6,

  // The OptimizationGuideDecider was not present.
  kMissingOptimizationGuideDecider = 7,

  // Form field was in a non-allowlisted iframe.
  kNonAllowlistedIframe = 8,

  // All checks up to this point passed, and the IntersectionObserver checks can
  // be started.
  kSuccess = 9,

  kMaxValue = kSuccess,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:OmniboxAutofillShowChipDecisionPart1)

void LogOmniboxAutofillShowChipDecisionPart1(
    OmniboxAutofillShowChipDecisionPart1 metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OMNIBOX_AUTOFILL_METRICS_H_
