// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_

#include <string_view>

#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"

namespace multistep_filter {

// =============================================================================
// ENUMS
// =============================================================================

// LINT.IfChange(MultistepFilterFacetType)
// If you add a new facet here, also update `MapStringToFacetType` in
// `multistep_filter_metrics_util.h/cc`.
enum class MultistepFilterFacetType {
  kUnknown = 0,
  kAgeChild = 1,
  kAmenityFreeBreakfast = 2,
  kAmenityFreeWifi = 3,
  kCabinClass = 4,
  kCountAdult = 5,
  kCountChild = 6,
  kCountInfant = 7,
  kCountInfantInLap = 8,
  kCountInfantInSeat = 9,
  kCountRoom = 10,
  kDateCheckin = 11,
  kDateCheckout = 12,
  kDateInbound = 13,
  kDateOutbound = 14,
  kLocationDestination = 15,
  kLocationInbound = 16,
  kLocationOutbound = 17,
  kPolicyFreeCancellation = 18,
  kPolicyPetsAllowed = 19,
  kRatingReviewMin = 20,
  kRatingStarMin = 21,
  kStops = 22,
  kTripType = 23,
  kMaxValue = kTripType,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/histograms.xml:MultistepFilterFacetType)

// LINT.IfChange(MultistepFilterPostSuggestionApplicationUserEngagement)
// Records post-acceptance behavior (tab close, navigation away from the
// suggestion-applied page (e.g. via Omnibox or bookmark), back
// navigation, or further on-page navigation) after accepting a Multistep Filter
// suggestion, distinguishing behavior within a session window.
enum class MultistepFilterPostSuggestionApplicationUserEngagement {
  kEngagedWithFurtherNavigationWithinSessionWindow = 0,
  kEngagedWithFurtherNavigationAfterSessionWindow = 1,
  kAbandonedWithinSessionWindowTabClosed = 2,
  kAbandonedAfterSessionWindowTabClosed = 3,
  kAbandonedWithinSessionWindowOmniboxOrBookmark = 4,
  kAbandonedAfterSessionWindowOmniboxOrBookmark = 5,
  kAbandonedWithinSessionWindowBackNavigation = 6,
  kAbandonedAfterSessionWindowBackNavigation = 7,
  kAbandonedWithinSessionWindowSessionOverride = 8,
  kAbandonedAfterSessionWindowSessionOverride = 9,
  kMaxValue = kAbandonedAfterSessionWindowSessionOverride,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterPostSuggestionApplicationUserEngagement)

// LINT.IfChange(MultistepFilterUserBehaviorAfterIgnore)
// Records the user's manual filtering behavior after ignoring or dismissing
// a Multistep Filter suggestion.
enum class MultistepFilterUserBehaviorAfterIgnore {
  kDidNotFilterFurther = 0,
  kAppliedSameFilters = 1,
  kAppliedDifferentFilters = 2,
  kMaxValue = kAppliedDifferentFilters,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterUserBehaviorAfterIgnore)

// LINT.IfChange(MultistepFilterRetentionState)
// If you add a new retention state here, also update `GetRetentionState` and
// `ForEachActiveRetentionSlice` in `multistep_filter_metrics_util.h/cc`.
enum class MultistepFilterRetentionState {
  kFirstImpression = 0,
  kAcceptedLastTime = 1,
  kRejectedLastTime_AcceptedAtLeastOnce = 2,
  kRejectedLastTime_NeverAccepted = 3,
  kMaxValue = kRejectedLastTime_NeverAccepted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterRetentionState)

// LINT.IfChange(MultistepFilterTaskType)
// If you add a new task type here, also update `MapStringToTaskType` in
// `multistep_filter_metrics_util.h/cc`.
enum class MultistepFilterTaskType {
  kUnknown = 0,
  kSearchFlights = 1,
  kSearchAccommodations = 2,
  kMaxValue = kSearchAccommodations,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterTaskType)

// =============================================================================
// HISTOGRAM NAMES & PREFIXES
// =============================================================================

// Suffix for histograms that are broken down by task type.
inline constexpr char kMultistepFilterByTaskHistogramPrefix[] = ".ByTask.";

// Suffix for histograms that are broken down by facet type.
inline constexpr char kMultistepFilterByFacetHistogramPrefix[] = ".ByFacet.";

// Suffix for histograms that are broken down by retention state.
inline constexpr char kMultistepFilterByRetentionHistogramPrefix[] =
    ".ByRetention.";

// Retention slice names.
inline constexpr char kRetentionSliceFirstImpression[] = "FirstImpression";
inline constexpr char kRetentionSliceAcceptedLastTime[] = "AcceptedLastTime";
inline constexpr char kRetentionSliceRejectedLastTime[] = "RejectedLastTime";
inline constexpr char kRetentionSliceAcceptedAtLeastOnce[] =
    "AcceptedAtLeastOnce";
inline constexpr char kRetentionSliceSawCuesButNeverAccepted[] =
    "SawCuesButNeverAccepted";

inline constexpr std::string_view kAllRetentionSlices[] = {
    kRetentionSliceFirstImpression, kRetentionSliceAcceptedLastTime,
    kRetentionSliceRejectedLastTime, kRetentionSliceAcceptedAtLeastOnce,
    kRetentionSliceSawCuesButNeverAccepted};

// Histogram names and prefixes for Multistep Filter metrics.
inline constexpr char kMultistepFilterAcceptanceHistogram[] =
    "MultistepFilter.Acceptance";
inline constexpr char kMultistepFilterAcceptanceInitialCueHistogram[] =
    "MultistepFilter.Acceptance.InitialCue";
inline constexpr char kMultistepFilterAcceptanceReopenedCueHistogram[] =
    "MultistepFilter.Acceptance.ReopenedCue";
inline constexpr char kMultistepFilterApplicationOutcomeHistogram[] =
    "MultistepFilter.ApplicationOutcome";
inline constexpr char kMultistepFilterNumberOfFacetsShownHistogram[] =
    "MultistepFilter.NumberOfFacetsShown";
inline constexpr char
    kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram[] =
        "MultistepFilter.NumberOfFacetsSuccessfullyApplied";

// Age metrics.
inline constexpr char kMultistepFilterSuggestionAgeAcceptedHistogram[] =
    "MultistepFilter.SuggestionAge.Accepted";
inline constexpr char
    kMultistepFilterSuggestionAgeAcceptedOnSameDomainHistogram[] =
        "MultistepFilter.SuggestionAge.AcceptedOnSameDomain";
inline constexpr char kMultistepFilterSuggestionAgeShownHistogram[] =
    "MultistepFilter.SuggestionAge.Shown";
inline constexpr char
    kMultistepFilterSuggestionAgeShownOnSameDomainHistogram[] =
        "MultistepFilter.SuggestionAge.ShownOnSameDomain";

// Latency metrics.
inline constexpr char
    kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram[] =
        "MultistepFilter.Time.SuggestionAcceptanceToApplied";
inline constexpr char
    kMultistepFilterTimeNavigationToSuggestionShownHistogram[] =
        "MultistepFilter.Time.NavigationToSuggestionShown";
inline constexpr char
    kMultistepFilterTimeNavigationToSuggestionAcceptedHistogram[] =
        "MultistepFilter.Time.NavigationToSuggestionAccepted";
inline constexpr char kMultistepFilterTimeSuggestionShownToAcceptedHistogram[] =
    "MultistepFilter.Time.SuggestionShownToAccepted";

// Post-acceptance metrics.
inline constexpr char
    kMultistepFilterPostSuggestionApplicationUserEngagementHistogram[] =
        "MultistepFilter.PostSuggestionApplication.UserEngagement";

inline constexpr char kMultistepFilterUserBehaviorAfterIgnoreHistogram[] =
    "MultistepFilter.UserBehaviorAfterIgnore";

// Synthetic trial names and groups.
inline constexpr char kMultistepFilterEvalsSyntheticTrialName[] =
    "SyntheticMultistepFilterEvals";
inline constexpr char kMultistepFilterEvalsSyntheticTrialGroupEnabled[] =
    "Enabled";

// =============================================================================
// TASK TYPE NAMES
// =============================================================================

inline constexpr char kMultistepFilterTaskTypeSearchAccommodations[] =
    "SEARCH_ACCOMMODATIONS";
inline constexpr char kMultistepFilterTaskTypeSearchFlights[] =
    "SEARCH_FLIGHTS";

// =============================================================================
// FILTER FACET NAMES
// =============================================================================

inline constexpr char kMultistepFilterFacetTypeAgeChild[] = "AGE_CHILD";
inline constexpr char kMultistepFilterFacetTypeAmenityFreeBreakfast[] =
    "AMENITY_FREE_BREAKFAST";
inline constexpr char kMultistepFilterFacetTypeAmenityFreeWifi[] =
    "AMENITY_FREE_WIFI";
inline constexpr char kMultistepFilterFacetTypeCabinClass[] = "CABIN_CLASS";
inline constexpr char kMultistepFilterFacetTypeCountAdult[] = "COUNT_ADULT";
inline constexpr char kMultistepFilterFacetTypeCountChild[] = "COUNT_CHILD";
inline constexpr char kMultistepFilterFacetTypeCountInfant[] = "COUNT_INFANT";
inline constexpr char kMultistepFilterFacetTypeCountInfantInLap[] =
    "COUNT_INFANT_IN_LAP";
inline constexpr char kMultistepFilterFacetTypeCountInfantInSeat[] =
    "COUNT_INFANT_IN_SEAT";
inline constexpr char kMultistepFilterFacetTypeCountRoom[] = "COUNT_ROOM";
inline constexpr char kMultistepFilterFacetTypeDateCheckin[] = "DATE_CHECKIN";
inline constexpr char kMultistepFilterFacetTypeDateCheckout[] = "DATE_CHECKOUT";
inline constexpr char kMultistepFilterFacetTypeDateInbound[] = "DATE_INBOUND";
inline constexpr char kMultistepFilterFacetTypeDateOutbound[] = "DATE_OUTBOUND";
inline constexpr char kMultistepFilterFacetTypeLocationDestination[] =
    "LOCATION_DESTINATION";
inline constexpr char kMultistepFilterFacetTypeLocationInbound[] =
    "LOCATION_INBOUND";
inline constexpr char kMultistepFilterFacetTypeLocationOutbound[] =
    "LOCATION_OUTBOUND";
inline constexpr char kMultistepFilterFacetTypePolicyFreeCancellation[] =
    "POLICY_FREE_CANCELLATION";
inline constexpr char kMultistepFilterFacetTypePolicyPetsAllowed[] =
    "POLICY_PETS_ALLOWED";
inline constexpr char kMultistepFilterFacetTypeRatingReviewMin[] =
    "RATING_REVIEW_MIN";
inline constexpr char kMultistepFilterFacetTypeRatingStarMin[] =
    "RATING_STAR_MIN";
inline constexpr char kMultistepFilterFacetTypeStops[] = "STOPS";
inline constexpr char kMultistepFilterFacetTypeTripType[] = "TRIP_TYPE";

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
