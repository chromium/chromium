// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/514743962 - All of these enums should be moved to more
// specific files and out of this file.  Do not add anything here.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_

namespace optimization_guide {

// The types of decisions that can be made for an optimization type.
//
// Keep in sync with OptimizationGuideOptimizationTypeDecision in enums.xml.
enum class OptimizationTypeDecision {
  kUnknown = 0,
  // The optimization type was allowed for the page load by an optimization
  // filter for the type.
  kAllowedByOptimizationFilter = 1,
  // The optimization type was not allowed for the page load by an optimization
  // filter for the type.
  kNotAllowedByOptimizationFilter = 2,
  // An optimization filter for that type was on the device but was not loaded
  // in time to make a decision. There is no guarantee that had the filter been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadOptimizationFilterButNotLoadedInTime = 3,
  // The optimization type was allowed for the page load based on a hint.
  kAllowedByHint = 4,
  // A hint that matched the page load was present but the optimization type was
  // not allowed to be applied.
  kNotAllowedByHint = 5,
  // A hint was available but there was not a page hint within that hint that
  // matched the page load.
  kNoMatchingPageHint = 6,
  // A hint that matched the page load was on the device but was not loaded in
  // time to make a decision. There is no guarantee that had the hint been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadHintButNotLoadedInTime = 7,
  // No hints were available in the cache that matched the page load.
  kNoHintAvailable = 8,
  // The OptimizationGuideDecider was not initialized yet.
  kDeciderNotInitialized = 9,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but was not available in time to make a
  // decision.
  kHintFetchStartedButNotAvailableInTime = 10,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but requested optimization type was not
  // registered.
  kRequestedUnregisteredType = 11,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but requested URL was invalid.
  kInvalidURL = 12,

  // Add new values above this line.
  kMaxValue = kInvalidURL,
};

// The statuses for racing a hints fetch with the current navigation based
// on the availability of hints for both the current host and URL.
//
// Keep in sync with OptimizationGuideRaceNavigationFetchAttemptStatus in
// enums.xml.
enum class RaceNavigationFetchAttemptStatus {
  kUnknown,
  // The race was not attempted because hint information for the host and URL
  // of the current navigation was already available.
  kRaceNavigationFetchNotAttempted,
  // The race was attempted for the host of the current navigation but not the
  // URL.
  kRaceNavigationFetchHost,
  // The race was attempted for the URL of the current navigation but not the
  // host.
  kRaceNavigationFetchURL,
  // The race was attempted for the host and URL of the current navigation.
  kRaceNavigationFetchHostAndURL,
  // A race for the current navigation's URL is already in progress.
  kRaceNavigationFetchAlreadyInProgress,
  // DEPRECATED: A race for the current navigation's URL was not attempted
  // because there were too many concurrent page navigation fetches in flight.
  kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches,

  // Add new values above this line.
  kMaxValue =
      kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches,
};

// Status of a request to fetch from the optimization guide service.
// This enum must remain synchronized with the enum
// |OptimizationGuideFetcherRequestStatus| in
// tools/metrics/histograms/enums.xml.
enum class FetcherRequestStatus {
  // No fetch status known. Used in testing.
  kUnknown,
  // Fetch request was sent and a response received.
  kSuccess,
  // Fetch request was sent but no response received.
  kResponseError,
  // DEPRECATED: Fetch request not sent because of offline network status.
  kDeprecatedNetworkOffline,
  // Fetch request not sent because fetcher was busy with another request.
  kFetcherBusy,
  // Hints fetch request not sent because the host and URL lists were empty.
  kNoHostsOrURLsToFetchHints,
  // Hints fetch request not sent because no supported optimization types were
  // provided.
  kNoSupportedOptimizationTypesToFetchHints,
  // Fetch request was canceled before completion.
  kRequestCanceled,
  // Fetch request was not started because user was not signed-in.
  kUserNotSignedIn,

  // Insert new values before this line.
  kMaxValue = kUserNotSignedIn
};

// Performance class of this device.
//
// These values are persisted to logs and prefs. Entries should not be
// renumbered and numeric values should never be reused.
enum class OnDeviceModelPerformanceClass : int {
  kUnknown = 0,

  // See on_device_model::mojom::PerformanceClass for explanation of these.
  kError = 1,
  kVeryLow = 2,
  kLow = 3,
  kMedium = 4,
  kHigh = 5,
  kVeryHigh = 6,

  // WARNING!: If you add a new performance class, please be aware of
  // `IsPerformanceClassCompatibleWithOnDeviceModel`.

  // The service crashed, so a valid value was not returned.
  kServiceCrash = 7,

  // GPU was blocklisted.
  kGpuBlocked = 8,

  // Native library failed to load.
  kFailedToLoadLibrary = 9,

  // This must be kept in sync with
  // OnDeviceModelPerformanceClass in optimization/enums.xml.

  // Insert new values before this line.
  kMaxValue = kFailedToLoadLibrary,
};



// Whether a response is complete or not.
enum class ResponseCompleteness {
  // This is a partial response, more output may follow.
  kPartial,
  // The response is complete and no more output will be produced.
  kComplete,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
