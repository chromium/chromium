// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_RANKING_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_RANKING_CLUSTER_FINALIZER_H_

#include "components/history_clusters/core/cluster_finalizer.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_types.h"
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
    return visit_duration_score_ * GetConfig().visit_duration_ranking_weight +
           foreground_duration_score_ *
               GetConfig().foreground_duration_ranking_weight +
           bookmark_score_ * GetConfig().bookmark_ranking_weight +
           srp_score_ * GetConfig().search_results_page_ranking_weight +
           has_url_keyed_image_score_ *
               GetConfig().has_url_keyed_image_ranking_weight;
  }

  void set_visit_duration_score(float score) { visit_duration_score_ = score; }

  void set_foreground_duration_score(float score) {
    foreground_duration_score_ = score;
  }

  void set_bookmarked() { bookmark_score_ = 1.0; }

  void set_is_srp() { srp_score_ = 1.0; }

  void set_has_url_keyed_image() { has_url_keyed_image_score_ = 1.0; }

 private:
  // The score for the duration associated with a visit.
  float visit_duration_score_ = 0.0;
  // The score for the foreground duration.
  float foreground_duration_score_ = 0.0;
  // The score for whether the visit was bookmarked.
  float bookmark_score_ = 0.0;
  // The score for whether the visit was on a search results page.
  float srp_score_ = 0.0;
  // The score for whether the visit had a URL-keyed image.
  float has_url_keyed_image_score_ = 0.0;
};

// A cluster finalizer that scores visits based on visit duration.
class RankingClusterFinalizer : public ClusterFinalizer {
 public:
  explicit RankingClusterFinalizer(
      ClusteringRequestSource clustering_request_source);
  ~RankingClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;

 private:
  // Calculates the scores for the visits within |cluster| based on
  // their total visit duration and updates |url_visit_scores|.
  void CalculateVisitDurationScores(
      history::Cluster& cluster,
      base::flat_map<history::VisitID, VisitScores>& url_visit_scores);

  // Calculates the scores for the visits within |cluster| based on
  // their binary attributes and updates |url_visit_scores|.
  void CalculateVisitAttributeScoring(
      history::Cluster& cluster,
      base::flat_map<history::VisitID, VisitScores>& url_visit_scores);

  // Computes the final scores for each visit based on the current
  // individual scores for each visit in |url_visit_scores|.
  void ComputeFinalVisitScores(
      history::Cluster& cluster,
      base::flat_map<history::VisitID, VisitScores>& url_visit_scores);
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_RANKING_CLUSTER_FINALIZER_H_
