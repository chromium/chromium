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

// Whether the visit is considered a noisy visit (i.e. high engagement,
// non-SRP).
bool IsNoisyVisit(const history::ClusterVisit& visit);

// Appends the visits from |cluster1| to the visits in |cluster2|.
//
// |cluster2|'s visits will be cleared in this operation.'
void AppendClusterVisits(history::Cluster& cluster1,
                         history::Cluster& cluster2);

// Removes clusters without visits from |clusters|.
void RemoveEmptyClusters(std::vector<history::Cluster>* clusters);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_
