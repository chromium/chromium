// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_

#include "components/country_codes/country_codes.h"

namespace regional_capabilities {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LoadedCountrySource)
enum class LoadedCountrySource {
  kNoneAvailable = 0,
  kCurrentOnly = 1,
  kPersistedOnly = 2,
  kBothMatch = 3,
  kPersistedPreferred = 4,
  kMaxValue = kPersistedPreferred
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:LoadedCapabilitiesCountrySource)

void RecordLoadedCountrySource(LoadedCountrySource source);

void RecordVariationsCountryMatching(
    country_codes::CountryId variations_latest_country,
    country_codes::CountryId persisted_profile_country,
    country_codes::CountryId current_device_country,
    bool is_device_country_from_fallback);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
