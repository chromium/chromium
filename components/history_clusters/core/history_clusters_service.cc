// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/file_clustering_backend.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/on_device_clustering_backend.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"

namespace history_clusters {

VisitDeletionObserver::VisitDeletionObserver(
    HistoryClustersService* history_clusters_service)
    : history_clusters_service_(history_clusters_service) {}

VisitDeletionObserver::~VisitDeletionObserver() = default;

void VisitDeletionObserver::AttachToHistoryService(
    history::HistoryService* history_service) {
  DCHECK(history_service);
  history_service_observation_.Observe(history_service);
}

void VisitDeletionObserver::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  history_clusters_service_->ClearKeywordCache();
}

HistoryClustersService::HistoryClustersService(
    const std::string& application_locale,
    history::HistoryService* history_service,
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
    TemplateURLService* template_url_service,
    optimization_guide::NewOptimizationGuideDecider* optimization_guide_decider)
    : is_journeys_enabled_(
          GetConfig().is_journeys_enabled_no_locale_check &&
          IsApplicationLocaleSupportedByJourneys(application_locale)),
      history_service_(history_service),
      visit_deletion_observer_(this),
      context_clusterer_observer_(history_service,
                                  template_url_service,
                                  optimization_guide_decider,
                                  engagement_score_provider) {
  DCHECK(history_service_);

  visit_deletion_observer_.AttachToHistoryService(history_service);

  backend_ = FileClusteringBackend::CreateIfEnabled();
  if (!backend_) {
    backend_ = std::make_unique<OnDeviceClusteringBackend>(
        entity_metadata_provider, engagement_score_provider,
        optimization_guide_decider, JourneysMidBlocklist());
  }

  RepeatedlyUpdateClusters();
}

HistoryClustersService::~HistoryClustersService() = default;

base::WeakPtr<HistoryClustersService> HistoryClustersService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryClustersService::Shutdown() {}

bool HistoryClustersService::IsJourneysEnabled() const {
  return is_journeys_enabled_;
}

void HistoryClustersService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void HistoryClustersService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

bool HistoryClustersService::ShouldNotifyDebugMessage() const {
  return !observers_.empty();
}

void HistoryClustersService::NotifyDebugMessage(
    const std::string& message) const {
  for (Observer& obs : observers_) {
    obs.OnDebugMessage(message);
  }
}

IncompleteVisitContextAnnotations&
HistoryClustersService::GetIncompleteVisitContextAnnotations(int64_t nav_id) {
  DCHECK(HasIncompleteVisitContextAnnotations(nav_id));
  return GetOrCreateIncompleteVisitContextAnnotations(nav_id);
}

IncompleteVisitContextAnnotations&
HistoryClustersService::GetOrCreateIncompleteVisitContextAnnotations(
    int64_t nav_id) {
  return incomplete_visit_context_annotations_[nav_id];
}

bool HistoryClustersService::HasIncompleteVisitContextAnnotations(
    int64_t nav_id) {
  return incomplete_visit_context_annotations_.count(nav_id);
}

void HistoryClustersService::CompleteVisitContextAnnotationsIfReady(
    int64_t nav_id) {
  auto& visit_context_annotations =
      GetIncompleteVisitContextAnnotations(nav_id);
  DCHECK((visit_context_annotations.status.history_rows &&
          visit_context_annotations.status.navigation_ended) ||
         !visit_context_annotations.status.navigation_end_signals);
  DCHECK(visit_context_annotations.status.expect_ukm_page_end_signals ||
         !visit_context_annotations.status.ukm_page_end_signals);
  if (visit_context_annotations.status.history_rows &&
      visit_context_annotations.status.navigation_end_signals &&
      (visit_context_annotations.status.ukm_page_end_signals ||
       !visit_context_annotations.status.expect_ukm_page_end_signals)) {
    // If the main Journeys feature is enabled, we want to persist visits.
    // And if the persist-only switch is enabled, we also want to persist them.
    if (IsJourneysEnabled() ||
        GetConfig().persist_context_annotations_in_history_db) {
      history_service_->SetOnCloseContextAnnotationsForVisit(
          visit_context_annotations.visit_row.visit_id,
          visit_context_annotations.context_annotations);
    }
    incomplete_visit_context_annotations_.erase(nav_id);
  }
}

