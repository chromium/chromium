// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_

#include "components/country_codes/country_codes.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace regional_capabilities {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ActiveRegionalProgram)
enum class ActiveRegionalProgram {
  kDefault = 0,
  kMixed = 1,
  kWaffle = 2,
  kTaiyaki = 3,
  kMaxValue = kTaiyaki,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:ActiveRegionalProgram)

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
  kPersistedPreferredOverFallback = 5,
  kCurrentPreferred = 6,
  kMaxValue = kCurrentPreferred
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:LoadedCapabilitiesCountrySource)

void RecordLoadedCountrySource(LoadedCountrySource source);

void RecordVariationsCountryMatching(
    country_codes::CountryId variations_latest_country,
    country_codes::CountryId persisted_profile_country,
    country_codes::CountryId current_device_country,
    bool is_device_country_from_fallback);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProgramAndLocationMatch)
enum class ProgramAndLocationMatch {
  SameAsProfileCountry = 0,
  SameRegionAsProgram = 1,
  NoMatch = 2,
  kMaxValue = NoMatch
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:RegionalProgramAndLocationMatch)

void RecordProgramAndLocationMatch(
    ProgramAndLocationMatch program_and_location_match);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FunnelStage)
enum class FunnelStage {
  kNotInRegionalScope = 0,
  kAlreadyCompleted = 1,
  kEligible = 2,
  kNotEligible = 3,
  kMaxValue = kNotEligible,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:RegionalCapabilitiesFunnelStage)

void RecordFunnelStage(FunnelStage stage);

// Records the histogram for the active regional program, used for UMA
// filtering.
//
// `programs` contains the active program for each profile. This method is
// responsible for aggregating these into a single histogram.
void RecordActiveRegionalProgram(
    const absl::flat_hash_set<ActiveRegionalProgram> programs);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
