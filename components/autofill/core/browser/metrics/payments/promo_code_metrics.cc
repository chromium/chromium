// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"

namespace autofill::autofill_metrics {

void LogPromoCodeFormEvent(PromoCodeFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.PromoCode", event);
}

void LogStoredPromoCodeMetrics(
    const std::vector<const AutofillOfferData*>& promo_codes) {
  const base::Time now = base::Time::Now();
  const size_t valid_count = std::ranges::count_if(
      promo_codes, [now](const AutofillOfferData* promo_code) {
        return promo_code->GetExpiry() > now;
      });
  base::UmaHistogramCounts100("Autofill.StoredPromoCodeCount",
                              promo_codes.size());
  base::UmaHistogramCounts100("Autofill.StoredPromoCodeCount.ValidCode",
                              valid_count);
  base::UmaHistogramCounts100("Autofill.StoredPromoCodeCount.ExpiredCode",
                              promo_codes.size() - valid_count);
}

}  // namespace autofill::autofill_metrics