std::unique_ptr<HistoryClustersServiceTaskGetMostRecentClusters>
HistoryClustersService::QueryClusters(
    ClusteringRequestSource clustering_request_source,
    base::Time begin_time,
    QueryClustersContinuationParams continuation_params,
    bool recluster,
    QueryClustersCallback callback,
    HistoryClustersServiceTaskGetMostRecentClusters::Source source) {
  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("HistoryClustersService::QueryClusters()");
    NotifyDebugMessage(
        "  begin_time = " +
        (begin_time.is_null() ? "null" : base::TimeToISO8601(begin_time)));
    NotifyDebugMessage(
        "  end_time = " +
        (continuation_params.continuation_time.is_null()
             ? "null"
             : base::TimeToISO8601(continuation_params.continuation_time)));
  }

  DCHECK(history_service_);
  return std::make_unique<HistoryClustersServiceTaskGetMostRecentClusters>(
      weak_ptr_factory_.GetWeakPtr(), incomplete_visit_context_annotations_,
      backend_.get(), history_service_, clustering_request_source, begin_time,
      continuation_params, recluster, std::move(callback), source);
}

void HistoryClustersService::RepeatedlyUpdateClusters() {
  // If `persist_on_query` is enabled, clusters are updated on query and not on
  // a timer.
  if (!GetConfig().persist_clusters_in_history_db ||
      GetConfig().persist_on_query) {
    return;
  }

  // Update clusters, both periodically and once after startup because:
  // 1) To avoid having very stale (up to 90 days) clusters for the initial
  //    period after startup.
  // 2) Likewise, to avoid having very stale keywords.
  // 3) Some users might not keep chrome running for the period.
  update_clusters_after_startup_delay_timer_.Start(
      FROM_HERE,
      base::Minutes(
          GetConfig()
              .persist_clusters_in_history_db_after_startup_delay_minutes),
      this, &HistoryClustersService::UpdateClusters);
  update_clusters_period_timer_.Start(
      FROM_HERE,
      base::Minutes(GetConfig().persist_clusters_in_history_db_period_minutes),
      this, &HistoryClustersService::UpdateClusters);
}

void HistoryClustersService::UpdateClusters() {
  DCHECK(history_service_);

  // TODO(manukh): This logic (if task not done, if time since < period) is
  //  repeated for both keyword caches and here. Should probably share it in
  //  some kind of base task that all 3 inherit from.

  if (update_clusters_task_ && !update_clusters_task_->Done())
    return;

  // Make sure clusters aren't updated too frequently. If `persist_on_query` is
  // false, this is already ensured by `update_clusters_period_timer_`. If
  // update_clusters_task_ is null, this is the 1st request which shouldn't be
  // delayed.
  if (GetConfig().persist_on_query &&
      update_clusters_timer_.Elapsed() <=
          base::Minutes(
              GetConfig().persist_clusters_in_history_db_period_minutes) &&
      update_clusters_task_) {
    return;
  }

  // Using custom histogram as this occurs too infrequently to be captured by
  // the built in histograms. `persist_clusters_in_history_db_period_minutes`
  // ranges from 1 to 12 hours while the built in timing histograms go up to 1
  // hr.
  base::UmaHistogramCustomTimes(
      "History.Clusters.UpdateClusters.TimeBetweenTasks",
      update_clusters_timer_.Elapsed(), base::Minutes(60), base::Hours(48),
      100);

  // Reset the timer.
  update_clusters_timer_ = {};

  update_clusters_task_ =
      std::make_unique<HistoryClustersServiceTaskUpdateClusters>(
          weak_ptr_factory_.GetWeakPtr(), incomplete_visit_context_annotations_,
          backend_.get(), history_service_, base::DoNothing());
}

absl::optional<history::ClusterKeywordData>
HistoryClustersService::DoesQueryMatchAnyCluster(const std::string& query) {
  if (!IsJourneysEnabled())
    return absl::nullopt;

  // We don't want any omnibox jank for low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    return absl::nullopt;

  StartKeywordCacheRefresh();
  if (GetConfig().persist_on_query)
    UpdateClusters();

  // Early exit for single-character queries, even if it's an exact match.
  // We still want to allow for two-character exact matches like "uk".
  if (query.length() <= 1)
    return absl::nullopt;

  auto query_lower = base::i18n::ToLower(base::UTF8ToUTF16(query));

  auto short_it = short_keyword_cache_.find(query_lower);
  if (short_it != short_keyword_cache_.end()) {
    return short_it->second;
  }

  auto it = all_keywords_cache_.find(query_lower);
  if (it != all_keywords_cache_.end()) {
    return it->second;
  }

  return absl::nullopt;
}

bool HistoryClustersService::DoesURLMatchAnyCluster(
    const std::string& url_keyword) {
  if (!IsJourneysEnabled())
    return false;

  // We don't want any omnibox jank for low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    return false;

  StartKeywordCacheRefresh();
  if (GetConfig().persist_on_query)
    UpdateClusters();

  return short_url_keywords_cache_.find(url_keyword) !=
             short_url_keywords_cache_.end() ||
         all_url_keywords_cache_.find(url_keyword) !=
             all_url_keywords_cache_.end();
}

