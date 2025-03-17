// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogBnplPrefToggled(bool enabled) {
  base::UmaHistogramBoolean("Autofill.SettingsPage.BnplToggled", enabled);
}

void LogBnplIssuersSyncedCountAtStartup(int count) {
  base::UmaHistogramCounts100("Autofill.Bnpl.IssuersSyncedCount.Startup",
                              count);
}

}  // namespace autofill::autofill_metrics
