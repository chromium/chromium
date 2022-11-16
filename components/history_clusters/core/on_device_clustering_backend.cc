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
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/category_cluster_finalizer.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/content_annotations_cluster_processor.h"
#include "components/history_clusters/core/content_visibility_cluster_finalizer.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/full_membership_cluster_processor.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/keyword_cluster_finalizer.h"
#include "components/history_clusters/core/label_cluster_finalizer.h"
#include "components/history_clusters/core/metrics_cluster_finalizer.h"
#include "components/history_clusters/core/noisy_cluster_finalizer.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/history_clusters/core/ranking_cluster_finalizer.h"
#include "components/history_clusters/core/similar_visit_deduper_cluster_finalizer.h"
#include "components/history_clusters/core/single_domain_cluster_finalizer.h"
#include "components/history_clusters/core/single_visit_cluster_finalizer.h"
#include "components/optimization_guide/core/batch_entity_metadata_task.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
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
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
    optimization_guide::NewOptimizationGuideDecider* optimization_guide_decider,
    base::flat_set<std::string> mid_blocklist)
    : entity_metadata_provider_(entity_metadata_provider),
      engagement_score_provider_(engagement_score_provider),
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
      engagement_score_cache_(GetConfig().engagement_score_cache_size),
      mid_blocklist_(mid_blocklist) {
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
    std::vector<history::AnnotatedVisit> visits) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visits.empty()) {
    std::move(callback).Run({});
    return;
  }

  // Just start clustering without getting entity metadata if we don't have a
  // provider to translate the entities.
  if (!entity_metadata_provider_) {
    OnBatchEntityMetadataRetrieved(
        clustering_request_source, /*completed_task=*/nullptr, visits,
        /*entity_metadata_start=*/absl::nullopt, std::move(callback),
        /*entity_metadata_map=*/{});
    return;
  }

  base::ElapsedThreadTimer entity_id_gathering_timer;

  // Figure out what entity IDs we need to fetch metadata for.
  base::flat_set<std::string> entity_ids;
  for (const auto& visit : visits) {
    for (const auto& entity :
         visit.content_annotations.model_annotations.entities) {
      // Remove entities that are on the keyword blocklist.
      if (mid_blocklist_.find(entity.id) != mid_blocklist_.end()) {
        continue;
      }
      // Only put the entity IDs in if they exceed a certain threshold.
      if (entity.weight < GetConfig().entity_relevance_threshold) {
        continue;
      }
      entity_ids.insert(entity.id);
    }
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.EntityIdGathering.ThreadTime",
      entity_id_gathering_timer.Elapsed());

  // Don't bother with getting entity metadata if there's nothing to get
  // metadata for.
  if (entity_ids.empty()) {
    OnBatchEntityMetadataRetrieved(
        clustering_request_source, /*completed_task=*/nullptr,
        std::move(visits),
        /*entity_metadata_start=*/absl::nullopt, std::move(callback),
        /*entity_metadata_map=*/{});
    return;
  }

  base::UmaHistogramCounts1000("History.Clusters.Backend.BatchEntityLookupSize",
                               entity_ids.size());

  // Fetch the metadata for the entity ID present in |visits|.
  auto batch_entity_metadata_task =
      std::make_unique<optimization_guide::BatchEntityMetadataTask>(
          entity_metadata_provider_, entity_ids);
  auto* batch_entity_metadata_task_ptr = batch_entity_metadata_task.get();
  in_flight_batch_entity_metadata_tasks_.insert(
      std::move(batch_entity_metadata_task));
  batch_entity_metadata_task_ptr->Execute(
      base::BindOnce(&OnDeviceClusteringBackend::OnBatchEntityMetadataRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), clustering_request_source,
                     batch_entity_metadata_task_ptr, std::move(visits),
                     base::TimeTicks::Now(), std::move(callback)));
}

void OnDeviceClusteringBackend::OnBatchEntityMetadataRetrieved(
    ClusteringRequestSource clustering_request_source,
    optimization_guide::BatchEntityMetadataTask* completed_task,
    std::vector<history::AnnotatedVisit> annotated_visits,
    absl::optional<base::TimeTicks> entity_metadata_start,
    ClustersCallback callback,
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (entity_metadata_start) {
    base::UmaHistogramTimes(
        "History.Clusters.Backend.BatchEntityLookupLatency2",
        base::TimeTicks::Now() - *entity_metadata_start);
  }

  if (base::TimeTicks::Now() >
      (engagement_score_cache_last_refresh_timestamp_ +
       GetConfig().engagement_score_cache_refresh_duration)) {
    engagement_score_cache_.Clear();
    engagement_score_cache_last_refresh_timestamp_ = base::TimeTicks::Now();
  }

  ProcessVisits(clustering_request_source, completed_task,
                std::move(annotated_visits), entity_metadata_start,
                std::move(callback), entity_metadata_map);
}

