// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// Merges |duplicate_visit| into |canonical_visit|.
void MergeDuplicateVisitIntoCanonicalVisit(
    const history::ClusterVisit& duplicate_visit,
    history::ClusterVisit& canonical_visit);

// Calculates all the visits within |cluster| that are considered
// "duplicates" and stores their ids in |duplicate_visit_ids|.
base::flat_set<history::VisitID> CalculateAllDuplicateVisitsForCluster(
    const history::Cluster& cluster);

// Whether the visit is considered a noisy visit (i.e. high engagement,
// non-SRP).
bool IsNoisyVisit(const history::ClusterVisit& visit);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_UTIL_H_
