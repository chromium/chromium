// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_metrics.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/puma_histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/program_settings.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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
    bool is_current_device_country_from_fallback) {
  base::UmaHistogramEnumeration(
      "RegionalCapabilities.PersistedCountryMatching",
      ComputeCountryMatchingStatus(persisted_profile_country,
                                   variations_latest_country));
  base::UmaHistogramEnumeration(
      is_current_device_country_from_fallback
          ? "RegionalCapabilities.FallbackCountryMatching"
          : "RegionalCapabilities.FetchedCountryMatching",
      ComputeCountryMatchingStatus(current_device_country,
                                   variations_latest_country));
}

void RecordProgramAndLocationMatch(
    ProgramAndLocationMatch program_and_location_match) {
  base::UmaHistogramEnumeration(
      "RegionalCapabilities.FunnelStage.RegionalPresence",
      program_and_location_match);
}

void RecordFunnelStage(FunnelStage stage) {
  base::UmaHistogramEnumeration("RegionalCapabilities.FunnelStage.Reported",
                                stage);
  base::PumaHistogramEnumeration(
      base::PumaType::kRc, "PUMA.RegionalCapabilities.FunnelStage.Reported",
      stage);
}

void RecordEligibilityFunnelStageDetails(
    SearchEngineChoiceScreenConditions conditions) {
  base::UmaHistogramEnumeration("RegionalCapabilities.FunnelStage.Eligibility",
                                conditions);
  base::PumaHistogramEnumeration(
      base::PumaType::kRc, "PUMA.RegionalCapabilities.FunnelStage.Eligibility",
      conditions);
}

void RecordTriggeringFunnelStageDetails(
    SearchEngineChoiceScreenConditions conditions) {
  base::UmaHistogramEnumeration("RegionalCapabilities.FunnelStage.Triggering",
                                conditions);
  base::PumaHistogramEnumeration(
      base::PumaType::kRc, "PUMA.RegionalCapabilities.FunnelStage.Triggering",
      conditions);
}

void RecordActiveRegionalProgram(
    const absl::flat_hash_set<ActiveRegionalProgram> programs) {
  base::UmaHistogramBoolean(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", !programs.empty());
  if (programs.empty()) {
    return;
  }

  auto non_default_programs(programs);
  non_default_programs.erase(ActiveRegionalProgram::kDefault);

  ActiveRegionalProgram merged_program;
  switch (non_default_programs.size()) {
    case 0:
      merged_program = ActiveRegionalProgram::kDefault;
      break;
    case 1:
      merged_program = *non_default_programs.cbegin();
      break;
    default:
      merged_program = ActiveRegionalProgram::kMixed;
      break;
  }

  base::UmaHistogramEnumeration("RegionalCapabilities.ActiveRegionalProgram2",
                                merged_program);
}

void RecordProgramSpecificExclusion(ProgramSpecificExclusion exclusion) {
  base::UmaHistogramEnumeration(
      "RegionalCapabilities.Debug.ProgramSpecificExclusion", exclusion);
}

}  // namespace regional_capabilities
