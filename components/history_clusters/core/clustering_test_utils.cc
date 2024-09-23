// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/clustering_test_utils.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters::testing {

std::vector<history::VisitID> ExtractDuplicateVisitIds(
    std::vector<history::DuplicateClusterVisit> duplicate_visits) {
  std::vector<history::VisitID> ids;
  base::ranges::transform(duplicate_visits, std::back_inserter(ids),
                          [](const auto& visit) { return visit.visit_id; });
  return ids;
}

VisitResult::VisitResult(
    int visit_id,
    float score,
    const std::vector<history::DuplicateClusterVisit>& duplicate_visits,
    std::u16string search_terms)
    : visit_id_(visit_id),
      score_(score),
      duplicate_visits_(duplicate_visits),
      search_terms_(search_terms) {}
VisitResult::VisitResult(const history::ClusterVisit& visit)
    : visit_id_(visit.annotated_visit.visit_row.visit_id),
      score_(visit.score),
      duplicate_visits_(visit.duplicate_visits),
      search_terms_(visit.annotated_visit.content_annotations.search_terms) {}

VisitResult::VisitResult(const VisitResult& other) = default;
VisitResult::~VisitResult() = default;

std::string VisitResult::ToString() const {
  std::vector<std::string> duplicate_visits_strings;
  base::ranges::transform(
      duplicate_visits_, std::back_inserter(duplicate_visits_strings),
      [&](const auto& duplicate_visit) {
        return base::NumberToString(duplicate_visit.visit_id);
      });
  return base::StringPrintf(
      "VisitResult(visit_id=%d, score=%f, duplicate_visits=[%s], "
      "search_terms=%s)",
      visit_id_, score_,
      base::JoinString(duplicate_visits_strings, ",  ").c_str(),
      base::UTF16ToUTF8(search_terms_).c_str());
}

std::ostream& operator<<(std::ostream& os, const VisitResult& vr) {
  os << vr.ToString();
  return os;
}

bool VisitResult::operator==(const VisitResult& rhs) const {
  constexpr const double kScoreTolerance = 1e-6;
  return visit_id_ == rhs.visit_id_ &&
         abs(score_ - rhs.score_) <= kScoreTolerance &&
         ExtractDuplicateVisitIds(duplicate_visits_) ==
             ExtractDuplicateVisitIds(rhs.duplicate_visits_) &&
         search_terms_ == rhs.search_terms_;
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

history::AnnotatedVisit CreateDefaultAnnotatedVisit(int visit_id,
                                                    const GURL& url,
                                                    base::Time visit_time) {
  history::AnnotatedVisit visit;
  visit.visit_row.visit_id = visit_id;
  visit.visit_row.visit_time = visit_time;
  visit.url_row.set_url(url);
  visit.url_row.set_title(u"sometitle");
  visit.visit_row.visit_duration = base::Seconds(10);
  return visit;
}

history::ClusterVisit CreateClusterVisit(
    const history::AnnotatedVisit& annotated_visit,
    std::optional<GURL> normalized_url,
    float score,
    history::ClusterVisit::InteractionState interaction_state) {
  history::ClusterVisit cluster_visit;
  cluster_visit.annotated_visit = annotated_visit;
  cluster_visit.score = score;
  cluster_visit.normalized_url =
      normalized_url ? *normalized_url : annotated_visit.url_row.url();
  cluster_visit.url_for_deduping =
      ComputeURLForDeduping(cluster_visit.normalized_url);
  cluster_visit.url_for_display =
      ComputeURLForDisplay(cluster_visit.normalized_url);
  cluster_visit.interaction_state = interaction_state;
  return cluster_visit;
}

history::DuplicateClusterVisit ClusterVisitToDuplicateClusterVisit(
    history::ClusterVisit cluster_visit) {
  return {cluster_visit.annotated_visit.visit_row.visit_id,
          cluster_visit.annotated_visit.url_row.url(),
          cluster_visit.annotated_visit.visit_row.visit_time};
}

history::Cluster CreateCluster(
    std::vector<history::ClusterVisit>& cluster_visits) {
  history::Cluster cluster;
  cluster.visits = cluster_visits;
  return cluster;
}

}  // namespace history_clusters::testing
