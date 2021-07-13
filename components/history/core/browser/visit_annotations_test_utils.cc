// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_test_utils.h"

namespace history {

base::Time IntToTime(int seconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromSeconds(seconds));
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
    const std::vector<AnnotatedVisitRow>& annotated_visits) {
  std::vector<VisitID> visit_ids;
  visit_ids.reserve(annotated_visits.size());
  base::ranges::transform(annotated_visits, std::back_inserter(visit_ids),
                          &AnnotatedVisitRow::visit_id);
  return visit_ids;
}

Cluster CreateCluster(const std::vector<VisitID>& visit_ids) {
  Cluster cluster;
  cluster.scored_annotated_visits.reserve(visit_ids.size());
  base::ranges::transform(visit_ids,
                          std::back_inserter(cluster.scored_annotated_visits),
                          [](const auto& visit_id) {
                            AnnotatedVisit annotated_visit;
                            annotated_visit.visit_row.visit_id = visit_id;
                            return ScoredAnnotatedVisit{annotated_visit};
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

ClusterRow CreateClusterRow(int64_t cluster_id,
                            const std::vector<int64_t>& visit_ids) {
  ClusterRow cluster{cluster_id};
  cluster.visit_ids = visit_ids;
  return cluster;
}

}  // namespace history
