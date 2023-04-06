// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/context_clusterer_history_service_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/search_engines/template_url_service.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"

namespace history_clusters {

namespace {

// Returns whether `visit` should be added to `cluster`.
bool ShouldAddVisitToCluster(const history::VisitRow& new_visit,
                             const std::u16string& search_terms,
                             const InProgressCluster& in_progress_cluster) {
  if ((new_visit.visit_time - in_progress_cluster.last_visit_time) >
      GetConfig().cluster_navigation_time_cutoff) {
    return false;
  }

  if (!search_terms.empty()) {
    return search_terms == in_progress_cluster.search_terms;
  }

  return true;
}

enum class ContextClustererDbLatencyType {
  kReserveNextClusterId = 0,
  kAddVisitsToCluster = 1,
  kUpdateClusterVisit = 2,

  // If adding more values here, please add the new stage to the variants under
  // the `History.Clusters.ContextClusterer.DbLatency` in
  // history/histograms.xml.
};

std::string DbLatencyHistogramSuffix(
    ContextClustererDbLatencyType db_latency_type) {
  switch (db_latency_type) {
    case ContextClustererDbLatencyType::kReserveNextClusterId:
      return ".ReserveNextClusterId";
    case ContextClustererDbLatencyType::kAddVisitsToCluster:
      return ".AddVisitsToCluster";
    case ContextClustererDbLatencyType::kUpdateClusterVisit:
      return ".UpdateClusterVisit";
  }
}

void LogDbLatencyHistogram(ContextClustererDbLatencyType db_latency_type,
                           base::TimeTicks start_time) {
  base::UmaHistogramTimes("History.Clusters.ContextClusterer.DbLatency" +
                              DbLatencyHistogramSuffix(db_latency_type),
                          base::TimeTicks::Now() - start_time);
}

enum class VisitProcessingStage {
  kUrlVisited = 0,
  kUrlsDeleted = 1,
  kCleanUpTimer = 2,

  // If adding more values here, please add the new stage to the variants under
  // the `History.Clusters.ContextClusterer.VisitProcessingLatency` in
  // history/histograms.xml.
};

std::string VisitProcessingStageHistogramSuffix(
    VisitProcessingStage visit_processing_stage) {
  switch (visit_processing_stage) {
    case VisitProcessingStage::kUrlVisited:
      return ".UrlVisited";
    case VisitProcessingStage::kUrlsDeleted:
      return ".UrlsDeleted";
    case VisitProcessingStage::kCleanUpTimer:
      return ".CleanUpTimer";
  };
}

class ScopedVisitProcessingTimer {
 public:
  explicit ScopedVisitProcessingTimer(
      VisitProcessingStage visit_processing_stage)
      : visit_processing_stage_(visit_processing_stage) {}
  ~ScopedVisitProcessingTimer() {
    base::UmaHistogramTimes(
        "History.Clusters.ContextClusterer.VisitProcessingLatency" +
            VisitProcessingStageHistogramSuffix(visit_processing_stage_),
        elapsed_timer_.Elapsed());
  }

 private:
  VisitProcessingStage visit_processing_stage_;

  base::ElapsedTimer elapsed_timer_;
};

}  // namespace

InProgressCluster::InProgressCluster() = default;
InProgressCluster::~InProgressCluster() = default;
InProgressCluster::InProgressCluster(const InProgressCluster&) = default;

CachedEngagementScore::CachedEngagementScore(float score,
                                             base::Time expiry_time)
    : score(score), expiry_time(expiry_time) {}
CachedEngagementScore::~CachedEngagementScore() = default;
CachedEngagementScore::CachedEngagementScore(const CachedEngagementScore&) =
    default;

ContextClustererHistoryServiceObserver::ContextClustererHistoryServiceObserver(
    history::HistoryService* history_service,
    TemplateURLService* template_url_service,
    optimization_guide::NewOptimizationGuideDecider* optimization_guide_decider,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider)
    : history_service_(history_service),
      template_url_service_(template_url_service),
      optimization_guide_decider_(optimization_guide_decider),
      engagement_score_cache_(GetConfig().engagement_score_cache_size),
      engagement_score_provider_(engagement_score_provider),
      clock_(base::DefaultClock::GetInstance()) {
  if (history_service_) {
    history_service_observation_.Observe(history_service_);
  }

  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::HISTORY_CLUSTERS});
  }
  clean_up_clusters_repeating_timer_.Start(
      FROM_HERE, GetConfig().context_clustering_clean_up_duration, this,
      &ContextClustererHistoryServiceObserver::CleanUpClusters);
}
ContextClustererHistoryServiceObserver::
    ~ContextClustererHistoryServiceObserver() = default;

