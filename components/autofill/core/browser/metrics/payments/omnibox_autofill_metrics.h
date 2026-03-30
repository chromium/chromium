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
enum class OmniboxAutofillShowChipDecisionPart1 {
  // Expected remaining buckets:
  //  kEnterpriseAdminDisabled = 0,
  //  kNoCreditCardsOnFile = 1,
  //  kNotCompleteCreditCardForm = 2,
  //  kCouldNotDeduceCardNumberField = 3,
  //  kNonAllowlistedIframe = 4,

  kSuccess = 5,

  kMaxValue = kSuccess,
};

void LogOmniboxAutofillShowChipDecisionPart1(
    OmniboxAutofillShowChipDecisionPart1 metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OMNIBOX_AUTOFILL_METRICS_H_
