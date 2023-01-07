// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_VISIT_ANNOTATIONS_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CORE_TEST_VISIT_ANNOTATIONS_TEST_UTILS_H_

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

namespace history {

// Returns a Time that's `seconds` seconds after Windows epoch.
base::Time IntToTime(int seconds);

// Extract `VisitID`s from `AnnotatedVisit`s.
std::vector<VisitID> GetVisitIds(
    const std::vector<AnnotatedVisit>& annotated_visits);

// Extract `VisitID`s from `ClusterVisit`s.
std::vector<VisitID> GetVisitIds(
    const std::vector<ClusterVisit>& cluster_visits);

// Extract cluster IDs from `Cluster`s.
std::vector<int64_t> GetClusterIds(
    const std::vector<history::Cluster>& clusters);

// Construct a `Cluster` containing `visit_ids`.
Cluster CreateCluster(const std::vector<VisitID>& visit_ids);

// Like `CreateCluster()`, but creates multiple `Cluster`s.
std::vector<Cluster> CreateClusters(
    const std::vector<std::vector<int64_t>>& visit_ids_per_cluster);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_VISIT_ANNOTATIONS_TEST_UTILS_H_
