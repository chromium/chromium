// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/risk_data_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogRiskDataLoadingLatency(base::TimeDelta duration) {
  base::UmaHistogramLongTimes("Autofill.RiskDataLoading.Latency", duration);
}

}  // namespace autofill::autofill_metrics
