// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/history_clusters/core/clustering_backend.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace site_engagement {
class SiteEngagementScoreProvider;
}  // namespace site_engagement

namespace history_clusters {

// A clustering backend that clusters visits on device.
class OnDeviceClusteringBackend : public ClusteringBackend {
 public:
  OnDeviceClusteringBackend(
      site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  ~OnDeviceClusteringBackend() override;

  // ClusteringBackend:
  void GetClusters(ClusteringRequestSource clustering_request_source,
                   ClustersCallback callback,
                   std::vector<history::AnnotatedVisit> visits,
                   bool requires_ui_and_triggerability) override;
  void GetClustersForUI(ClusteringRequestSource clustering_request_source,
                        QueryClustersFilterParams filter_params,
                        ClustersCallback callback,
                        std::vector<history::Cluster> clusters) override;
  void GetClusterTriggerability(
      ClustersCallback callback,
      std::vector<history::Cluster> clusters) override;

 private:
  // Adds additional metadata that might be used for clustering or
  // Journeys to each visit in `annotated_visits`, such as site engagement.
  void ProcessVisits(ClusteringRequestSource clustering_request_source,
                     std::vector<history::AnnotatedVisit> annotated_visits,
                     bool requires_ui_and_triggerability,
                     ClustersCallback callback);

  // Called when all visits have been processed.
  void OnAllVisitsFinishedProcessing(
      ClusteringRequestSource clustering_request_source,
      std::vector<history::ClusterVisit> cluster_visits,
      bool requires_ui_and_triggerability,
      ClustersCallback callback);

  // Dispatches call to `GetClustersForUIOnBackgroundThread()` from the main
  // thread.
  void DispatchGetClustersForUIToBackgroundThread(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      ClustersCallback callback,
      std::vector<history::Cluster> clusters);

  // Dispatches call to `GetClusterTriggerabilityOnBackgroundThread()` from the
  // main thread.
  void DispatchGetClusterTriggerabilityToBackgroundThread(
      ClustersCallback callback,
      std::vector<history::Cluster> clusters);

  // Clusters `visits` on background thread.
  static std::vector<history::Cluster> ClusterVisitsOnBackgroundThread(
      ClusteringRequestSource clustering_request_source,
      bool engagement_score_provider_is_valid,
      std::vector<history::ClusterVisit> visits,
      bool requires_ui_and_triggerability);

  // Gets the displayable variant of `clusters` that will be shown on the UI
  // surface associated with `clustering_request_source` on background thread.
  // This will merge similar clusters, rank visits within the cluster, as well
  // as provide a label. If `calculate_triggerability` is set to true, it will
  // also determine the updated triggerability metadata for the new clusters.
  //
  // TODO(sophiechang): Remove `calculate_triggerability` field once the new
  //   path is fully migrated to. It is only separated out for metrics that are
  //   recorded by the fuller `ClusterVisitsOnBackgroundThread()`.
  static std::vector<history::Cluster> GetClustersForUIOnBackgroundThread(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      bool engagement_score_provider_is_valid,
      std::vector<history::Cluster> clusters,
      bool calculate_triggerability);

  // Gets the metadata required for cluster triggerability (e.g. keywords,
  // whether to show on prominent UI surfaces) for each cluster in `clusters` on
  // background thread.
  //
  // TODO(sophiechang): Remove `from_ui` field once the new path is fully
  //   migrated to. It is only separated so that users can flip between
  //   experiments somewhat seamlessly.
  static std::vector<history::Cluster>
  GetClusterTriggerabilityOnBackgroundThread(
      bool engagement_score_provider_is_valid,
      std::vector<history::Cluster> clusters,
      bool from_ui);

  // Used to get engagement scores. Can be null during tests. Not owned. Must
  // outlive `this`.
  raw_ptr<site_engagement::SiteEngagementScoreProvider>
      engagement_score_provider_ = nullptr;

  // Used to get page load metadata. Can be null if feature not enabled. Not
  // owned. Must outlive `this`.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  // The task runners to run clustering passes on.
  // `user_visible_priority_background_task_runner_` should be used iff
  // clustering is blocking content on a page that user is actively looking at.
  const base::TaskTraits user_visible_task_traits_;
  const base::TaskTraits continue_on_shutdown_user_visible_task_traits_;
  scoped_refptr<base::SequencedTaskRunner>
      user_visible_priority_background_task_runner_;
  const base::TaskTraits best_effort_task_traits_;
  const base::TaskTraits continue_on_shutdown_best_effort_task_traits_;
  scoped_refptr<base::SequencedTaskRunner>
      best_effort_priority_background_task_runner_;

  // Last time `engagement_score_cache_` was refreshed.
  base::TimeTicks engagement_score_cache_last_refresh_timestamp_;
  // URL host to score mapping.
  base::HashingLRUCache<std::string, float> engagement_score_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceClusteringBackend> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_
