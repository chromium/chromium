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

// This enum represents the second chunk of the decision of whether to show the
// omnibox autofill chip or not.
//
// Note that this histogram is recorded once per Omnibox Autofill flow (either
// when `OnFieldBecameVisible()` or `Reset()` is called), *not* necessarily once
// per page load.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(OmniboxAutofillShowChipDecisionPart2)
enum class OmniboxAutofillShowChipDecisionPart2 {
  // IntersectionObserver never reported that the field became visible.
  kIntersectionObserverNeverReportedVisibility = 0,

  // IntersectionObserver reported that the field became visible, and the
  // omnibox chip can be shown.
  kSuccess = 1,

  kMaxValue = kSuccess,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:OmniboxAutofillShowChipDecisionPart2)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(OmniboxAutofillEvents)
enum class OmniboxAutofillEvents {
  kChipShown = 0,
  kChipShownOnce = 1,
  kChipClicked = 2,
  kChipClickedOnce = 3,
  kSuggestionAccepted = 4,
  kSuggestionAcceptedOnce = 5,
  kFormFilled = 6,
  kFormFilledOnce = 7,
  kFormSubmittedOnce = 8,
  kMaxValue = kFormSubmittedOnce,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:OmniboxAutofillEvents)

void LogOmniboxAutofillShowChipDecisionPart1(
    OmniboxAutofillShowChipDecisionPart1 metric);

void LogOmniboxAutofillShowChipDecisionPart2(
    OmniboxAutofillShowChipDecisionPart2 metric);

void LogOmniboxAutofillEvents(OmniboxAutofillEvents metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OMNIBOX_AUTOFILL_METRICS_H_