void ContextClustererHistoryServiceObserver::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& new_visit) {
  TRACE_EVENT0("browser",
               "ContextClusteringHistoryServiceObserver::OnURLVisited");

  ScopedVisitProcessingTimer url_visited_processing_timer(
      VisitProcessingStage::kUrlVisited);

  // Update the normalized URL if it's a search URL.
  std::string normalized_url = url_row.url().possibly_invalid_spec();
  std::u16string search_terms;
  bool is_search_normalized_url = false;
  if (template_url_service_) {
    absl::optional<TemplateURLService::SearchMetadata> search_metadata =
        template_url_service_->ExtractSearchMetadata(url_row.url());
    if (search_metadata) {
      normalized_url = search_metadata->normalized_url.possibly_invalid_spec();
      search_terms = search_metadata->search_terms;
      is_search_normalized_url = true;
    }
  }

  history::ClusterVisit cluster_visit =
      CreateClusterVisit(normalized_url, is_search_normalized_url, new_visit);

  if (!new_visit.originator_cache_guid.empty()) {
    // Skip determining the exact cluster id for remote synced visits.

    if (ShouldUseNavigationContextClustersFromPersistence()) {
      // Remote synced visits are context clustered on the originator machine
      // and persisted via `HistorySyncBridge`; however, it still requires it's
      // ClusterVisit metadata. It is calculated here instead of in
      // `HistorySyncBridge` to avoid a circular dependency.
      //
      // If the sender device does not have context clustering at nav time
      // enabled, this ends up being a no-op.
      history_service_->UpdateClusterVisit(
          std::move(cluster_visit),
          base::BindOnce(&LogDbLatencyHistogram,
                         ContextClustererDbLatencyType::kUpdateClusterVisit,
                         base::TimeTicks::Now()),
          &task_tracker_);
    }
    return;
  }

  if (optimization_guide_decider_ &&
      optimization_guide_decider_->CanApplyOptimization(
          url_row.url(), optimization_guide::proto::HISTORY_CLUSTERS,
          /*optimization_metadata=*/nullptr) !=
          optimization_guide::OptimizationGuideDecision::kTrue) {
    // Skip local visits that are on the blocklist.
    return;
  }

  // See what cluster we should add it to.
  absl::optional<int64_t> cluster_id;

  std::vector<history::VisitID> previous_visit_ids_to_check;
  if (new_visit.opener_visit != 0) {
    previous_visit_ids_to_check.push_back(new_visit.opener_visit);
  }
  if (new_visit.referring_visit != 0) {
    previous_visit_ids_to_check.push_back(new_visit.referring_visit);
  }
  if (!previous_visit_ids_to_check.empty()) {
    // See if we have clustered any of the previous visits with opener taking
    // precedence.
    for (history::VisitID previous_visit_id : previous_visit_ids_to_check) {
      auto it = visit_id_to_cluster_map_.find(previous_visit_id);
      if (it != visit_id_to_cluster_map_.end()) {
        cluster_id = it->second;
        break;
      }
    }
  } else {
    // See if we have clustered the URL. (forward-back, reload, etc.)
    auto it = visit_url_to_cluster_map_.find(normalized_url);
    if (it != visit_url_to_cluster_map_.end()) {
      cluster_id = it->second;
    }
  }

  // See if we should add to cluster.
  if (cluster_id) {
    auto& in_progress_cluster = in_progress_clusters_.at(*cluster_id);
    if (!ShouldAddVisitToCluster(new_visit, search_terms,
                                 in_progress_cluster)) {
      FinalizeCluster(*cluster_id);

      cluster_id = absl::nullopt;
    }
  }
  bool is_new_cluster = !cluster_id;

  // Add a new cluster if we haven't assigned one already.
  if (is_new_cluster) {
    cluster_id_counter_++;
    cluster_id = cluster_id_counter_;

    in_progress_clusters_.emplace(*cluster_id, InProgressCluster());
  }

  // Add to cluster maps.
  auto& in_progress_cluster = in_progress_clusters_.at(*cluster_id);
  in_progress_cluster.last_visit_time = new_visit.visit_time;
  in_progress_cluster.visit_urls.insert(normalized_url);
  in_progress_cluster.visit_ids.emplace_back(new_visit.visit_id);
  if (!search_terms.empty()) {
    // Only update the cluster search terms if it's non-empty. It should only be
    // set as non-empty for at most one unique term as enforced by
    // `ShouldAddVisitToCluster()`.
    in_progress_cluster.search_terms = search_terms;
  }
  visit_id_to_cluster_map_[new_visit.visit_id] = *cluster_id;
  visit_url_to_cluster_map_[normalized_url] = *cluster_id;

  // Only persist the visit if the transition is visible. Still add it to the
  // cluster maps in case newer visits reference it though.
  if (ShouldUseNavigationContextClustersFromPersistence() &&
      IsTransitionUserVisible(new_visit.transition)) {
    // For new clusters, asyncly reserve an ID and have the
    //   `OnPersistedClusterIdReceived()` callback add the visits.
    // For clusters created recently for which history service hasn't yet
    //   returned the IDs, there's already a callback pending that will add the
    //   visits.
    // For clusters whose IDs are already known, add the visits here.
    if (in_progress_cluster.persisted_cluster_id > 0) {
      // Persist visit to existing cluster.
      history_service->AddVisitsToCluster(
          in_progress_cluster.persisted_cluster_id, {std::move(cluster_visit)},
          base::BindOnce(&LogDbLatencyHistogram,
                         ContextClustererDbLatencyType::kAddVisitsToCluster,
                         base::TimeTicks::Now()),
          &task_tracker_);
      return;
    }

    // As `in_progress_cluster` does not have a persisted cluster ID yet, add
    // the ClusterVisit to the vector of visits that needs to get persisted.
    in_progress_cluster.unpersisted_visits.push_back(std::move(cluster_visit));

    if (is_new_cluster) {
      // Cluster creation is async. Reserve next cluster ID and wait to persist
      // items until it comes back in `OnPersistedClusterIdReceived()`.
      history_service->ReserveNextClusterId(
          base::BindOnce(&ContextClustererHistoryServiceObserver::
                             OnPersistedClusterIdReceived,
                         weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                         *cluster_id),
          &task_tracker_);
    }
  }
}

