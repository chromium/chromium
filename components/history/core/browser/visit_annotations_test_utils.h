// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_TEST_UTILS_H_

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

namespace history {

// Returns a Time that's `seconds` seconds after Windows epoch.
base::Time IntToTime(int seconds);

// Extract `VisitID`s from `AnnotatedVisit`s.
std::vector<VisitID> GetVisitIds(
    const std::vector<AnnotatedVisit>& annotated_visits);

// Extract `VisitID`s from `AnnotatedVisitRow`s.
std::vector<VisitID> GetVisitIds(
    const std::vector<AnnotatedVisitRow>& annotated_visits);

// Construct a `Cluster` containing `visit_ids`.
Cluster CreateCluster(const std::vector<VisitID>& visit_ids);

// Like `CreateCluster()`, but creates multiple `Cluster`s.
std::vector<Cluster> CreateClusters(
    const std::vector<std::vector<int64_t>>& visit_ids_per_cluster);

// Construct a `ClusterRow` with the specified ids.
ClusterRow CreateClusterRow(int64_t cluster_id,
                            const std::vector<int64_t>& visit_ids);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_TEST_UTILS_H_
