// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_

#include <memory>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

namespace autofill::autofill_metrics {

// Logs the offer data associated with a profile. This should be called each
// time a Chrome profile is launched.
void LogStoredOfferMetrics(
    const std::vector<std::unique_ptr<AutofillOfferData>>& offers);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_