void OnDeviceClusteringBackend::ProcessVisits(
    ClusteringRequestSource clustering_request_source,
    optimization_guide::BatchEntityMetadataTask* completed_task,
    std::vector<history::AnnotatedVisit> annotated_visits,
    absl::optional<base::TimeTicks> entity_metadata_start,
    ClustersCallback callback,
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ElapsedThreadTimer process_batch_timer;

  std::vector<history::ClusterVisit> cluster_visits;
  base::flat_map<std::string, optimization_guide::EntityMetadata>
      entity_id_to_metadata_map;
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

    const std::string& visit_host = cluster_visit.normalized_url.host();
    if (engagement_score_provider_) {
      auto it = engagement_score_cache_.Peek(visit_host);
      if (it != engagement_score_cache_.end()) {
        cluster_visit.engagement_score = it->second;
      } else {
        float score =
            engagement_score_provider_->GetScore(cluster_visit.normalized_url);
        engagement_score_cache_.Put(visit_host, score);
        cluster_visit.engagement_score = score;
      }
    }

    // Rewrite the entities for the visit, but only if it is possible that we
    // had additional metadata for it.
    if (entity_metadata_provider_) {
      base::flat_map<std::string, int> inserted_categories;
      auto entity_it =
          visit.content_annotations.model_annotations.entities.begin();
      while (entity_it !=
             visit.content_annotations.model_annotations.entities.end()) {
        auto entity_metadata_it = entity_metadata_map.find(entity_it->id);
        if (entity_metadata_it == entity_metadata_map.end() ||
            entity_it->weight < GetConfig().entity_relevance_threshold) {
          entity_it =
              visit.content_annotations.model_annotations.entities.erase(
                  entity_it);
          continue;
        }

        entity_id_to_metadata_map[entity_it->id] = entity_metadata_it->second;
        entity_it++;
      }
    }

    cluster_visit.annotated_visit = std::move(visit);
    cluster_visits.push_back(std::move(cluster_visit));
  }

  RecordBatchUpdateProcessingTime(process_batch_timer.Elapsed());
  OnAllVisitsFinishedProcessing(
      clustering_request_source, completed_task, std::move(cluster_visits),
      std::move(entity_id_to_metadata_map), std::move(callback));
}

void OnDeviceClusteringBackend::OnAllVisitsFinishedProcessing(
    ClusteringRequestSource clustering_request_source,
    optimization_guide::BatchEntityMetadataTask* completed_task,
    std::vector<history::ClusterVisit> cluster_visits,
    base::flat_map<std::string, optimization_guide::EntityMetadata>
        entity_id_to_metadata_map,
    ClustersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Mark the task as completed, as we are done with it and have moved
  // everything adequately at this point.
  if (completed_task) {
    auto it = in_flight_batch_entity_metadata_tasks_.find(completed_task);
    if (it != in_flight_batch_entity_metadata_tasks_.end()) {
      in_flight_batch_entity_metadata_tasks_.erase(it);
    }
  }

  // Post the actual clustering work onto the thread pool, then reply on the
  // calling sequence. This is to prevent UI jank.

  base::OnceCallback<std::vector<history::Cluster>()> clustering_callback =
      base::BindOnce(
          &OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread,
          clustering_request_source, engagement_score_provider_ != nullptr,
          std::move(cluster_visits), std::move(entity_id_to_metadata_map));

  switch (clustering_request_source) {
    case ClusteringRequestSource::kJourneysPage:
      user_visible_priority_background_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE, std::move(clustering_callback), std::move(callback));
      break;
    case ClusteringRequestSource::kKeywordCacheGeneration:
      best_effort_priority_background_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE, std::move(clustering_callback), std::move(callback));
      break;
  }
}

