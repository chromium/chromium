// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_

#include <string>

#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/enums.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace regional_capabilities {

std::string ToString(SearchEngineChoiceScreenConditions condition);

// Return whether `condition` describes the profile as being eligible or not to
// proceed with showing choice screens.
bool IsEligible(SearchEngineChoiceScreenConditions condition);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ActiveRegionalProgram)
enum class ActiveRegionalProgram {
  // kUnknown = 0,
  kDefault = 1,
  kMixed = 2,
  kWaffle = 3,
  kTaiyaki = 4,
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
// LINT.IfChange(AndroidProgramResolution)
enum class AndroidProgramResolution {
  kSuccess = 0,
  kDefaultForOutOfProgramCountry = 1,
  kMaxValue = kDefaultForOutOfProgramCountry,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:AndroidProgramResolution)

void RecordAndroidProgramResolution(AndroidProgramResolution resolution);

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

void RecordEligibilityFunnelStageDetails(
    SearchEngineChoiceScreenConditions conditions);

void RecordTriggeringFunnelStageDetails(
    SearchEngineChoiceScreenConditions conditions);

// Records the histogram for the active regional program, used for UMA
// filtering.
//
// `programs` contains the active program for each profile. This method is
// responsible for aggregating these into a single histogram.
void RecordActiveRegionalProgram(
    const absl::flat_hash_set<ActiveRegionalProgram> programs);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProgramSpecificExclusion)
enum class ProgramSpecificExclusion {
  kNotRecordingChoiceFromSettings = 0,
  kNotPreservingChoiceFromOtherProgram = 1,
  kMaxValue = kNotPreservingChoiceFromOtherProgram,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/regional_capabilities/enums.xml:RegionalProgramSpecificExclusion)

void RecordProgramSpecificExclusion(ProgramSpecificExclusion exclusion);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
