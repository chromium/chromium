// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_

#include <limits.h>

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace history {

// Organic Repeatable Queries
BASE_DECLARE_FEATURE(kOrganicRepeatableQueries);
extern const base::FeatureParam<int> kMaxNumRepeatableQueries;
extern const base::FeatureParam<bool> kScaleRepeatableQueriesScores;
extern const base::FeatureParam<bool> kPrivilegeRepeatableQueries;
extern const base::FeatureParam<bool> kRepeatableQueriesIgnoreDuplicateVisits;
extern const base::FeatureParam<int> kRepeatableQueriesMaxAgeDays;
extern const base::FeatureParam<int> kRepeatableQueriesMinVisitCount;

// When enabled, this feature flag begins populating the VisitedLinkDatabase
// with data.
BASE_DECLARE_FEATURE(kPopulateVisitedLinkDatabase);

// Most Visited Tiles scoring function changes.
BASE_DECLARE_FEATURE(kMostVisitedTilesNewScoring);

// Most Visited Tiles Visual Deduplication.
BASE_DECLARE_FEATURE(kMostVisitedTilesVisualDeduplication);

// List of values for |kMvtScoringParamRecencyFactor|
inline constexpr char kMvtScoringParamRecencyFactor_Classic[] = "default";
inline constexpr char kMvtScoringParamRecencyFactor_Decay[] = "decay";
inline constexpr char kMvtScoringParamRecencyFactor_DecayStaircase[] =
    "decay_staircase";

// The name of the recency factor strategy to use for MVT computation.
inline constexpr base::FeatureParam<std::string> kMvtScoringParamRecencyFactor(
    &kMostVisitedTilesNewScoring,
    "recency_factor",
#if BUILDFLAG(IS_ANDROID)
    kMvtScoringParamRecencyFactor_DecayStaircase);
#else
    kMvtScoringParamRecencyFactor_Classic);
#endif  // BUILDFLAG(IS_ANDROID)

// The per-day decay factor for each visit, used by "decay" only.
inline constexpr base::FeatureParam<double> kMvtScoringParamDecayPerDay(
    &kMostVisitedTilesNewScoring,
    "decay_per_day",
    1.0);

// The cap to daily visit count for each segment, used by {"decay",
// "decay_staircase"}.
inline constexpr base::FeatureParam<int> kMvtScoringParamDailyVisitCountCap(
    &kMostVisitedTilesNewScoring,
    "daily_visit_count_cap",
#if BUILDFLAG(IS_ANDROID)
    10);
#else
    INT_MAX);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
