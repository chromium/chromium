// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/features.h"

#include "build/build_config.h"
#include "components/history/core/browser/top_sites_constants.h"

namespace history {
namespace {
constexpr auto is_android = !!BUILDFLAG(IS_ANDROID);
constexpr auto kOrganicRepeatableQueriesDefaultValue =
    base::FEATURE_DISABLED_BY_DEFAULT;

// Specifies the scaling behavior, i.e. whether the relevance scales of the
// top sites and repeatable queries should be first aligned.
// The default behavior is to mix the two lists as is.
constexpr bool kScaleRepeatableQueriesScoresDefaultValue = is_android;

// Defines the maximum number of repeatable queries that can be shown.
// The default behavior is having no limit, i.e., the number of the tiles.
constexpr int kMaxNumRepeatableQueriesDefaultValue =
    BUILDFLAG(IS_ANDROID) ? 4 : kTopSitesNumber;
}  // namespace

// If enabled, the most repeated queries from the user browsing history are
// shown in the Most Visited tiles.
BASE_FEATURE(kOrganicRepeatableQueries, kOrganicRepeatableQueriesDefaultValue);

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

BASE_FEATURE(kPopulateVisitedLinkDatabase, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, makes links `:visited` even when the visit results in an HTTP
// response code of 404.
BASE_FEATURE(kVisitedLinksOn404, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, uses new scoring function for Most Visited Tiles computation.
BASE_FEATURE(kMostVisitedTilesNewScoring,
             is_android ? base::FEATURE_ENABLED_BY_DEFAULT
                        : base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kMvtScoringParamRecencyFactor_Classic[] = "default";
constexpr char kMvtScoringParamRecencyFactor_Decay[] = "decay";
constexpr char kMvtScoringParamRecencyFactor_DecayStaircase[] =
    "decay_staircase";

// The name of the recency factor strategy to use for MVT computation.
constexpr base::FeatureParam<std::string> kMvtScoringParamRecencyFactor{
    &kMostVisitedTilesNewScoring, "recency_factor",
#if BUILDFLAG(IS_ANDROID)
    kMvtScoringParamRecencyFactor_DecayStaircase};
#else
    kMvtScoringParamRecencyFactor_Classic};
#endif  // BUILDFLAG(IS_ANDROID)

// The per-day decay factor for each visit, used by "decay" only.
constexpr base::FeatureParam<double> kMvtScoringParamDecayPerDay{
    &kMostVisitedTilesNewScoring, "decay_per_day", 1.0};

// The cap to daily visit count for each segment, used by {"decay",
// "decay_staircase"}.
constexpr base::FeatureParam<int> kMvtScoringParamDailyVisitCountCap{
    &kMostVisitedTilesNewScoring, "daily_visit_count_cap",
#if BUILDFLAG(IS_ANDROID)
    10};
#else
    INT_MAX};
#endif  // BUILDFLAG(IS_ANDROID)

// If enabled, very old history databases that cannot be migrated are deleted.
BASE_FEATURE(kRazeOldHistoryDatabase,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Enables Milestone 2 of History-Actor integration, this includes hiding
// actor-initiated visits from non-primary sources (Omnibox, MVT) and updating
// the deduplication logic of actor visits.
BASE_FEATURE(kBrowsingHistoryActorIntegrationM2,
             base::FeatureState::FEATURE_ENABLED_BY_DEFAULT);

// Enables Milestone 3 of History-Actor integration, this includes improvements
// in history entry grouping and filtering.
BASE_FEATURE(kBrowsingHistoryActorIntegrationM3,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// Enables improved chrome://history de-duplication logic, this includes
// grouping entries by hostname and title per day.
BASE_FEATURE(kBrowsingHistorySimilarVisitsGrouping,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// Whether Browsing History Actor Integration M2 or any dependent feature is
// enabled.
bool IsBrowsingHistoryActorIntegrationM2Enabled() {
  return base::FeatureList::IsEnabled(kBrowsingHistoryActorIntegrationM2) ||
         base::FeatureList::IsEnabled(kBrowsingHistoryActorIntegrationM3);
}

// Whether Browsing History Actor Integration M3 is enabled.
bool IsBrowsingHistoryActorIntegrationM3Enabled() {
  return base::FeatureList::IsEnabled(kBrowsingHistoryActorIntegrationM3);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// If enabled, the BrowsingHistoryService will start querying only local data,
// and switch to querying remote data only once all local data has been
// exhausted.
BASE_FEATURE(kHistoryQueryOnlyLocalFirst, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the WebHistoryService will use a new API for querying browsing
// history (https://footprints-pa.googleapis.com/...) instead of the old and
// deprecated one (https://history.google.com/history/api/...).
BASE_FEATURE(kWebHistoryUseNewApi, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace history
