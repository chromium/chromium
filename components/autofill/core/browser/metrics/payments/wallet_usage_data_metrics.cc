// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill::autofill_metrics {

void LogStoredVirtualCardUsageCount(const size_t usage_data_size) {
  base::UmaHistogramCounts1000(
      "Autofill.VirtualCardUsageData.StoredUsageDataCount", usage_data_size);
}

void LogSyncedVirtualCardUsageDataBeingValid(bool valid) {
  base::UmaHistogramBoolean(
      "Autofill.VirtualCardUsageData.SyncedUsageDataBeingValid", valid);
}

}  // namespace autofill::autofill_metrics