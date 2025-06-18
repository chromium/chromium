// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOYALTY_CARDS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOYALTY_CARDS_METRICS_H_

namespace autofill::autofill_metrics {

// Enum containing possible values for
// `Autofill.LoyaltyCard.EmailOrLoyaltyCardAcceptance` metric.
// Entries should not be renumbered and
// numeric values should never be reused. Keep this enum up to date with the one
// in tools/metrics/histograms/metadata/autofill/enums.xml.
enum class AutofillEmailOrLoyaltyCardAcceptanceMetricValue {
  kEmailSelected = 0,
  kLoyaltyCardSelected = 1,
  kMaxValue = kLoyaltyCardSelected,
};

void LogEmailOrLoyaltyCardSuggestionAccepted(
    AutofillEmailOrLoyaltyCardAcceptanceMetricValue value);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOYALTY_CARDS_METRICS_H_