void HistoryClustersService::ClearKeywordCache() {
  all_keywords_cache_timestamp_ = base::Time();
  short_keyword_cache_timestamp_ = base::Time();
  all_keywords_cache_.clear();
  all_url_keywords_cache_.clear();
  short_keyword_cache_.clear();
  short_keyword_cache_.clear();
  cache_keyword_query_task_.reset();
}

void HistoryClustersService::PrintKeywordBagStateToLogMessage() const {
  NotifyDebugMessage("-- Printing Short-Time Keyword Bag --");
  NotifyDebugMessage("Timestamp: " +
                     base::TimeToISO8601(short_keyword_cache_timestamp_));
  NotifyDebugMessage(GetDebugJSONForKeywordMap(short_keyword_cache_));
  NotifyDebugMessage(GetDebugJSONForUrlKeywordSet(short_url_keywords_cache_));

  NotifyDebugMessage("-- Printing All-Time Keyword Bag --");
  NotifyDebugMessage("Timestamp: " +
                     base::TimeToISO8601(all_keywords_cache_timestamp_));
  NotifyDebugMessage(GetDebugJSONForKeywordMap(all_keywords_cache_));
  NotifyDebugMessage(GetDebugJSONForUrlKeywordSet(all_url_keywords_cache_));

  NotifyDebugMessage("-- Printing Keyword Bags Done --");
}

void HistoryClustersService::StartKeywordCacheRefresh() {
  // If `all_keywords_cache_` is older than 2 hours, update it with the keywords
  // of all clusters. Otherwise, update `short_keyword_cache_` with the
  // keywords of only the clusters not represented in all_keywords_cache_.

  // Don't make new queries if there's a pending query.
  if (cache_keyword_query_task_ && !cache_keyword_query_task_->Done())
    return;

  // 2 hour threshold chosen arbitrarily for cache refresh time.
  if ((base::Time::Now() - all_keywords_cache_timestamp_) > base::Hours(2)) {
    // Update the timestamp right away, to prevent this from running again.
    // (The cache_query_task_tracker_ should also do this.)
    all_keywords_cache_timestamp_ = base::Time::Now();

    NotifyDebugMessage("Starting all_keywords_cache_ generation.");
    cache_keyword_query_task_ = QueryClusters(
        ClusteringRequestSource::kKeywordCacheGeneration,
        /*begin_time=*/base::Time(),
        /*continuation_params=*/{}, false,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                       /*begin_time=*/base::Time(),
                       std::make_unique<KeywordMap>(),
                       std::make_unique<URLKeywordSet>(), &all_keywords_cache_,
                       &all_url_keywords_cache_),
        HistoryClustersServiceTaskGetMostRecentClusters::Source::
            kAllKeywordCacheRefresh);
  } else if ((base::Time::Now() - all_keywords_cache_timestamp_).InSeconds() >
                 10 &&
             (base::Time::Now() - short_keyword_cache_timestamp_).InSeconds() >
                 10) {
    // Update the timestamp right away, to prevent this from running again.
    short_keyword_cache_timestamp_ = base::Time::Now();

    NotifyDebugMessage("Starting short_keywords_cache_ generation.");
    cache_keyword_query_task_ = QueryClusters(
        ClusteringRequestSource::kKeywordCacheGeneration,
        /*begin_time=*/all_keywords_cache_timestamp_,
        /*continuation_params=*/{}, false,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                       all_keywords_cache_timestamp_,
                       std::make_unique<KeywordMap>(),
                       std::make_unique<URLKeywordSet>(), &short_keyword_cache_,
                       &short_url_keywords_cache_),
        HistoryClustersServiceTaskGetMostRecentClusters::Source::
            kShortKeywordCacheRefresh);
  }
}

