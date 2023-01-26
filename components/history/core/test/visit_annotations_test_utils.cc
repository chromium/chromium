// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/visit_annotations_test_utils.h"

#include "components/history/core/browser/history_types.h"

namespace history {

base::Time IntToTime(int seconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(seconds));
}

std::vector<VisitID> GetVisitIds(
    const std::vector<AnnotatedVisit>& annotated_visits) {
  std::vector<VisitID> visit_ids;
  visit_ids.reserve(annotated_visits.size());
  base::ranges::transform(
      annotated_visits, std::back_inserter(visit_ids),
      [](const auto& visit_row) { return visit_row.visit_id; },
      &AnnotatedVisit::visit_row);
  return visit_ids;
}

std::vector<VisitID> GetVisitIds(
    const std::vector<ClusterVisit>& cluster_visits) {
  std::vector<VisitID> visit_ids;
  visit_ids.reserve(cluster_visits.size());
  base::ranges::transform(
      cluster_visits, std::back_inserter(visit_ids),
      [](const auto& cluster_visit) {
        return cluster_visit.annotated_visit.visit_row.visit_id;
      });
  return visit_ids;
}

std::vector<int64_t> GetClusterIds(
    const std::vector<history::Cluster>& clusters) {
  std::vector<int64_t> cluster_ids;
  cluster_ids.reserve(clusters.size());
  base::ranges::transform(
      clusters, std::back_inserter(cluster_ids),
      [](const auto& cluster) { return cluster.cluster_id; });
  return cluster_ids;
}

Cluster CreateCluster(const std::vector<VisitID>& visit_ids) {
  Cluster cluster;
  cluster.visits.reserve(visit_ids.size());
  base::ranges::transform(visit_ids, std::back_inserter(cluster.visits),
                          [](const auto& visit_id) {
                            ClusterVisit visit;
                            visit.annotated_visit.visit_row.visit_id = visit_id;
                            visit.score = 1;
                            return visit;
                          });
  return cluster;
}

std::vector<Cluster> CreateClusters(
    const std::vector<std::vector<int64_t>>& visit_ids_per_cluster) {
  std::vector<Cluster> clusters;
  clusters.reserve(visit_ids_per_cluster.size());
  base::ranges::transform(visit_ids_per_cluster, std::back_inserter(clusters),
                          &CreateCluster);
  return clusters;
}

}  // namespace history
