// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PROMO_CODE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PROMO_CODE_METRICS_H_

#include <vector>

namespace autofill {

class AutofillOfferData;

namespace autofill_metrics {

// Metrics to track user events related to merchant promo code fields.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AutofillPromoCodeFormEvent)
enum class PromoCodeFormEvent {
  // Enum value 0 is reserved for kNavigatedToPageWithPromoCode (navigated to a
  // page with a promo code field and promo code present).
  // Enum value 1 is reserved for kNavigatedToPageWithoutPromoCode (navigated to
  // a page with a promo code field and no promo code present).

  // Promo code suggestions shown.
  kPromoCodeSuggestionsShown = 2,

  // Promo code suggestion filled.
  kPromoCodeSuggestionFilled = 3,

  kMaxValue = kPromoCodeSuggestionFilled,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillPromoCodeFormEvent)

// Logs a promo code form event to Autofill.FormEvents.PromoCode.
void LogPromoCodeFormEvent(PromoCodeFormEvent event);

// Logs the number of promo codes stored for the user's account, in total and
// split by whether each promo code is valid or expired, measured at Chrome
// profile launch time. This is used to understand how many promo codes users
// have available to autofill.
void LogStoredPromoCodeMetrics(
    const std::vector<const AutofillOfferData*>& promo_codes);

}  // namespace autofill_metrics

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PROMO_CODE_METRICS_H_
