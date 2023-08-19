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
#include "components/history_clusters/core/content_annotations_cluster_processor.h"
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
#include "components/optimization_guide/core/batch_entity_metadata_task.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"
#include "components/url_formatter/url_formatter.h"

namespace history_clusters {

namespace {

void RecordEntityIdGatheringTime(base::TimeDelta time_delta) {
  base::UmaHistogramTimes(
      "History.Clusters.Backend.EntityIdGathering.ThreadTime", time_delta);
}

void RecordBatchUpdateProcessingTime(base::TimeDelta time_delta) {
  base::UmaHistogramTimes(
      "History.Clusters.Backend.ProcessBatchOfVisits.ThreadTime", time_delta);
}

using EntityMetadataProcessedCallback = base::OnceCallback<void(
    std::vector<history::Cluster>,
    base::flat_map<std::string, optimization_guide::EntityMetadata>)>;
// Processes `entity_metadata_map` and rewrites `clusters` with valid entity
// metadata. Invokes `callback` with rewritten clusters synchronously when
// done.
void ProcessEntityMetadata(
    EntityMetadataProcessedCallback callback,
    std::vector<history::Cluster> clusters,
    base::flat_map<std::string, optimization_guide::EntityMetadata>
        entity_metadata_map) {
  // Prune out entities that do not meet the relevance threshold or are not in
  // the most updated mapping.
  base::flat_map<std::string, optimization_guide::EntityMetadata>
      processed_entity_metadata_map;
  for (auto& cluster : clusters) {
    for (auto& visit : cluster.visits) {
      auto entity_it = visit.annotated_visit.content_annotations
                           .model_annotations.entities.begin();
      while (entity_it != visit.annotated_visit.content_annotations
                              .model_annotations.entities.end()) {
        auto entity_metadata_it = entity_metadata_map.find(entity_it->id);
        if (entity_metadata_it == entity_metadata_map.end() ||
            entity_it->weight < GetConfig().entity_relevance_threshold) {
          entity_it = visit.annotated_visit.content_annotations
                          .model_annotations.entities.erase(entity_it);
          continue;
        }

        processed_entity_metadata_map[entity_it->id] =
            entity_metadata_it->second;
        entity_it++;
      }
    }
  }

  std::move(callback).Run(std::move(clusters),
                          std::move(processed_entity_metadata_map));
}

}  // namespace

OnDeviceClusteringBackend::OnDeviceClusteringBackend(
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
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
    std::vector<history::AnnotatedVisit> visits,
    bool requires_ui_and_triggerability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visits.empty()) {
    std::move(callback).Run({});
    return;
  }

  base::flat_set<std::string> entity_ids;
  if (requires_ui_and_triggerability) {
    base::ElapsedThreadTimer entity_id_gathering_timer;
    for (const auto& visit : visits) {
      for (const auto& entity :
           visit.content_annotations.model_annotations.entities) {
        if (ShouldRetrieveEntityMetadataForEntity(entity)) {
          entity_ids.insert(entity.id);
        }
      }
    }
    RecordEntityIdGatheringTime(entity_id_gathering_timer.Elapsed());
  }

  RetrieveBatchEntityMetadata(
      std::move(entity_ids),
      base::BindOnce(&OnDeviceClusteringBackend::ProcessVisits,
                     weak_ptr_factory_.GetWeakPtr(), clustering_request_source,
                     std::move(visits), requires_ui_and_triggerability,
                     std::move(callback)));
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

  base::flat_set<std::string> entity_ids = GetEntityIdsForClusters(clusters);
  EntityMetadataProcessedCallback entity_metadata_processed_callback =
      base::BindOnce(&OnDeviceClusteringBackend::
                         DispatchGetClustersForUIToBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), clustering_request_source,
                     std::move(filter_params), std::move(callback));
  RetrieveBatchEntityMetadata(
      std::move(entity_ids),
      base::BindOnce(&ProcessEntityMetadata,
                     std::move(entity_metadata_processed_callback),
                     std::move(clusters)));
}

void OnDeviceClusteringBackend::GetClusterTriggerability(
    ClustersCallback callback,
    std::vector<history::Cluster> clusters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clusters.empty()) {
    std::move(callback).Run({});
    return;
  }

  base::flat_set<std::string> entity_ids = GetEntityIdsForClusters(clusters);
  EntityMetadataProcessedCallback entity_metadata_processed_callback =
      base::BindOnce(&OnDeviceClusteringBackend::
                         DispatchGetClusterTriggerabilityToBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  RetrieveBatchEntityMetadata(
      std::move(entity_ids),
      base::BindOnce(&ProcessEntityMetadata,
                     std::move(entity_metadata_processed_callback),
                     std::move(clusters)));
}

