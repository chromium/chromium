// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history_clusters {
namespace features {

// Params & helpers functions

// Enables configuring the on-device clustering backend.
extern const base::Feature kOnDeviceClustering;

// Uses an in-memory cache that stores engagement score.
extern const base::Feature kUseEngagementScoreCache;

// Splits clustering task into smaller batches.
extern const base::Feature kSplitClusteringTasksToSmallerBatches;

// Returns the maximum duration between navigations that
// a visit can be considered for the same cluster.
base::TimeDelta ClusterNavigationTimeCutoff();

// Returns whether content clustering is enabled and
// should be performed by the clustering backend.
bool ContentClusteringEnabled();

// Returns the weight that should be placed on entity similarity for determining
// if two clusters are similar enough to be combined into one.
float ContentClusteringEntitySimilarityWeight();

// Returns the weight that should be placed on category similarity for
// determining if two clusters are similar enough to be combined into one.
float ContentClusteringCategorySimilarityWeight();

// Returns the similarity threshold, between 0 and 1, used to determine if
// two clusters are similar enough to be combined into
// a single cluster.
float ContentClusteringSimilarityThreshold();

// Returns the threshold for which we should mark a cluster as being able to
// show on prominent UI surfaces.
float ContentVisibilityThreshold();

// Returns the min page topics model version to honor the visibility score for.
int64_t GetMinPageTopicsModelVersionToUseContentVisibilityFrom();

// Whether to hide single-visit clusters on prominent UI surfaces.
bool ShouldHideSingleVisitClustersOnProminentUISurfaces();

// Whether to collapse visits within a cluster that will show on the UI in the
// same way.
bool ShouldDedupeSimilarVisits();

// Whether to filter clusters that are noisy from the UI. This will
// heuristically remove clusters that are unlikely to be "interesting".
bool ShouldFilterNoisyClusters();

// Returns the threshold used to determine if a cluster, and its visits, has
// too high site engagement to be likely useful.
float NoisyClusterVisitEngagementThreshold();

// Returns the number of visits considered interesting, or not noisy, required
// to prevent the cluster from being filtered out (i.e., marked as not visible
// on the zero state UI).
size_t NumberInterestingVisitsFilterThreshold();

// Returns the weight to use for the visit duration when ranking visits within a
// cluster. Will always be greater than or equal to 0.
float VisitDurationRankingWeight();

// Returns the weight to use for the foreground duration when ranking visits
// within a cluster. Will always be greater than or equal to 0.
float ForegroundDurationRankingWeight();

// Returns the weight to use for bookmarked visits when ranking visits within
// a cluster. Will always be greater than or equal to 0.
float BookmarkRankingWeight();

// Returns the weight to use for visits that are search results pages ranking
// visits within a cluster. Will always be greater than or equal to 0.
float SearchResultsPageRankingWeight();

// Returns the weight to use for visits that have page titles ranking visits
// within a cluster. Will always be greater than or equal to 0.
float HasPageTitleRankingWeight();

// Returns true if content clustering should use the intersection similarity
// score. Note, if this is used, the threshold used for clustering by content
// score should be < .5 (see ContentClusteringSimilarityThreshold above) or the
// weightings between entity and category content similarity scores should be
// adjusted.
bool ContentClusterOnIntersectionSimilarity();

// Returns the threshold, in terms of the number of overlapping keywords, to use
// when clustering based on intersection score.
int ClusterIntersectionThreshold();

// Whether to include category names in the keywords for a cluster.
bool ShouldIncludeCategoriesInKeywords();

// Whether to exclude keywords from visits that may be considered "noisy" to the
// user (i.e. highly engaged, non-SRP).
bool ShouldExcludeKeywordsFromNoisyVisits();

// Returns the default batch size for annotating visits when clustering.
size_t GetClusteringTasksBatchSize();

// Whether to split the clusters when a search visit is encountered.
bool ShouldSplitClustersAtSearchVisits();

}  // namespace features
}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_
