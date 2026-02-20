// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_ENUMS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_ENUMS_H_

#include <iosfwd>

namespace regional_capabilities {

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
  // kProfileOutOfScope = 4, // Deprecated
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
  // Although a choice had already been made, this profile is eligible due to
  // having been requested by the program for devices that can be identified has
  // having had this choice imported, for example from backup and restore flows.
  kEligibleForRestore = 22,
  // The user has a prepopulated engine that's not in the list of engines to be
  // offered on the choice screen set as default, while the program settings
  // require to highlight the current default.
  kHasNonHighlightablePrepopulatedSearchEngine = 23,

  kMaxValue = kHasNonHighlightablePrepopulatedSearchEngine,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:SearchEngineChoiceScreenConditions)

// Declaration for gtest, implemented in test-only targets.
void PrintTo(const SearchEngineChoiceScreenConditions& condition,
             std::ostream* os);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_ENUMS_H_