base::flat_set<std::string> OnDeviceClusteringBackend::GetEntityIdsForClusters(
    const std::vector<history::Cluster>& clusters) {
  base::ElapsedThreadTimer entity_id_gathering_timer;
  base::flat_set<std::string> entity_ids;
  for (const auto& cluster : clusters) {
    for (const auto& visit : cluster.visits) {
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
        if (ShouldRetrieveEntityMetadataForEntity(entity)) {
          entity_ids.insert(entity.id);
        }
      }
    }
  }
  RecordEntityIdGatheringTime(entity_id_gathering_timer.Elapsed());

  return entity_ids;
}

bool OnDeviceClusteringBackend::ShouldRetrieveEntityMetadataForEntity(
    const history::VisitContentModelAnnotations::Category& entity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Remove entities that are on the keyword blocklist.
  if (mid_blocklist_.find(entity.id) != mid_blocklist_.end()) {
    return false;
  }
  // Only put the entity IDs in if they exceed a certain threshold.
  if (entity.weight < GetConfig().entity_relevance_threshold) {
    return false;
  }
  return true;
}

void OnDeviceClusteringBackend::RetrieveBatchEntityMetadata(
    base::flat_set<std::string> entity_ids,
    EntityRetrievedCallback entity_retrieved_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!entity_metadata_provider_ || entity_ids.empty()) {
    OnBatchEntityMetadataRetrieved(
        /*completed_task=*/nullptr,
        /*entity_metadata_start=*/absl::nullopt,
        std::move(entity_retrieved_callback),
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
  batch_entity_metadata_task_ptr->Execute(base::BindOnce(
      &OnDeviceClusteringBackend::OnBatchEntityMetadataRetrieved,
      weak_ptr_factory_.GetWeakPtr(), batch_entity_metadata_task_ptr,
      base::TimeTicks::Now(), std::move(entity_retrieved_callback)));
}

void OnDeviceClusteringBackend::OnBatchEntityMetadataRetrieved(
    optimization_guide::BatchEntityMetadataTask* completed_task,
    absl::optional<base::TimeTicks> entity_metadata_start,
    EntityRetrievedCallback entity_retrieved_callback,
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (entity_metadata_start) {
    base::UmaHistogramTimes(
        "History.Clusters.Backend.BatchEntityLookupLatency2",
        base::TimeTicks::Now() - *entity_metadata_start);
  }

  std::move(entity_retrieved_callback).Run(entity_metadata_map);

  // Mark the task as completed, as we are done with it and have moved
  // everything adequately at this point.
  if (completed_task) {
    auto it = in_flight_batch_entity_metadata_tasks_.find(completed_task);
    if (it != in_flight_batch_entity_metadata_tasks_.end()) {
      in_flight_batch_entity_metadata_tasks_.erase(it);
    }
  }
}

void OnDeviceClusteringBackend::ProcessVisits(
    ClusteringRequestSource clustering_request_source,
    std::vector<history::AnnotatedVisit> annotated_visits,
    bool requires_ui_and_triggerability,
    ClustersCallback callback,
    base::flat_map<std::string, optimization_guide::EntityMetadata>
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

    // The engagement score and entity metadata are only required for computing
    // clusters for UI and triggerability.
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

      // Rewrite the entities for the visit, but only if it is possible that we
      // had additional metadata for it.
      if (entity_metadata_provider_) {
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
    }

    cluster_visit.annotated_visit = std::move(visit);
    cluster_visits.push_back(std::move(cluster_visit));
  }

  RecordBatchUpdateProcessingTime(process_batch_timer.Elapsed());
  OnAllVisitsFinishedProcessing(
      clustering_request_source, std::move(cluster_visits),
      requires_ui_and_triggerability, std::move(entity_id_to_metadata_map),
      std::move(callback));
}

