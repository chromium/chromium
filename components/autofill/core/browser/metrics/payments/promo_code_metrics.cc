// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogPromoCodeFormEvent(PromoCodeFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.PromoCode", event);
}

}  // namespace autofill::autofill_metrics
