// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONTEXT_CLUSTERER_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONTEXT_CLUSTERER_HISTORY_SERVICE_OBSERVER_H_

#include <map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service_observer.h"

class TemplateURLService;

namespace history {
class HistoryService;
}  // namespace history

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace site_engagement {
class SiteEngagementScoreProvider;
}  // namespace site_engagement

namespace history_clusters {

// Information required for determine pending cluster.
struct InProgressCluster {
  InProgressCluster();
  ~InProgressCluster();
  InProgressCluster(const InProgressCluster&);

  // The visit IDs that were added to this in-progress cluster.
  std::vector<history::VisitID> visit_ids;
  // The normalized URLs that are a part of this in-progress cluster.
  base::flat_set<std::string> visit_urls;
  // The visit time of the last visit added to this in-progress cluster.
  base::Time last_visit_time;
  // The search terms associated with this in-progress cluster. It will only be
  // set once if a search visit is part of this in-progress cluster.
  std::u16string search_terms;
  // The corresponding cluster ID in the persisted database.
  int64_t persisted_cluster_id = 0;
  // The vector of visits that have not been persisted yet. Note that each entry
  // only contains the minimum required to persist a cluster visit.
  std::vector<history::ClusterVisit> unpersisted_visits;
  // Whether this cluster was meant to be cleaned up but is being held for
  // persistence.
  bool cleaned_up = false;
};

struct CachedEngagementScore {
  CachedEngagementScore(float score, base::Time expiry_time);
  ~CachedEngagementScore();
  CachedEngagementScore(const CachedEngagementScore&);

  // The site engagement score.
  float score = 0.0;

  // The time that this cache entry expires.
  base::Time expiry_time;
};

// A HistoryServiceObserver responsible for grouping visits into clusters.
//
// This groups visits together based on their navigation graph (previous visit,
// forward-back, reload, etc.). It is responsible for determining when a cluster
// is closed based on navigational factors as well as a timer that regularly
// cleans up in-progress clusters.
//
// Still todo are to persist the visits to the clusters database as they come
// in. After this is fully rolled out, there should not be any concept of
// "incomplete" visits and that the on-device clustering backend will receive a
// vector of clusters to combine or add metadata to.
class ContextClustererHistoryServiceObserver
    : public history::HistoryServiceObserver {
 public:
  ContextClustererHistoryServiceObserver(
      history::HistoryService* history_service,
      TemplateURLService* template_url_service,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      site_engagement::SiteEngagementScoreProvider* engagement_score_provider);
  ~ContextClustererHistoryServiceObserver() override;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& visit_row) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class ContextClustererHistoryServiceObserverTest;

  // Cleans up clusters that have not been interacted with for awhile.
  void CleanUpClusters();

  // Finalizes the cluster with index, `cluster_id`.
  void FinalizeCluster(int64_t cluster_id);

  // Callback invoked when the History Service returns the cluster ID
  // (`persisted_cluster_id`) to use for `cluster_id`.
  void OnPersistedClusterIdReceived(base::TimeTicks start_time,
                                    int64_t cluster_id,
                                    int64_t persisted_cluster_id);

  // Creates a cluster visit from `normalized_url` and `visit_row`.
  history::ClusterVisit CreateClusterVisit(const std::string& normalized_url,
                                           bool is_search_normalized_url,
                                           const history::VisitRow& visit_row);

  // Gets the site engagement score for `normalized_url`.
  float GetEngagementScore(const GURL& normalized_url);

  // Overrides `clock_` for testing.
  void OverrideClockForTesting(const base::Clock* clock);

  // Returns the number of clusters created since the start of the session.
  int64_t num_clusters_created() const { return cluster_id_counter_; }

  // Mapping from cluster ID to the contents of the in-progress cluster.
  std::map<int64_t, InProgressCluster> in_progress_clusters_;

  // Mapping from visit ID to the in-progress cluster ID it belongs to.
  std::map<history::VisitID, int64_t> visit_id_to_cluster_map_;

  // Mapping from normalized URL spec to the in-progress cluster ID it belongs
  // to.
  std::map<std::string, int64_t> visit_url_to_cluster_map_;

  // A running counter that is used to index the in-progress clusters.
  int64_t cluster_id_counter_ = 0;

  // Used to invoke `CleanUpClusters()` periodically.
  base::RepeatingTimer clean_up_clusters_repeating_timer_;

  // The History Service that `this` observers. Should never be null.
  raw_ptr<history::HistoryService> history_service_;

  // Used to determine if a visit is a search visit. Should only be null for
  // tests.
  raw_ptr<TemplateURLService> template_url_service_;

  // Used to determine whether to include a visit in any cluster. Can be null,
  // but is guaranteed to outlive `this`.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  // URL host to score mapping.
  base::HashingLRUCache<std::string, CachedEngagementScore>
      engagement_score_cache_;

  // Used to determine how "interesting" a visit is likely to be to a user.
  // Should only be null for tests.
  raw_ptr<site_engagement::SiteEngagementScoreProvider>
      engagement_score_provider_;

  // Used to schedule the clean up of clusters.
  raw_ptr<const base::Clock> clock_;

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<ContextClustererHistoryServiceObserver>
      weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONTEXT_CLUSTERER_HISTORY_SERVICE_OBSERVER_H_
