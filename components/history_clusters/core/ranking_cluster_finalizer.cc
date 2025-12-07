// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/ranking_cluster_finalizer.h"

#include "base/containers/adapters.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {

namespace {

// See https://en.wikipedia.org/wiki/Smoothstep.
float clamp(float x, float lowerlimit, float upperlimit) {
  if (x < lowerlimit)
    x = lowerlimit;
  if (x > upperlimit)
    x = upperlimit;
  return x;
}

// Maps |values| from the range specified by |low| and
// |high| into [0, 1].
// See https://en.wikipedia.org/wiki/Smoothstep.
float Smoothstep(float low, float high, float value) {
  DCHECK_NE(low, high);
  const float x = clamp((value - low) / (high - low), 0.0, 1.0);
  return x * x * (3 - 2 * x);
}

}  // namespace

RankingClusterFinalizer::RankingClusterFinalizer(
    ClusteringRequestSource clustering_request_source) {}

RankingClusterFinalizer::~RankingClusterFinalizer() = default;

void RankingClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<history::VisitID, VisitScores> url_visit_scores;

  CalculateVisitDurationScores(cluster, url_visit_scores);
  CalculateVisitAttributeScoring(cluster, url_visit_scores);
  ComputeFinalVisitScores(cluster, url_visit_scores);
}

void RankingClusterFinalizer::CalculateVisitAttributeScoring(
    history::Cluster& cluster,
    base::flat_map<history::VisitID, VisitScores>& url_visit_scores) {
  for (const auto& visit : cluster.visits) {
    auto it = url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);
    if (it == url_visit_scores.end()) {
      auto visit_score = VisitScores();
      url_visit_scores.insert(
          {visit.annotated_visit.visit_row.visit_id, visit_score});
    }
    it = url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);

    // Check if the visit is bookmarked.
    if (visit.annotated_visit.context_annotations.is_existing_bookmark ||
        visit.annotated_visit.context_annotations.is_new_bookmark) {
      it->second.set_bookmarked();
    }

    // Check if the visit contained a search query.
    if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
      it->second.set_is_srp();
    }

    // Check if the visit had a URL keyed image and it can be fetched when shown
    // in the UI.
    if (visit.annotated_visit.content_annotations.has_url_keyed_image &&
        visit.annotated_visit.visit_row.is_known_to_sync) {
      it->second.set_has_url_keyed_image();
    }

    // Additional/future attribute checks go here.
  }
}

void RankingClusterFinalizer::CalculateVisitDurationScores(
    history::Cluster& cluster,
    base::flat_map<history::VisitID, VisitScores>& url_visit_scores) {
  // |max_visit_duration| and |max_foreground_duration| must be > 0 for
  // reshaping between 0 and 1.
  base::TimeDelta max_visit_duration = base::Seconds(1);
  base::TimeDelta max_foreground_duration = base::Seconds(1);
  for (const auto& visit : cluster.visits) {
    // We don't care about checking for duplicate visits since the duration
    // should have been rolled up already.
    if (visit.annotated_visit.visit_row.visit_duration > max_visit_duration) {
      max_visit_duration = visit.annotated_visit.visit_row.visit_duration;
    }
    if (visit.annotated_visit.context_annotations.total_foreground_duration >
        max_foreground_duration) {
      max_foreground_duration =
          visit.annotated_visit.context_annotations.total_foreground_duration;
    }
  }
  for (const auto& visit : cluster.visits) {
    float visit_duration_score =
        Smoothstep(0.0f, max_visit_duration.InSecondsF(),
                   visit.annotated_visit.visit_row.visit_duration.InSecondsF());
    float foreground_duration_score =
        Smoothstep(0.0f, max_foreground_duration.InSecondsF(),
                   std::max(0.0, visit.annotated_visit.context_annotations
                                     .total_foreground_duration.InSecondsF()));
    auto visit_scores_it =
        url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);
    if (visit_scores_it == url_visit_scores.end()) {
      VisitScores visit_scores;
      visit_scores.set_visit_duration_score(visit_duration_score);
      visit_scores.set_foreground_duration_score(foreground_duration_score);
      url_visit_scores.insert(
          {visit.annotated_visit.visit_row.visit_id, std::move(visit_scores)});
    } else {
      visit_scores_it->second.set_visit_duration_score(visit_duration_score);
      visit_scores_it->second.set_foreground_duration_score(
          foreground_duration_score);
    }
  }
}

void RankingClusterFinalizer::ComputeFinalVisitScores(
    history::Cluster& cluster,
    base::flat_map<history::VisitID, VisitScores>& url_visit_scores) {
  float max_score = -1.0;
  for (auto& visit : cluster.visits) {
    // Determine the max score to use for normalizing all the scores.
    auto visit_scores_it =
        url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);
    if (visit_scores_it != url_visit_scores.end()) {
      if (IsShownVisitCandidate(visit)) {
        visit.score = visit_scores_it->second.GetTotalScore();
      } else {
        visit.score = 0.0;
      }
      if (visit.score > max_score) {
        max_score = visit.score;
      }
    }
  }
  if (max_score <= 0.0)
    return;

  // Now normalize the score by `max_score` so the values are all between 0
  // and 1.
  for (auto& visit : cluster.visits) {
    visit.score = visit.score / max_score;
  }
}

}  // namespace history_clusters
