// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_backend.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/cluster_finalizer.h"
#include "components/history_clusters/core/cluster_interaction_state_processor.h"
#include "components/history_clusters/core/cluster_processor.h"
#include "components/history_clusters/core/cluster_similarity_heuristics_processor.h"
#include "components/history_clusters/core/clusterer.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/content_visibility_cluster_finalizer.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/filter_cluster_processor.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/keyword_cluster_finalizer.h"
#include "components/history_clusters/core/label_cluster_finalizer.h"
#include "components/history_clusters/core/noisy_cluster_finalizer.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/history_clusters/core/ranking_cluster_finalizer.h"
#include "components/history_clusters/core/similar_visit_deduper_cluster_finalizer.h"
#include "components/history_clusters/core/single_visit_cluster_finalizer.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"
#include "components/url_formatter/url_formatter.h"

namespace history_clusters {

namespace {

void RecordBatchUpdateProcessingTime(base::TimeDelta time_delta) {
  base::UmaHistogramTimes(
      "History.Clusters.Backend.ProcessBatchOfVisits.ThreadTime", time_delta);
}

}  // namespace

OnDeviceClusteringBackend::OnDeviceClusteringBackend(
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : engagement_score_provider_(engagement_score_provider),
      user_visible_task_traits_(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
      continue_on_shutdown_user_visible_task_traits_(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      user_visible_priority_background_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(
              GetConfig().use_continue_on_shutdown
                  ? continue_on_shutdown_user_visible_task_traits_
                  : user_visible_task_traits_)),
      best_effort_task_traits_(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      continue_on_shutdown_best_effort_task_traits_(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      best_effort_priority_background_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(
              GetConfig().use_continue_on_shutdown
                  ? continue_on_shutdown_best_effort_task_traits_
                  : best_effort_task_traits_)),
      engagement_score_cache_last_refresh_timestamp_(base::TimeTicks::Now()),
      engagement_score_cache_(GetConfig().engagement_score_cache_size) {
  if (GetConfig().should_check_hosts_to_skip_clustering_for &&
      optimization_guide_decider) {
    optimization_guide_decider_ = optimization_guide_decider;
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::HISTORY_CLUSTERS});
  }
}

OnDeviceClusteringBackend::~OnDeviceClusteringBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OnDeviceClusteringBackend::GetClusters(
    ClusteringRequestSource clustering_request_source,
    ClustersCallback callback,
    std::vector<history::AnnotatedVisit> visits,
    bool requires_ui_and_triggerability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visits.empty()) {
    std::move(callback).Run({});
    return;
  }

  ProcessVisits(clustering_request_source, std::move(visits),
                requires_ui_and_triggerability, std::move(callback));
}

void OnDeviceClusteringBackend::GetClustersForUI(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    ClustersCallback callback,
    std::vector<history::Cluster> clusters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clusters.empty()) {
    std::move(callback).Run({});
    return;
  }

  DispatchGetClustersForUIToBackgroundThread(
      clustering_request_source, std::move(filter_params), std::move(callback),
      std::move(clusters));
}

void OnDeviceClusteringBackend::GetClusterTriggerability(
    ClustersCallback callback,
    std::vector<history::Cluster> clusters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clusters.empty()) {
    std::move(callback).Run({});
    return;
  }

  DispatchGetClusterTriggerabilityToBackgroundThread(std::move(callback),
                                                     std::move(clusters));
}

