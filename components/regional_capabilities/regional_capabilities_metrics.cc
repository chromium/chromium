// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/country_codes/country_codes.h"

namespace regional_capabilities {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(CountryMatchingStatus)
enum class CountryMatchingStatus {
  kCountryMissing = 1,
  kVariationsCountryMissing = 2,
  kMatchesVariationsLatest = 3,
  kDoesNotMatchVariationsLatest = 4,
  kMaxValue = kDoesNotMatchVariationsLatest,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:CountryMatchingStatus)

CountryMatchingStatus ComputeCountryMatchingStatus(
    country_codes::CountryId country_id,
    country_codes::CountryId variations_latest_country) {
  if (!country_id.IsValid()) {
    return CountryMatchingStatus::kCountryMissing;
  }
  if (!variations_latest_country.IsValid()) {
    return CountryMatchingStatus::kVariationsCountryMissing;
  }
  if (country_id == variations_latest_country) {
    return CountryMatchingStatus::kMatchesVariationsLatest;
  }

  return CountryMatchingStatus::kDoesNotMatchVariationsLatest;
}

}  // namespace

void RecordLoadedCountrySource(LoadedCountrySource source) {
  base::UmaHistogramEnumeration("RegionalCapabilities.LoadedCountrySource",
                                source);
}

void RecordVariationsCountryMatching(
    country_codes::CountryId variations_latest_country,
    country_codes::CountryId persisted_profile_country,
    country_codes::CountryId current_device_country,
    bool is_device_country_from_fallback) {
  base::UmaHistogramEnumeration(
      "RegionalCapabilities.PersistedCountryMatching",
      ComputeCountryMatchingStatus(persisted_profile_country,
                                   variations_latest_country));
  base::UmaHistogramEnumeration(
      is_device_country_from_fallback
          ? "RegionalCapabilities.FallbackCountryMatching"
          : "RegionalCapabilities.FetchedCountryMatching",
      ComputeCountryMatchingStatus(current_device_country,
                                   variations_latest_country));
}

}  // namespace regional_capabilities
