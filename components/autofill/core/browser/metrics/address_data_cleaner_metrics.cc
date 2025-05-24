// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_data_cleaner_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogNumberOfProfilesConsideredForDedupe(size_t num_considered) {
  base::UmaHistogramCounts100("Autofill.NumberOfProfilesConsideredForDedupe",
                              num_considered);
}

void LogNumberOfProfilesRemovedDuringDedupe(size_t num_removed) {
  base::UmaHistogramCounts100("Autofill.NumberOfProfilesRemovedDuringDedupe",
                              num_removed);
}

void LogNumberOfAddressesDeletedForDisuse(size_t num_profiles) {
  base::UmaHistogramCounts100("Autofill.AddressesDeletedForDisuse",
                              num_profiles);
}

}  // namespace autofill::autofill_metrics
