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

// LINT.IfChange(MultistepFilterPostSuggestionApplicationFirstNavigation)
// Records navigation behavior after accepting a Multistep Filter suggestion,
// distinguishing behavior within a session window.
enum class MultistepFilterPostSuggestionApplicationFirstNavigation {
  kBackNavigationWithinSessionWindow = 0,
  kBackNavigationAfterSessionWindow = 1,
  kForwardOrOtherNavigation = 2,
  kMaxValue = kForwardOrOtherNavigation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterPostSuggestionApplicationFirstNavigation)

// LINT.IfChange(MultistepFilterPostSuggestionApplicationTabClose)
// Records tab closure behavior after accepting a Multistep Filter suggestion,
// distinguishing behavior within a session window.
enum class MultistepFilterPostSuggestionApplicationTabClose {
  kTabClosedWithinSessionWindow = 0,
  kTabClosedWithFurtherNavigation = 1,
  kTabClosedAfterSessionWindow = 2,
  kMaxValue = kTabClosedAfterSessionWindow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:MultistepFilterPostSuggestionApplicationTabClose)

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
    kMultistepFilterPostSuggestionApplicationFirstNavigationHistogram[] =
        "MultistepFilter.PostSuggestionApplication.FirstNavigation";
inline constexpr char
    kMultistepFilterPostSuggestionApplicationTabCloseHistogram[] =
        "MultistepFilter.PostSuggestionApplication.TabClose";
}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
