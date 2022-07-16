// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_RANKING_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_RANKING_CLUSTER_FINALIZER_H_

#include "components/history_clusters/core/cluster_finalizer.h"
#include "components/history_clusters/core/on_device_clustering_features.h"

namespace history_clusters {

// An object that contains the elements that make up a visit's score
// based on a Finch-controllable weighting.
class VisitScores {
 public:
  VisitScores() = default;
  ~VisitScores() = default;

  // Calculate the total visit score based on the individual scores and a set of
  // weightings between each. Note, this score is not between [0, 1] as
  // normalization will consider all visits within a cluster.
  float GetTotalScore() const {
    return visit_duration_score_ * features::VisitDurationRankingWeight() +
           foreground_duration_score_ *
               features::ForegroundDurationRankingWeight() +
           bookmark_score_ * features::BookmarkRankingWeight() +
           srp_score_ * features::SearchResultsPageRankingWeight();
  }

  void set_visit_duration_score(float score) { visit_duration_score_ = score; }

  void set_foreground_duration_score(float score) {
    foreground_duration_score_ = score;
  }

  void set_bookmarked() { bookmark_score_ = 1.0; }

  void set_is_srp() { srp_score_ = 1.0; }

 private:
  // The score for the duration associated with a visit.
  float visit_duration_score_ = 0.0;
  // The score for the foreground duration.
  float foreground_duration_score_ = 0.0;
  // The score for whether the visit was bookmarked.
  float bookmark_score_ = 0.0;
  // The score for whether the visit was on a search results page.
  float srp_score_ = 0.0;
};

// A cluster finalizer that scores visits based on visit duration.
class RankingClusterFinalizer : public ClusterFinalizer {
 public:
  RankingClusterFinalizer();
  ~RankingClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;

 private:
  // Calculates the scores for the visits within |cluster| based on
  // their total visit duration and updates |url_visit_scores|. Only
  // visits not in |duplicate_visit_ids| will be scored.
  void CalculateVisitDurationScores(
      history::Cluster& cluster,
      base::flat_map<history::VisitID, VisitScores>& url_visit_scores,
      const base::flat_set<history::VisitID>& duplicate_visit_ids);

  // Calculates the scores for the visits within |cluster| based on
  // their binary attributes and updates |url_visit_scores|. Only
  // visits not in |duplicate_visit_ids| will be scored.
  void CalculateVisitAttributeScoring(
      history::Cluster& cluster,
      base::flat_map<history::VisitID, VisitScores>& url_visit_scores,
      const base::flat_set<history::VisitID>& duplicate_visit_ids);

  // Computes the final scores for each visit based on the current
  // individual scores for each visit in |url_visit_scores|. Only
  // visits not in |duplicate_visit_ids| will be scored.
  void ComputeFinalVisitScores(
      history::Cluster& cluster,
      base::flat_map<history::VisitID, VisitScores>& url_visit_scores,
      const base::flat_set<history::VisitID>& duplicate_visit_ids);
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_RANKING_CLUSTER_FINALIZER_H_