void OnDeviceClusteringBackend::ProcessVisits(
    ClusteringRequestSource clustering_request_source,
    std::vector<history::AnnotatedVisit> annotated_visits,
    bool requires_ui_and_triggerability,
    ClustersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ElapsedThreadTimer process_batch_timer;

  std::vector<history::ClusterVisit> cluster_visits;
  for (auto& visit : annotated_visits) {
    // Skip visits that should not be clustered.
    if (optimization_guide_decider_) {
      optimization_guide::OptimizationGuideDecision decision =
          optimization_guide_decider_->CanApplyOptimization(
              visit.url_row.url(), optimization_guide::proto::HISTORY_CLUSTERS,
              /*optimization_metadata=*/nullptr);
      if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
        continue;
      }
    }

    history::ClusterVisit cluster_visit;

    if (visit.content_annotations.search_normalized_url.is_empty()) {
      cluster_visit.normalized_url = visit.url_row.url();
      cluster_visit.url_for_deduping =
          ComputeURLForDeduping(cluster_visit.normalized_url);
    } else {
      cluster_visit.normalized_url =
          visit.content_annotations.search_normalized_url;
      // Search visits just use the `normalized_url` for deduping.
      cluster_visit.url_for_deduping = cluster_visit.normalized_url;
    }
    cluster_visit.url_for_display =
        ComputeURLForDisplay(cluster_visit.normalized_url);

    // The engagement score is only required for computing clusters for UI and
    // triggerability.
    if (requires_ui_and_triggerability) {
      const std::string& visit_host = cluster_visit.normalized_url.host();
      if (engagement_score_provider_) {
        auto it = engagement_score_cache_.Peek(visit_host);
        if (it != engagement_score_cache_.end()) {
          cluster_visit.engagement_score = it->second;
        } else {
          float score = engagement_score_provider_->GetScore(
              cluster_visit.normalized_url);
          engagement_score_cache_.Put(visit_host, score);
          cluster_visit.engagement_score = score;
        }
      }
    }

    cluster_visit.annotated_visit = std::move(visit);
    cluster_visits.push_back(std::move(cluster_visit));
  }

  RecordBatchUpdateProcessingTime(process_batch_timer.Elapsed());
  OnAllVisitsFinishedProcessing(
      clustering_request_source, std::move(cluster_visits),
      requires_ui_and_triggerability, std::move(callback));
}

void OnDeviceClusteringBackend::OnAllVisitsFinishedProcessing(
    ClusteringRequestSource clustering_request_source,
    std::vector<history::ClusterVisit> cluster_visits,
    bool requires_ui_and_triggerability,
    ClustersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Post the actual clustering work onto the thread pool, then reply on the
  // calling sequence. This is to prevent UI jank.

  base::OnceCallback<std::vector<history::Cluster>()> clustering_callback =
      base::BindOnce(
          &OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread,
          clustering_request_source, engagement_score_provider_ != nullptr,
          std::move(cluster_visits), requires_ui_and_triggerability);

  if (IsUIRequestSource(clustering_request_source)) {
    user_visible_priority_background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, std::move(clustering_callback), std::move(callback));
  } else {
    best_effort_priority_background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, std::move(clustering_callback), std::move(callback));
  }
}

void OnDeviceClusteringBackend::DispatchGetClustersForUIToBackgroundThread(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    ClustersCallback callback,
    std::vector<history::Cluster> clusters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  user_visible_priority_background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &OnDeviceClusteringBackend::GetClustersForUIOnBackgroundThread,
          clustering_request_source, base::OwnedRef(std::move(filter_params)),
          engagement_score_provider_ != nullptr, std::move(clusters),
          // Only Journeys has both non-prominent and prominent UI surfaces and
          // requires searchability.
          /*calculate_triggerability=*/clustering_request_source ==
              ClusteringRequestSource::kJourneysPage),
      std::move(callback));
}

void OnDeviceClusteringBackend::
    DispatchGetClusterTriggerabilityToBackgroundThread(
        ClustersCallback callback,
        std::vector<history::Cluster> clusters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  best_effort_priority_background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OnDeviceClusteringBackend::
                         GetClusterTriggerabilityOnBackgroundThread,
                     engagement_score_provider_ != nullptr, std::move(clusters),
                     /*from_ui=*/false),
      std::move(callback));
}

// static
std::vector<history::Cluster>
OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread(
    ClusteringRequestSource clustering_request_source,
    bool engagement_score_provider_is_valid,
    std::vector<history::ClusterVisit> visits,
    bool requires_ui_and_triggerability) {
  base::ElapsedThreadTimer compute_clusters_timer;

  // 1. Group visits into clusters.
  base::ElapsedThreadTimer context_clusterer_timer;
  std::unique_ptr<Clusterer> clusterer = std::make_unique<Clusterer>();
  std::vector<history::Cluster> clusters =
      clusterer->CreateInitialClustersFromVisits(std::move(visits));
  base::UmaHistogramTimes(
      "History.Clusters.Backend.ContextClusterer.ThreadTime",
      context_clusterer_timer.Elapsed());

  if (requires_ui_and_triggerability) {
    // 2. Determine how the clusters should be displayed.
    base::ElapsedThreadTimer compute_clusters_for_ui_timer;
    clusters = GetClustersForUIOnBackgroundThread(
        clustering_request_source, QueryClustersFilterParams(),
        engagement_score_provider_is_valid, std::move(clusters),
        /*calculate_triggerability=*/false);
    base::UmaHistogramTimes(
        "History.Clusters.Backend.ComputeClustersForUI.ThreadTime",
        compute_clusters_for_ui_timer.Elapsed());

    // 3. Determine the triggerability for the clusters.
    base::ElapsedThreadTimer cluster_triggerability_timer;
    clusters = GetClusterTriggerabilityOnBackgroundThread(
        engagement_score_provider_is_valid, std::move(clusters),
        /*from_ui=*/true);
    base::UmaHistogramTimes(
        "History.Clusters.Backend.ComputeClusterTriggerability2.ThreadTime",
        cluster_triggerability_timer.Elapsed());

    base::UmaHistogramTimes(
        "History.Clusters.Backend.ComputeClusters.ThreadTime",
        compute_clusters_timer.Elapsed());
  }

  return clusters;
}

