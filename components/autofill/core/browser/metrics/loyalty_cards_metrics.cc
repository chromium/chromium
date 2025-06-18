// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/loyalty_cards_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

// Logs email or loyalty card suggestion accepted for
// EMAIL_OR_LOYALTY_MEMBERSHIP_ID field.
void LogEmailOrLoyaltyCardSuggestionAccepted(
    AutofillEmailOrLoyaltyCardAcceptanceMetricValue value) {
  base::UmaHistogramEnumeration(
      "Autofill.LoyaltyCard.EmailOrLoyaltyCardAcceptance", value);
}

}  // namespace autofill::autofill_metrics
