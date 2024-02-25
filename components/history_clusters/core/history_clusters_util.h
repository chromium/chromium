// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history_clusters {

enum class ClusteringRequestSource;

// Computes a simplified GURL for deduping purposes only. The resulting GURL may
// not be valid or navigable, and is only intended for History Cluster deduping.
//
// Note, this is NOT meant to be applied to Search Result Page URLs. Those
// should be separately canonicalized by TemplateURLService and not sent here.
GURL ComputeURLForDeduping(const GURL& url);

// Returns a string suitable for display in the Journeys UI from the normalized
// visit URL. Displays the host and the path. Set `trim_after_host` to true to
// also remove the path, query, and ref.
std::u16string ComputeURLForDisplay(const GURL& normalized_url,
                                    bool trim_after_host = false);

// Stable sorts visits according to score, then reverse-chronologically.
void StableSortVisits(std::vector<history::ClusterVisit>& visits);

// Erases all clusters that don't match `query`. Also may re-score the visits
// within matching clusters.
//
// If `query` is an empty string, leaves `clusters` unmodified.
void ApplySearchQuery(const std::string& query,
                      std::vector<history::Cluster>& clusters);

// If `query` is empty, erases all non-prominent clusters.
//
// If `query` is non-empty, we assume that the user is searching for something,
// so we only cull duplicate occurrences of single-visit non-prominent clusters.
// The set of single-visit clusters we've already seen is tracked by
// `seen_single_visit_cluster_urls` and this function updates that set.
void CullNonProminentOrDuplicateClusters(
    std::string query,
    std::vector<history::Cluster>& clusters,
    std::set<GURL>* seen_single_visit_cluster_urls);

// Removes low scoring visits and clusters depending on `is_zero_query_state`.
void CullVisitsThatShouldBeHidden(std::vector<history::Cluster>& clusters,
                                  bool is_zero_query_state);

// Coalesces the related searches off of individual visits and places them at
// the cluster level with numerical limits defined by flags.
void CoalesceRelatedSearches(std::vector<history::Cluster>& clusters);

// Enforces the reverse-chronological invariant of clusters, as well the
// by-score sorting of visits within clusters. Exposed for testing.
void SortClusters(std::vector<history::Cluster>* clusters);

// Whether to use navigation context clusters from persistence.
bool ShouldUseNavigationContextClustersFromPersistence();

// Whether the transition is user-visible.
bool IsTransitionUserVisible(int32_t transition);

// Returns the histogram name slice for the clustering request source.
std::string GetHistogramNameSliceForRequestSource(
    ClusteringRequestSource source);

// Returns whether `source` is a UI source.
bool IsUIRequestSource(ClusteringRequestSource source);

// Returns whether |visit| should be shown in the UI.
bool IsShownVisitCandidate(const history::ClusterVisit& visit);

// Returns whether `visit` could possibly be classified as one of the categories
// in `categories`.
bool IsVisitInCategories(const history::ClusterVisit& visit,
                         const base::flat_set<std::string>& categories);

// Returns whether `cluster` could possibly be classified as one of the
// categories in `categories`.
bool IsClusterInCategories(const history::Cluster& cluster,
                           const base::flat_set<std::string>& categories);

// Return the set of category ids associated with a given cluster.
std::set<std::string> GetClusterCategoryIds(const history::Cluster& cluster);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_