// static
std::vector<history::Cluster>
OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread(
    ClusteringRequestSource clustering_request_source,
    bool engagement_score_provider_is_valid,
    std::vector<history::ClusterVisit> visits,
    base::flat_map<std::string, optimization_guide::EntityMetadata>
        entity_id_to_entity_metadata_map) {
  base::ElapsedThreadTimer cluster_visits_timer;

  // TODO(crbug.com/1260145): All of these objects are "stateless" between
  // requests for clusters. If there needs to be shared state, the entire
  // backend needs to be refactored to separate these objects from the UI and
  // background thread.
  std::unique_ptr<Clusterer> clusterer = std::make_unique<Clusterer>();

  // The cluster processors to be run.
  std::vector<std::unique_ptr<ClusterProcessor>> cluster_processors;

  // The cluster finalizers to be run.
  std::vector<std::unique_ptr<ClusterFinalizer>> cluster_finalizers;

  cluster_processors.push_back(
      std::make_unique<FullMembershipClusterProcessor>());
  if (GetConfig().content_clustering_enabled) {
    cluster_processors.push_back(
        std::make_unique<ContentAnnotationsClusterProcessor>(
            &entity_id_to_entity_metadata_map));
  }

  cluster_finalizers.push_back(
      std::make_unique<ContentVisibilityClusterFinalizer>());
  cluster_finalizers.push_back(
      std::make_unique<SimilarVisitDeduperClusterFinalizer>());
  cluster_finalizers.push_back(std::make_unique<RankingClusterFinalizer>());
  if (GetConfig().should_hide_single_visit_clusters_on_prominent_ui_surfaces) {
    cluster_finalizers.push_back(
        std::make_unique<SingleVisitClusterFinalizer>());
  }
  if (GetConfig().should_hide_single_domain_clusters_on_prominent_ui_surfaces) {
    cluster_finalizers.push_back(
        std::make_unique<SingleDomainClusterFinalizer>());
  }
  // Add feature to turn on/off site engagement score filter.
  if (engagement_score_provider_is_valid &&
      GetConfig().should_filter_noisy_clusters) {
    cluster_finalizers.push_back(std::make_unique<NoisyClusterFinalizer>());
  }
  if (GetConfig().should_use_categories_to_filter_on_prominent_ui_surfaces) {
    cluster_finalizers.push_back(std::make_unique<CategoryClusterFinalizer>());
  }
  cluster_finalizers.push_back(std::make_unique<KeywordClusterFinalizer>(
      &entity_id_to_entity_metadata_map));
  if (GetConfig().should_label_clusters) {
    cluster_finalizers.push_back(std::make_unique<LabelClusterFinalizer>(
        &entity_id_to_entity_metadata_map));
  }
  if (clustering_request_source ==
      ClusteringRequestSource::kKeywordCacheGeneration) {
    // Only log metrics for whole clusters when it is a query-less state to get
    // a better lay of the land. We estimate this by using the request source.
    cluster_finalizers.push_back(std::make_unique<MetricsClusterFinalizer>());
  }

  // Group visits into clusters.
  base::ElapsedThreadTimer clusterer_timer;
  std::vector<history::Cluster> clusters =
      clusterer->CreateInitialClustersFromVisits(std::move(visits));
  base::UmaHistogramTimes(
      "History.Clusters.Backend.ContextClusterer.ThreadTime",
      clusterer_timer.Elapsed());

  // Process clusters.
  base::ElapsedThreadTimer cluster_processors_timer;
  for (const auto& processor : cluster_processors) {
    processor->ProcessClusters(&clusters);
  }
  base::UmaHistogramTimes(
      "History.Clusters.Backend.ClusterProcessors.ThreadTime",
      cluster_processors_timer.Elapsed());

  // Run finalizers that dedupe and score visits within a cluster and
  // log several metrics about the result.
  base::ElapsedThreadTimer cluster_finalizers_timer;
  std::vector<int> keyword_sizes;
  std::vector<int> visits_in_clusters;
  keyword_sizes.reserve(clusters.size());
  visits_in_clusters.reserve(clusters.size());
  for (auto& cluster : clusters) {
    for (const auto& finalizer : cluster_finalizers) {
      finalizer->FinalizeCluster(cluster);
    }
    if (GetConfig()
            .should_show_all_clusters_unconditionally_on_prominent_ui_surfaces) {
      // Override the `should_show_on_prominent_ui_surfaces` bit if set by
      // config.
      cluster.should_show_on_prominent_ui_surfaces = true;
    }
    visits_in_clusters.emplace_back(cluster.visits.size());
    keyword_sizes.emplace_back(cluster.keyword_to_data_map.size());
  }
  base::UmaHistogramTimes(
      "History.Clusters.Backend.ClusterFinalizers.ThreadTime",
      cluster_finalizers_timer.Elapsed());

  if (!visits_in_clusters.empty()) {
    // We check for empty to ensure the below code doesn't crash, but
    // realistically this vector should never be empty.
    base::UmaHistogramCounts1000("History.Clusters.Backend.ClusterSize.Min",
                                 *std::min_element(visits_in_clusters.begin(),
                                                   visits_in_clusters.end()));
    base::UmaHistogramCounts1000("History.Clusters.Backend.ClusterSize.Max",
                                 *std::max_element(visits_in_clusters.begin(),
                                                   visits_in_clusters.end()));
  }
  if (!keyword_sizes.empty()) {
    // We check for empty to ensure the below code doesn't crash, but
    // realistically this vector should never be empty.
    base::UmaHistogramCounts100(
        "History.Clusters.Backend.NumKeywordsPerCluster.Min",
        *std::min_element(keyword_sizes.begin(), keyword_sizes.end()));
    base::UmaHistogramCounts100(
        "History.Clusters.Backend.NumKeywordsPerCluster.Max",
        *std::max_element(keyword_sizes.begin(), keyword_sizes.end()));
  }

  base::UmaHistogramTimes("History.Clusters.Backend.ComputeClusters.ThreadTime",
                          cluster_visits_timer.Elapsed());

  return clusters;
}

}  // namespace history_clusters
