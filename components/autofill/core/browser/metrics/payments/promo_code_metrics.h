// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PROMO_CODE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PROMO_CODE_METRICS_H_

#include <vector>

#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"

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
  // Navigated to a page with a visible promo code field and an active promo
  // code offer present for the page origin.
  kNavigatedToPageWithPromoCode = 0,

  // Navigated to a page with a visible promo code field and no active promo
  // code offer present for the page origin.
  kNavigatedToPageWithoutPromoCode = 1,

  // Promo code suggestions shown.
  kPromoCodeSuggestionsShown = 2,

  // Promo code suggestion filled.
  kPromoCodeSuggestionFilled = 3,

  // Form submitted with promo code present for the page. This is tracked
  // separately of submissions with fills to compare conversion rates with users
  // that don't have a promo code saved for a page.
  kFormSubmittedWithPromoCodePresentForPage = 4,

  // Form submitted without promo code present for the page. This is used to
  // compare conversion rates between pages with and without promo codes.
  kFormSubmittedWithoutPromoCodePresentForPage = 5,

  // Form submitted after a promo code suggestion was filled. This is used to
  // see the funnel for how often users submit a form after filling a promo
  // code suggestion.
  kFormSubmittedAfterPromoCodeFilled = 6,

  // Form submitted and a merchant promo code field contained a value. This can
  // be compared with the autofilled metric to see how often users manually
  // enter a promo code in the field.
  kFormSubmittedWithPromoCodeFieldContainingValue = 7,

  kMaxValue = kFormSubmittedWithPromoCodeFieldContainingValue,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillPromoCodeFormEvent)

// Logs a promo code form event to Autofill.FormEvents.PromoCode.
void LogPromoCodeFormEvent(PromoCodeFormEvent event);

// Logs the number of promo codes stored for the user's account, in total and
// split by whether each promo code is valid or expired, measured at Chrome
// profile launch time. This is used to understand how many promo codes users
// have available to autofill.
void LogStoredPromoCodeMetrics(
    const std::vector<const AutofillOfferData*>& promo_codes);

// Logs the result when the offer notification bubble is closed.
void LogOfferNotificationBubbleResult(PaymentsUiClosedReason closed_reason);

}  // namespace autofill_metrics

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PROMO_CODE_METRICS_H_
