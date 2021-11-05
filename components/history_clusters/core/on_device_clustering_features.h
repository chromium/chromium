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

}  // namespace features
}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_