// static
std::vector<history::Cluster>
OnDeviceClusteringBackend::GetClustersForUIOnBackgroundThread(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    bool engagement_score_provider_is_valid,
    std::vector<history::Cluster> clusters,
    bool calculate_triggerability) {
  // The cluster processors to be run.
  std::vector<std::unique_ptr<ClusterProcessor>> cluster_processors;
  cluster_processors.push_back(
      std::make_unique<ClusterInteractionStateProcessor>(filter_params));
  cluster_processors.push_back(
      std::make_unique<ClusterSimilarityHeuristicsProcessor>());

  // The cluster finalizers to run that affect the appearance of a cluster on a
  // UI surface.
  std::vector<std::unique_ptr<ClusterFinalizer>> cluster_finalizers;
  cluster_finalizers.push_back(
      std::make_unique<SimilarVisitDeduperClusterFinalizer>());
  cluster_finalizers.push_back(
      std::make_unique<RankingClusterFinalizer>(clustering_request_source));
  cluster_finalizers.push_back(std::make_unique<LabelClusterFinalizer>());

  // Process clusters.
  for (const auto& processor : cluster_processors) {
    processor->ProcessClusters(&clusters);
  }

  // Run finalizers that dedupe and score visits within a cluster and
  // log several metrics about the result.
  for (auto& cluster : clusters) {
    for (const auto& finalizer : cluster_finalizers) {
      finalizer->FinalizeCluster(cluster);
    }
  }

  // Apply any filtering after we've decided how to score clusters.
  std::unique_ptr<FilterClusterProcessor> filterer =
      std::make_unique<FilterClusterProcessor>(
          clustering_request_source, filter_params,
          engagement_score_provider_is_valid);
  filterer->ProcessClusters(&clusters);

  return calculate_triggerability
             ? GetClusterTriggerabilityOnBackgroundThread(
                   engagement_score_provider_is_valid, std::move(clusters),
                   /*from_ui=*/true)
             : clusters;
}

// static
std::vector<history::Cluster>
OnDeviceClusteringBackend::GetClusterTriggerabilityOnBackgroundThread(
    bool engagement_score_provider_is_valid,
    std::vector<history::Cluster> clusters,
    bool from_ui) {
  // The cluster finalizers to be run.
  std::vector<std::unique_ptr<ClusterFinalizer>> cluster_finalizers;

  if (!from_ui) {
    // Cluster finalizers to run that affect the appearance of a cluster on a UI
    // surface and are run here in case a user goes in and out of the new
    // context clustering path so that the user will have presentable clusters
    // when they swap back.
    // TODO(b/259466296): Remove this block once that path is fully launched.
    cluster_finalizers.push_back(
        std::make_unique<SimilarVisitDeduperClusterFinalizer>());
    cluster_finalizers.push_back(std::make_unique<RankingClusterFinalizer>(
        ClusteringRequestSource::kJourneysPage));
    cluster_finalizers.push_back(std::make_unique<LabelClusterFinalizer>());
  }

  // Cluster finalizers that affect the keywords for a cluster.
  cluster_finalizers.push_back(std::make_unique<KeywordClusterFinalizer>());

  // Cluster finalizers that affect the visibility of a cluster.
  cluster_finalizers.push_back(
      std::make_unique<ContentVisibilityClusterFinalizer>());
  cluster_finalizers.push_back(std::make_unique<SingleVisitClusterFinalizer>());
  if (engagement_score_provider_is_valid) {
    cluster_finalizers.push_back(std::make_unique<NoisyClusterFinalizer>());
  }

  for (auto& cluster : clusters) {
    // Initially set this default to true since the finalizers will only set the
    // visibility to false.
    cluster.should_show_on_prominent_ui_surfaces = true;
    for (const auto& finalizer : cluster_finalizers) {
      finalizer->FinalizeCluster(cluster);
    }
    cluster.triggerability_calculated = true;
  }

  return clusters;
}

}  // namespace history_clusters