void HistoryClustersService::PopulateClusterKeywordCache(
    base::ElapsedTimer total_latency_timer,
    base::Time begin_time,
    std::unique_ptr<KeywordMap> keyword_accumulator,
    std::unique_ptr<URLKeywordSet> url_keyword_accumulator,
    KeywordMap* cache,
    URLKeywordSet* url_cache,
    std::vector<history::Cluster> clusters,
    QueryClustersContinuationParams continuation_params) {
  base::ElapsedThreadTimer populate_keywords_thread_timer;
  const size_t max_keyword_phrases = GetConfig().max_keyword_phrases;

  // Copy keywords from every cluster into the accumulator set.
  for (auto& cluster : clusters) {
    if (!cluster.should_show_on_prominent_ui_surfaces) {
      // `clusters` doesn't have any post-processing, so we need to skip
      // sensitive clusters here.
      continue;
    }
    if (cluster.visits.size() < 2) {
      // Only accept keywords from clusters with at least two visits. This is a
      // simple first-pass technique to avoid overtriggering the omnibox action.
      continue;
    }
    // Lowercase the keywords for case insensitive matching while adding to the
    // accumulator.
    // Keep the keyword data with the highest score if found in multiple
    // clusters.
    if (keyword_accumulator->size() < max_keyword_phrases) {
      for (const auto& keyword_data_p : cluster.keyword_to_data_map) {
        auto keyword = base::i18n::ToLower(keyword_data_p.first);
        auto it = keyword_accumulator->find(keyword);
        if (it == keyword_accumulator->end()) {
          keyword_accumulator->insert(
              std::make_pair(keyword, keyword_data_p.second));
        } else if (it->second.score < keyword_data_p.second.score) {
          // Update keyword data to the one with a higher score.
          it->second = keyword_data_p.second;
        }
      }
    }

    // Push a simplified form of the URL for each visit into the cache.
    if (url_keyword_accumulator->size() < max_keyword_phrases) {
      for (const auto& visit : cluster.visits) {
        if (visit.engagement_score >
                GetConfig().noisy_cluster_visits_engagement_threshold &&
            !GetConfig().omnibox_action_on_noisy_urls) {
          // Do not add a noisy visit to the URL keyword accumulator if not
          // enabled via flag. Note that this is at the visit-level rather than
          // at the cluster-level, which is handled by the NoisyClusterFinalizer
          // in the ClusteringBackend.
          continue;
        }
        url_keyword_accumulator->insert(
            (!visit.annotated_visit.content_annotations.search_normalized_url
                  .is_empty())
                ? visit.normalized_url.spec()
                : ComputeURLKeywordForLookup(visit.normalized_url));
      }
    }
  }

  // Make a continuation request to get the next page of clusters and their
  // keywords only if both 1) there is more clusters remaining, and 2) we
  // haven't reached the soft cap `max_keyword_phrases` (or there is no cap).
  constexpr char kKeywordCacheThreadTimeUmaName[] =
      "History.Clusters.KeywordCache.ThreadTime";
  if (!continuation_params.exhausted_all_visits &&
      (keyword_accumulator->size() < max_keyword_phrases ||
       url_keyword_accumulator->size() < max_keyword_phrases)) {
    const auto query_purpose =
        cache == &all_keywords_cache_
            ? HistoryClustersServiceTaskGetMostRecentClusters::Source::
                  kAllKeywordCacheRefresh
            : HistoryClustersServiceTaskGetMostRecentClusters::Source::
                  kShortKeywordCacheRefresh;
    cache_keyword_query_task_ = QueryClusters(
        ClusteringRequestSource::kKeywordCacheGeneration, begin_time,
        continuation_params, /*recluster=*/false,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(total_latency_timer), begin_time,
                       // Pass on the accumulator sets to the next callback.
                       std::move(keyword_accumulator),
                       std::move(url_keyword_accumulator), cache, url_cache),
        query_purpose);
    // Log this even if we go back for more clusters.
    base::UmaHistogramTimes(kKeywordCacheThreadTimeUmaName,
                            populate_keywords_thread_timer.Elapsed());
    return;
  }

  // We've got all the keywords now. Move them all into the flat_set at once
  // via the constructor for efficiency (as recommended by the flat_set docs).
  // De-duplication is handled by the flat_set itself.
  *cache = std::move(*keyword_accumulator);
  *url_cache = std::move(*url_keyword_accumulator);
  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("Cache construction complete; keyword cache:");
    NotifyDebugMessage(GetDebugJSONForKeywordMap(*cache));
    NotifyDebugMessage("Url cache:");
    NotifyDebugMessage(GetDebugJSONForUrlKeywordSet(*url_cache));
  }

  // Record keyword phrase & keyword counts for the appropriate cache.
  if (cache == &all_keywords_cache_) {
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.AllKeywordsCount",
        static_cast<int>(cache->size()));
  } else {
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.ShortKeywordsCount",
        static_cast<int>(cache->size()));
  }

  base::UmaHistogramTimes(kKeywordCacheThreadTimeUmaName,
                          populate_keywords_thread_timer.Elapsed());
  base::UmaHistogramMediumTimes("History.Clusters.KeywordCache.Latency",
                                total_latency_timer.Elapsed());
}

}  // namespace history_clusters
