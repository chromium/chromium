// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_data_cleaner_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill::autofill_metrics {

void LogNumberOfNamesMigratedDuringCleanup(size_t num_names) {
  base::UmaHistogramCounts100(
      "Autofill.NumberOfNamesMigratedToAlternativeNamesDuringCleanUp",
      num_names);
}

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

void LogNumberOfProfilesConsideredForDedupePerCountryCode(
    const absl::flat_hash_map<std::string, int>&
        profile_count_by_country_code) {
  // TODO(b/496153767): Remove these metrics once enough data to evaluate the
  // bucketing optimization impact on deduplication is available.
  for (const auto& [country_code, count] : profile_count_by_country_code) {
    if (country_code.empty()) {
      base::UmaHistogramCounts1000(
          "Autofill.NumberOfProfilesWithMissingCountryCodeConsideredForDedupe",
          count);
    } else {
      base::UmaHistogramCounts1000(
          "Autofill.NumberOfProfilesPerValidCountryCodeConsideredForDedupe",
          count);
    }
  }
}

}  // namespace autofill::autofill_metrics