void ContextClustererHistoryServiceObserver::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  TRACE_EVENT0("browser",
               "ContextClusteringHistoryServiceObserver::OnURLsDeleted");
  ScopedVisitProcessingTimer urls_deleted_processing_timer(
      VisitProcessingStage::kUrlsDeleted);

  // Clear out everything if the user deleted all history.
  if (deletion_info.IsAllHistory()) {
    in_progress_clusters_.clear();
    visit_url_to_cluster_map_.clear();
    visit_id_to_cluster_map_.clear();
    return;
  }

  // Delete relevant visits from in-progress clusters.
  base::flat_set<int64_t> clusters_to_finalize;
  for (const auto& deleted_url : deletion_info.deleted_rows()) {
    std::string normalized_deleted_url =
        deleted_url.url().possibly_invalid_spec();
    if (template_url_service_) {
      absl::optional<TemplateURLService::SearchMetadata> search_metadata =
          template_url_service_->ExtractSearchMetadata(deleted_url.url());
      if (search_metadata) {
        normalized_deleted_url =
            search_metadata->normalized_url.possibly_invalid_spec();
      }
    }
    auto it = visit_url_to_cluster_map_.find(normalized_deleted_url);
    if (it != visit_url_to_cluster_map_.end()) {
      // TODO(b/259466296): Maybe check time range.
      clusters_to_finalize.insert(it->second);
    }
  }

  // Finalize clusters.
  for (int64_t cluster_id : clusters_to_finalize) {
    FinalizeCluster(cluster_id);
  }
}

