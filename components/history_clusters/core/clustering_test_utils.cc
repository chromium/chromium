// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/clustering_test_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {
namespace testing {

VisitResult::VisitResult(int visit_id,
                         float score,
                         const std::vector<VisitResult>& duplicate_visits,
                         std::u16string search_terms)
    : visit_id_(visit_id),
      score_(score),
      duplicate_visits_(duplicate_visits),
      search_terms_(search_terms) {}
VisitResult::VisitResult(const history::ClusterVisit& visit)
    : visit_id_(visit.annotated_visit.visit_row.visit_id),
      score_(visit.score),
      search_terms_(visit.annotated_visit.content_annotations.search_terms) {
  for (const auto& duplicate : visit.duplicate_visits) {
    duplicate_visits_.emplace_back(duplicate);
  }
}

VisitResult::VisitResult(const VisitResult& other) = default;
VisitResult::~VisitResult() = default;

std::string VisitResult::ToString() const {
  std::string duplicate_visits_string = "{}";
  if (!duplicate_visits_.empty()) {
    std::vector<std::string> duplicate_visits_strings;
    for (const auto& dup_visit : duplicate_visits_) {
      // In case of multiple layers of nesting, indent inner layers a bit more.
      std::string dup_visit_string = dup_visit.ToString();
      base::ReplaceChars(dup_visit_string, "\n", "\n  ", &dup_visit_string);
      duplicate_visits_strings.push_back(dup_visit_string);
    }
    duplicate_visits_string = base::StringPrintf(
        "{\n  %s\n}",
        base::JoinString(duplicate_visits_strings, "\n  ").c_str());
  }
  return base::StringPrintf(
      "VisitResult(visit_id=%d, score=%f, duplicate_visits=%s, "
      "search_terms=%s)",
      visit_id_, score_, duplicate_visits_string.c_str(),
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
         duplicate_visits_ == rhs.duplicate_visits_ &&
         search_terms_ == rhs.search_terms_;
}

history::AnnotatedVisit CreateDefaultAnnotatedVisit(int visit_id,
                                                    const GURL& url,
                                                    base::Time visit_time) {
  history::AnnotatedVisit visit;
  visit.visit_row.visit_id = visit_id;
  visit.visit_row.visit_time = visit_time;
  visit.url_row.set_url(url);
  visit.visit_row.visit_duration = base::Seconds(10);
  return visit;
}

history::ClusterVisit CreateClusterVisit(
    const history::AnnotatedVisit& annotated_visit,
    absl::optional<GURL> normalized_url,
    float score) {
  history::ClusterVisit cluster_visit;
  cluster_visit.annotated_visit = annotated_visit;
  cluster_visit.score = score;
  cluster_visit.normalized_url =
      normalized_url ? *normalized_url : annotated_visit.url_row.url();
  cluster_visit.url_for_deduping =
      ComputeURLForDeduping(cluster_visit.normalized_url);
  cluster_visit.url_for_display =
      ComputeURLForDisplay(cluster_visit.normalized_url);
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
