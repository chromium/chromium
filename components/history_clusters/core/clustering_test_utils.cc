// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/clustering_test_utils.h"

#include "base/strings/utf_string_conversions.h"

namespace history_clusters {
namespace testing {

VisitResult::VisitResult(
    int visit_id,
    float score,
    const std::vector<history::VisitID>& duplicate_visit_ids,
    bool is_search_visit)
    : visit_id_(visit_id),
      score_(score),
      duplicate_visit_ids_(duplicate_visit_ids),
      is_search_visit_(is_search_visit) {}
VisitResult::VisitResult(const history::ClusterVisit& visit)
    : visit_id_(visit.annotated_visit.visit_row.visit_id),
      score_(visit.score),
      duplicate_visit_ids_(visit.duplicate_visit_ids),
      is_search_visit_(visit.is_search_visit) {}

VisitResult::VisitResult(const VisitResult& other) = default;
VisitResult::~VisitResult() = default;

bool VisitResult::operator==(const VisitResult& rhs) const {
  return visit_id_ == rhs.visit_id_ && score_ == rhs.score_ &&
         duplicate_visit_ids_ == rhs.duplicate_visit_ids_ &&
         is_search_visit_ == rhs.is_search_visit_;
}

history::AnnotatedVisit CreateDefaultAnnotatedVisit(int visit_id,
                                                    const GURL& url) {
  history::AnnotatedVisit visit;
  visit.visit_row.visit_id = visit_id;
  visit.url_row.set_url(url);
  visit.visit_row.visit_duration = base::Seconds(10);
  return visit;
}

history::ClusterVisit CreateClusterVisit(
    const history::AnnotatedVisit& annotated_visit,
    absl::optional<GURL> normalized_url) {
  history::ClusterVisit cluster_visit;
  cluster_visit.annotated_visit = annotated_visit;
  cluster_visit.score = 1.0;
  cluster_visit.normalized_url =
      normalized_url ? *normalized_url : annotated_visit.url_row.url();
  return cluster_visit;
}

std::vector<std::vector<testing::VisitResult>> ToVisitResults(
    const std::vector<history::Cluster>& clusters) {
  std::vector<std::vector<testing::VisitResult>> clusters_results;
  for (const auto& cluster : clusters) {
    std::vector<testing::VisitResult> visit_results;
    for (const auto& visit : cluster.visits) {
      visit_results.push_back(testing::VisitResult(visit));
    }
    clusters_results.push_back(visit_results);
  }
  return clusters_results;
}

}  // namespace testing
}  // namespace history_clusters
