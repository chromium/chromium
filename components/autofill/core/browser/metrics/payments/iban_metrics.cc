// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill::autofill_metrics {

void LogStrikesPresentWhenIBANSaved(const int num_strikes) {
  base::UmaHistogramCounts100(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Local", num_strikes);
}

void LogIBANSaveNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric) {
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.StrikeDatabase.IbanSaveNotOfferedDueToMaxStrikes", metric);
}

}  // namespace autofill::autofill_metrics
