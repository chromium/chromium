// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_WALLET_USAGE_DATA_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_WALLET_USAGE_DATA_METRICS_H_

#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// Logs metrics about the virtual card usage data associated with a Chrome
// profile. This should be called each time a Chrome profile is launched.
void LogStoredVirtualCardUsageCount(size_t usage_data_size);

// Logs whether a synced virtual card usage data is valid. Checked for every
// synced virtual card usage data.
void LogSyncedVirtualCardUsageDataBeingValid(bool invalid);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_WALLET_USAGE_DATA_METRICS_H_