void OnDeviceClusteringBackend::OnAllVisitsFinishedProcessing(
    ClusteringRequestSource clustering_request_source,
    std::vector<history::ClusterVisit> cluster_visits,
    bool requires_ui_and_triggerability,
    base::flat_map<std::string, optimization_guide::EntityMetadata>
        entity_id_to_metadata_map,
    ClustersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Post the actual clustering work onto the thread pool, then reply on the
  // calling sequence. This is to prevent UI jank.

  base::OnceCallback<std::vector<history::Cluster>()> clustering_callback =
      base::BindOnce(
          &OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread,
          clustering_request_source, engagement_score_provider_ != nullptr,
          std::move(cluster_visits), requires_ui_and_triggerability,
          base::OwnedRef(std::move(entity_id_to_metadata_map)));

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
    std::vector<history::Cluster> clusters,
    base::flat_map<std::string, optimization_guide::EntityMetadata>
        entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  user_visible_priority_background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &OnDeviceClusteringBackend::GetClustersForUIOnBackgroundThread,
          clustering_request_source, base::OwnedRef(std::move(filter_params)),
          engagement_score_provider_ != nullptr, std::move(clusters),
          base::OwnedRef(std::move(entity_metadata_map)),
          // Only Journeys has both non-prominent and prominent UI surfaces and
          // requires searchability.
          /*calculate_triggerability=*/clustering_request_source ==
              ClusteringRequestSource::kJourneysPage),
      std::move(callback));
}

void OnDeviceClusteringBackend::
    DispatchGetClusterTriggerabilityToBackgroundThread(
        ClustersCallback callback,
        std::vector<history::Cluster> clusters,
        base::flat_map<std::string, optimization_guide::EntityMetadata>
            entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  best_effort_priority_background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OnDeviceClusteringBackend::
                         GetClusterTriggerabilityOnBackgroundThread,
                     engagement_score_provider_ != nullptr, std::move(clusters),
                     base::OwnedRef(std::move(entity_metadata_map)),
                     /*from_ui=*/false),
      std::move(callback));
}

// static
std::vector<history::Cluster>
OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread(
    ClusteringRequestSource clustering_request_source,
    bool engagement_score_provider_is_valid,
    std::vector<history::ClusterVisit> visits,
    bool requires_ui_and_triggerability,
    base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_id_to_entity_metadata_map) {
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
        entity_id_to_entity_metadata_map,
        /*calculate_triggerability=*/false);
    base::UmaHistogramTimes(
        "History.Clusters.Backend.ComputeClustersForUI.ThreadTime",
        compute_clusters_for_ui_timer.Elapsed());

    // 3. Determine the triggerability for the clusters.
    base::ElapsedThreadTimer cluster_triggerability_timer;
    clusters = GetClusterTriggerabilityOnBackgroundThread(
        engagement_score_provider_is_valid, std::move(clusters),
        entity_id_to_entity_metadata_map, /*from_ui=*/true);
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
    base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_id_to_entity_metadata_map,
    bool calculate_triggerability) {
  // The cluster processors to be run.
  std::vector<std::unique_ptr<ClusterProcessor>> cluster_processors;
  cluster_processors.push_back(
      std::make_unique<ClusterInteractionStateProcessor>(filter_params));
  cluster_processors.push_back(
      std::make_unique<ClusterSimilarityHeuristicsProcessor>());
  if (filter_params.group_clusters_by_content) {
    cluster_processors.push_back(
        std::make_unique<ContentAnnotationsClusterProcessor>(
            &entity_id_to_entity_metadata_map));
  }

  // The cluster finalizers to run that affect the appearance of a cluster on a
  // UI surface.
  std::vector<std::unique_ptr<ClusterFinalizer>> cluster_finalizers;
  cluster_finalizers.push_back(
      std::make_unique<SimilarVisitDeduperClusterFinalizer>());
  cluster_finalizers.push_back(
      std::make_unique<RankingClusterFinalizer>(clustering_request_source));
  cluster_finalizers.push_back(std::make_unique<LabelClusterFinalizer>(
      &entity_id_to_entity_metadata_map));

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
                   entity_id_to_entity_metadata_map, /*from_ui=*/true)
             : clusters;
}

// static
std::vector<history::Cluster>
OnDeviceClusteringBackend::GetClusterTriggerabilityOnBackgroundThread(
    bool engagement_score_provider_is_valid,
    std::vector<history::Cluster> clusters,
    base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_id_to_entity_metadata_map,
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
    cluster_finalizers.push_back(std::make_unique<LabelClusterFinalizer>(
        &entity_id_to_entity_metadata_map));
  }

  // Cluster finalizers that affect the keywords for a cluster.
  cluster_finalizers.push_back(std::make_unique<KeywordClusterFinalizer>(
      &entity_id_to_entity_metadata_map));

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
