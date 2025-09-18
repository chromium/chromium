// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_METRICS_H_

#include "components/country_codes/country_codes.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace search_engines {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SearchEngineChoiceScreenConditions)
enum class SearchEngineChoiceScreenConditions {
  // The user has a custom search engine set.
  kHasCustomSearchEngine = 0,
  // The user has a search provider list override.
  kSearchProviderOverride = 1,
  // The user is not in the regional scope.
  kNotInRegionalScope = 2,
  // A policy sets the default search engine or disables search altogether.
  kControlledByPolicy = 3,
  // The profile is out of scope.
  kProfileOutOfScope = 4,
  // An extension controls the default search engine.
  kExtensionControlled = 5,
  // The user is eligible to see the screen at the next opportunity.
  kEligible = 6,
  // The choice has already been completed.
  kAlreadyCompleted = 7,
  // The browser type is unsupported.
  kUnsupportedBrowserType = 8,
  // The feature can't run, it is disabled by local or remote configuration.
  kFeatureSuppressed = 9,
  // Some other dialog is showing and interfering with the choice one.
  kSuppressedByOtherDialog = 10,
  // The browser window can't fit the dialog's smallest variant.
  kBrowserWindowTooSmall = 11,
  // The user has a distribution custom search engine set as default.
  kHasDistributionCustomSearchEngine = 12,
  // The user has an unknown (which we assume is because it has been removed)
  // prepopulated search engine set as default.
  kHasRemovedPrepopulatedSearchEngine = 13,
  // The user does not have Google as the default search engine.
  kHasNonGoogleSearchEngine = 14,
  // The user is eligible, the app could have presented a dialog but the
  // application was started via an external intent and the dialog skipped.
  kAppStartedByExternalIntent = 15,
  // The browser attempting to show the choice screen in a dialog is already
  // showing a choice screen.
  kAlreadyBeingShown = 16,
  // The user made the choice in the guest session and opted to save it across
  // guest sessions.
  kUsingPersistedGuestSessionChoice = 17,
  // The user's current location is not compatible with the regional scope *and*
  // the regional program requires restricting to its associated countries.
  kIncompatibleCurrentLocation = 18,
  // The user is not eligible to make the choice because of their account
  // capabilities.
  kAccountNotEligible = 19,
  // The choice screen could not be presented because program settings require
  // it to not be shown on the UI surface that triggered it.
  kIneligibleSurface = 20,
  // The user is not eligible to make the choice because of their management
  // status.
  kManaged = 21,

  kMaxValue = kManaged,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:SearchEngineChoiceScreenConditions)
}  // namespace search_engines

namespace regional_capabilities {

using search_engines::SearchEngineChoiceScreenConditions;

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
