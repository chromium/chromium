// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_

#include <string_view>

#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"

namespace multistep_filter {

// LINT.IfChange(MultistepFilterApplicationOutcome)
// Records the overall technical filter application outcome after a user accepts
// a Multistep Filter suggestion.
enum class MultistepFilterApplicationOutcome {
  kAllFiltersApplied = 0,
  kNotAllFiltersApplied = 1,
  kMaxValue = kNotAllFiltersApplied,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterApplicationOutcome)

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
inline constexpr char
    kMultistepFilterPostSuggestionApplicationUserEngagementHistogram[] =
        "MultistepFilter.PostSuggestionApplication.UserEngagement";
}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
