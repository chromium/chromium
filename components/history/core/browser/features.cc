// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/features.h"

#include "build/build_config.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/sync/base/features.h"

namespace history {
namespace {
constexpr auto is_android = !!BUILDFLAG(IS_ANDROID);
constexpr auto kOrganicRepeatableQueriesDefaultValue =
    is_android ? base::FEATURE_ENABLED_BY_DEFAULT
               : base::FEATURE_DISABLED_BY_DEFAULT;

// Specifies the scaling behavior, i.e. whether the relevance scales of the
// top sites and repeatable queries should be first aligned.
// The default behavior is to mix the two lists as is.
constexpr bool kScaleRepeatableQueriesScoresDefaultValue =
    BUILDFLAG(IS_ANDROID) ? true : false;

// Defines the maximum number of repeatable queries that can be shown.
// The default behavior is having no limit, i.e., the number of the tiles.
constexpr int kMaxNumRepeatableQueriesDefaultValue =
    BUILDFLAG(IS_ANDROID) ? 4 : kTopSitesNumber;
}  // namespace

// If enabled, the most repeated queries from the user browsing history are
// shown in the Most Visited tiles.
BASE_FEATURE(kOrganicRepeatableQueries,
             "OrganicRepeatableQueries",
             kOrganicRepeatableQueriesDefaultValue);

// The maximum number of repeatable queries to show in the Most Visited tiles.
const base::FeatureParam<int> kMaxNumRepeatableQueries(
    &kOrganicRepeatableQueries,
    "MaxNumRepeatableQueries",
    kMaxNumRepeatableQueriesDefaultValue);

// Whether the scores for the repeatable queries and the most visited sites
// should first be scaled to an equivalent range before mixing.
const base::FeatureParam<bool> kScaleRepeatableQueriesScores(
    &kOrganicRepeatableQueries,
    "ScaleRepeatableQueriesScores",
    kScaleRepeatableQueriesScoresDefaultValue);

// Whether a repeatable query should precede a most visited site with equal
// score. The default behavior is for the sites to precede the queries.
// Used for tie-breaking, especially when kScaleRepeatableQueriesScores is true.
const base::FeatureParam<bool> kPrivilegeRepeatableQueries(
    &kOrganicRepeatableQueries,
    "PrivilegeRepeatableQueries",
    false);

// Whether duplicative visits should be ignored for the repeatable queries. A
// duplicative visit is a visit to the same search term in an interval smaller
// than kAutocompleteDuplicateVisitIntervalThreshold.
const base::FeatureParam<bool> kRepeatableQueriesIgnoreDuplicateVisits(
    &kOrganicRepeatableQueries,
    "RepeatableQueriesIgnoreDuplicateVisits",
    is_android);

// The maximum number of days since the last visit (in days) in order for a
// search query to considered as a repeatable query.
const base::FeatureParam<int> kRepeatableQueriesMaxAgeDays(
    &kOrganicRepeatableQueries,
    "RepeatableQueriesMaxAgeDays",
    90);

// The minimum number of visits for a search query to considered as a
// repeatable query.
const base::FeatureParam<int> kRepeatableQueriesMinVisitCount(
    &kOrganicRepeatableQueries,
    "RepeatableQueriesMinVisitCount",
    is_android ? 6 : 1);

BASE_FEATURE(kPopulateVisitedLinkDatabase,
             "PopulateVisitedLinkDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMostVisitedTilesNewScoring,
             "MostVisitedTilesNewScoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kMvtScoringParamRecencyFactor[] = "recency_factor";
const char kMvtScoringParamDailyVisitCountCap[] = "daily_visit_count_cap";

const char kMvtScoringParamRecencyFactor_Default[] = "default";
const char kMvtScoringParamRecencyFactor_DecayStaircase[] = "decay_staircase";

}  // namespace history
