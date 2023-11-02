// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// Moves |duplicate_visit| into |canonical_visit|'s list of duplicate visits.
// |duplicate_visit| should be considered invalid after this call.
void MergeDuplicateVisitIntoCanonicalVisit(
    history::ClusterVisit&& duplicate_visit,
    history::ClusterVisit& canonical_visit);

// Enforces the reverse-chronological invariant of clusters, as well the
// by-score sorting of visits within clusters. Exposed for testing.
void SortClusters(std::vector<history::Cluster>* clusters);

// Whether the visit is considered a noisy visit (i.e. high engagement,
// non-SRP).
bool IsNoisyVisit(const history::ClusterVisit& visit);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_