void ContextClustererHistoryServiceObserver::CleanUpClusters() {
  TRACE_EVENT0("browser",
               "ContextClusteringHistoryServiceObserver::CleanUpClusters");

  ScopedVisitProcessingTimer clean_up_timer(
      VisitProcessingStage::kCleanUpTimer);

  if (in_progress_clusters_.empty()) {
    // Nothing to clean up, just return.
    return;
  }

  base::UmaHistogramCounts1000(
      "History.Clusters.ContextClusterer.NumClusters.AtCleanUp",
      in_progress_clusters_.size());

  // See which clusters we need to clean up.
  base::flat_set<int64_t> clusters_to_finalize;
  for (const auto& cluster_id_and_cluster : in_progress_clusters_) {
    if ((clock_->Now() - cluster_id_and_cluster.second.last_visit_time) >
        GetConfig().cluster_navigation_time_cutoff) {
      clusters_to_finalize.insert(cluster_id_and_cluster.first);
    }
  }

  // Finalize clusters.
  for (int64_t cluster_id : clusters_to_finalize) {
    FinalizeCluster(cluster_id);
  }

  base::UmaHistogramCounts1000(
      "History.Clusters.ContextClusterer.NumClusters.CleanedUp",
      clusters_to_finalize.size());

  base::UmaHistogramCounts1000(
      "History.Clusters.ContextClusterer.NumClusters.PostCleanUp",
      in_progress_clusters_.size());
}

void ContextClustererHistoryServiceObserver::FinalizeCluster(
    int64_t cluster_id) {
  DCHECK(in_progress_clusters_.find(cluster_id) != in_progress_clusters_.end());

  // Delete relevant visits from in-progress maps.
  auto& cluster = in_progress_clusters_.at(cluster_id);
  for (const auto& visit_url : cluster.visit_urls) {
    visit_url_to_cluster_map_.erase(visit_url);
  }
  for (const auto visit_id : cluster.visit_ids) {
    visit_id_to_cluster_map_.erase(visit_id);
  }

  in_progress_clusters_.erase(cluster_id);
}

void ContextClustererHistoryServiceObserver::OnPersistedClusterIdReceived(
    base::TimeTicks start_time,
    int64_t cluster_id,
    int64_t persisted_cluster_id) {
  LogDbLatencyHistogram(ContextClustererDbLatencyType::kReserveNextClusterId,
                        start_time);

  auto cluster_it = in_progress_clusters_.find(cluster_id);
  base::UmaHistogramBoolean(
      "History.Clusters.ContextClusterer.ClusterCleanedUpBeforePersistence",
      cluster_it == in_progress_clusters_.end());
  if (cluster_it == in_progress_clusters_.end()) {
    return;
  }

  cluster_it->second.persisted_cluster_id = persisted_cluster_id;
  // Persist all visits we've seen so far.
  history_service_->AddVisitsToCluster(
      persisted_cluster_id, cluster_it->second.unpersisted_visits,
      base::BindOnce(&LogDbLatencyHistogram,
                     ContextClustererDbLatencyType::kAddVisitsToCluster,
                     base::TimeTicks::Now()),
      &task_tracker_);

  // Clear these out since the visits have now been requested to be persisted.
  // This is safe to clear here as the vector should have already been copied to
  // the history DB thread in `AddVisitsToCluster()`.
  cluster_it->second.unpersisted_visits.clear();
}

history::ClusterVisit
ContextClustererHistoryServiceObserver::CreateClusterVisit(
    const std::string& normalized_url,
    bool is_search_normalized_url,
    const history::VisitRow& visit_row) {
  history::ClusterVisit cluster_visit;
  cluster_visit.annotated_visit.visit_row.visit_id = visit_row.visit_id;
  cluster_visit.normalized_url = GURL(normalized_url);
  cluster_visit.url_for_deduping =
      is_search_normalized_url
          ? cluster_visit.normalized_url
          : ComputeURLForDeduping(cluster_visit.normalized_url);
  cluster_visit.url_for_display =
      ComputeURLForDisplay(cluster_visit.normalized_url);
  if (engagement_score_provider_) {
    cluster_visit.engagement_score =
        GetEngagementScore(cluster_visit.normalized_url);
  }
  return cluster_visit;
}

float ContextClustererHistoryServiceObserver::GetEngagementScore(
    const GURL& normalized_url) {
  if (!GetConfig().use_engagement_score_cache) {
    return engagement_score_provider_->GetScore(normalized_url);
  }

  std::string visit_host = normalized_url.host();
  auto it = engagement_score_cache_.Peek(visit_host);
  if (it != engagement_score_cache_.end() &&
      it->second.expiry_time > clock_->Now()) {
    return it->second.score;
  }

  float score = engagement_score_provider_->GetScore(normalized_url);
  engagement_score_cache_.Put(
      visit_host,
      CachedEngagementScore(
          score,
          clock_->Now() + GetConfig().engagement_score_cache_refresh_duration));
  return score;
}

void ContextClustererHistoryServiceObserver::OverrideClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

}  // namespace history_clusters
