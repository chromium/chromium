// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/file_clustering_backend.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service_task_get_most_recent_clusters.h"
#include "components/history_clusters/core/history_clusters_service_task_get_most_recent_clusters_for_ui.h"
#include "components/history_clusters/core/history_clusters_service_task_update_cluster_triggerability.h"
#include "components/history_clusters/core/history_clusters_service_task_update_clusters.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/on_device_clustering_backend.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"

namespace history_clusters {

namespace {

void RecordUpdateClustersLatencyHistogram(const std::string& histogram_name,
                                          base::ElapsedTimer elapsed_timer) {
  base::UmaHistogramMediumTimes(histogram_name, elapsed_timer.Elapsed());
}

// Serializes a KeywordMap to a base::Value::Dict using hardcoded keys. Recover
// a KeywordMap serialized in this way via DictToKeywordsCache.
base::Value::Dict KeywordsCacheToDict(
    HistoryClustersService::KeywordMap* keyword_map) {
  base::Value::Dict keyword_dict;
  if (!keyword_map) {
    return keyword_dict;
  }
  for (const auto& pair : *keyword_map) {
    base::Value::Dict cluster_keyword_dict;
    cluster_keyword_dict.Set("type", pair.second.type);
    cluster_keyword_dict.Set("score", pair.second.score);
    keyword_dict.Set(base::UTF16ToUTF8(pair.first),
                     std::move(cluster_keyword_dict));
  }
  return keyword_dict;
}

// Deserializes a KeywordMap from a base::Value::Dict serialized using
// KeywordsCacheToDict().
HistoryClustersService::KeywordMap DictToKeywordsCache(
    const base::Value::Dict* dict) {
  HistoryClustersService::KeywordMap keyword_map;
  if (!dict) {
    return keyword_map;
  }

  for (auto pair : *dict) {
    const base::Value::Dict& entry_dict = pair.second.GetDict();
    std::optional<int> type = entry_dict.FindInt("type");
    std::optional<double> score = entry_dict.FindDouble("score");
    if (!type || !score) {
      continue;
    }
    keyword_map.insert(std::make_pair(
        base::UTF8ToUTF16(pair.first),
        history::ClusterKeywordData(
            static_cast<history::ClusterKeywordData::ClusterKeywordType>(*type),
            *score)));
  }

  return keyword_map;
}

constexpr base::TimeDelta kAllKeywordsCacheRefreshAge = base::Hours(2);

}  // namespace

HistoryClustersService::HistoryClustersService(
    const std::string& application_locale,
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
    TemplateURLService* template_url_service,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    PrefService* prefs)
    : persist_caches_to_prefs_(GetConfig().persist_caches_to_prefs),
      is_journeys_feature_flag_enabled_(
          GetConfig().is_journeys_enabled_no_locale_check &&
          IsApplicationLocaleSupportedByJourneys(application_locale)),
      history_service_(history_service),
      pref_service_(prefs) {
  if (prefs && is_journeys_feature_flag_enabled_) {
    // Log whether the user has Journeys enabled if they are eligible for it.
    base::UmaHistogramBoolean(
        "History.Clusters.JourneysEligibleAndEnabledAtSessionStart",
        prefs->GetBoolean(prefs::kVisible));
  }

  if (!is_journeys_feature_flag_enabled_) {
    return;
  }

  // The remaining pieces are only needed for Journeys, so don't instantiate
  // them if Journeys is not enabled.

  if (history_service_) {
    history_service_observation_.Observe(history_service);
  }

  context_clusterer_observer_ =
      std::make_unique<ContextClustererHistoryServiceObserver>(
          history_service, template_url_service, optimization_guide_decider,
          engagement_score_provider);

  backend_ = FileClusteringBackend::CreateIfEnabled();
  if (!backend_) {
    backend_ = std::make_unique<OnDeviceClusteringBackend>(
        engagement_score_provider, optimization_guide_decider);
  }

  LoadCachesFromPrefs();
}

HistoryClustersService::~HistoryClustersService() = default;

base::WeakPtr<HistoryClustersService> HistoryClustersService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryClustersService::Shutdown() {}

bool HistoryClustersService::IsJourneysEnabledAndVisible() const {
  const bool journeys_is_managed =
      pref_service_->IsManagedPreference(prefs::kVisible);
  // History clusters are always visible unless the visibility prefs is
  // set to false by policy.
  return is_journeys_feature_flag_enabled_ &&
         (pref_service_->GetBoolean(prefs::kVisible) || !journeys_is_managed);
}

// static
bool HistoryClustersService::IsJourneysImagesEnabled() {
  return GetConfig().images;
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
    if (IsJourneysEnabledAndVisible() ||
        GetConfig().persist_context_annotations_in_history_db) {
      history_service_->SetOnCloseContextAnnotationsForVisit(
          visit_context_annotations.visit_row.visit_id,
          visit_context_annotations.context_annotations);
    }
    incomplete_visit_context_annotations_.erase(nav_id);
  }
}

std::unique_ptr<HistoryClustersServiceTask>
HistoryClustersService::QueryClusters(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    base::Time begin_time,
    QueryClustersContinuationParams continuation_params,
    bool recluster,
    QueryClustersCallback callback) {
  if (!IsJourneysEnabledAndVisible()) {
    // TODO(crbug.com/40266727): Make this into a CHECK after verifying all
    // callers.
    std::move(callback).Run({}, QueryClustersContinuationParams::DoneParams());
    return nullptr;
  }

  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("HistoryClustersService::QueryClusters()");
    NotifyDebugMessage("  begin_time = " + GetDebugTime(begin_time));
    NotifyDebugMessage("  end_time = " +
                       GetDebugTime(continuation_params.continuation_time));
  }

  DCHECK(history_service_);
  if (ShouldUseNavigationContextClustersFromPersistence() &&
      IsUIRequestSource(clustering_request_source) && !recluster) {
    return std::make_unique<
        HistoryClustersServiceTaskGetMostRecentClustersForUI>(
        weak_ptr_factory_.GetWeakPtr(), backend_.get(), history_service_,
        clustering_request_source, std::move(filter_params), begin_time,
        continuation_params, std::move(callback));
  }
  return std::make_unique<HistoryClustersServiceTaskGetMostRecentClusters>(
      weak_ptr_factory_.GetWeakPtr(), incomplete_visit_context_annotations_,
      backend_.get(), history_service_, clustering_request_source, begin_time,
      continuation_params, recluster, std::move(callback));
}

void HistoryClustersService::UpdateClusters() {
  DCHECK(history_service_);

  // TODO(manukh): This logic (if task not done, if time since < period) is
  //  repeated for both keyword caches and here. Should probably share it in
  //  some kind of base task that all 3 inherit from.

  if (update_clusters_task_ && !update_clusters_task_->Done())
    return;

  // Make sure clusters aren't updated too frequently. If update_clusters_task_
  // is null, this is the 1st request which shouldn't be delayed.
  if (update_clusters_timer_.Elapsed() <=
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

  if (GetConfig().use_navigation_context_clusters) {
    update_clusters_task_ =
        std::make_unique<HistoryClustersServiceTaskUpdateClusterTriggerability>(
            weak_ptr_factory_.GetWeakPtr(), backend_.get(), history_service_,
            received_synced_visit_since_last_update_,
            base::BindOnce(
                &RecordUpdateClustersLatencyHistogram,
                "History.Clusters.Backend.UpdateClusterTriggerability.Total",
                base::ElapsedTimer()));

    // Reset state for next iteration.
    received_synced_visit_since_last_update_ = false;
  } else {
    update_clusters_task_ =
        std::make_unique<HistoryClustersServiceTaskUpdateClusters>(
            weak_ptr_factory_.GetWeakPtr(),
            incomplete_visit_context_annotations_, backend_.get(),
            history_service_,
            base::BindOnce(&RecordUpdateClustersLatencyHistogram,
                           "History.Clusters.Backend.UpdateClusters.Total",
                           base::ElapsedTimer()));
  }
}

std::optional<history::ClusterKeywordData>
HistoryClustersService::DoesQueryMatchAnyCluster(const std::string& query) {
  if (!IsJourneysEnabledAndVisible()) {
    return std::nullopt;
  }

  // We don't want any omnibox jank for low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    return std::nullopt;

  StartKeywordCacheRefresh();
  UpdateClusters();

  // Early exit for single-character queries, even if it's an exact match.
  // We still want to allow for two-character exact matches like "uk".
  if (query.length() <= 1)
    return std::nullopt;

  auto query_lower = base::i18n::ToLower(base::UTF8ToUTF16(query));

  auto short_it = short_keyword_cache_.find(query_lower);
  if (short_it != short_keyword_cache_.end()) {
    return short_it->second;
  }

  auto it = all_keywords_cache_.find(query_lower);
  if (it != all_keywords_cache_.end()) {
    return it->second;
  }

  return std::nullopt;
}

void HistoryClustersService::ClearKeywordCache() {
  all_keywords_cache_timestamp_ = base::Time();
  short_keyword_cache_timestamp_ = base::Time();
  all_keywords_cache_.clear();
  short_keyword_cache_.clear();
  cache_keyword_query_task_.reset();
  WriteShortCacheToPrefs();
  WriteAllCacheToPrefs();
}

void HistoryClustersService::PrintKeywordBagStateToLogMessage() const {
  NotifyDebugMessage("-- Printing Short-Time Keyword Bag --");
  NotifyDebugMessage("Timestamp: " +
                     GetDebugTime(short_keyword_cache_timestamp_));
  NotifyDebugMessage(GetDebugJSONForKeywordMap(short_keyword_cache_));

  NotifyDebugMessage("-- Printing All-Time Keyword Bag --");
  NotifyDebugMessage("Timestamp: " +
                     GetDebugTime(all_keywords_cache_timestamp_));
  NotifyDebugMessage(GetDebugJSONForKeywordMap(all_keywords_cache_));

  NotifyDebugMessage("-- Printing Keyword Bags Done --");
}

void HistoryClustersService::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& visit_row) {
  if (!visit_row.originator_cache_guid.empty()) {
    received_synced_visit_since_last_update_ = true;
  }
}

void HistoryClustersService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  ClearKeywordCache();
}

void HistoryClustersService::StartKeywordCacheRefresh() {
  // If `all_keywords_cache_` is older than 2 hours, update it with the keywords
  // of all clusters. Otherwise, update `short_keyword_cache_` with the
  // keywords of only the clusters not represented in all_keywords_cache_.

  // Don't make new queries if there's a pending query.
  if (cache_keyword_query_task_ && !cache_keyword_query_task_->Done())
    return;

  QueryClustersContinuationParams continuation_params;
  if (ShouldUseNavigationContextClustersFromPersistence()) {
    // Overwrite this so that queries for unclustered visits are not made.
    // In the old path, the `GetAnnotatedVisitsToCluster` task would set
    // the continuation time when it exhausted all unclustered visits, so it
    // needs to be set here.
    continuation_params.exhausted_unclustered_visits = true;
    continuation_params.continuation_time = base::Time::Now();
  }

  // 2 hour threshold chosen arbitrarily for cache refresh time.
  if ((base::Time::Now() - all_keywords_cache_timestamp_) >
      kAllKeywordsCacheRefreshAge) {
    // Update the timestamp right away, to prevent this from running again.
    // (The cache_query_task_tracker_ should also do this.)
    all_keywords_cache_timestamp_ = base::Time::Now();

    NotifyDebugMessage("Starting all_keywords_cache_ generation.");
    cache_keyword_query_task_ = QueryClusters(
        ClusteringRequestSource::kAllKeywordCacheRefresh,
        QueryClustersFilterParams(),
        /*begin_time=*/base::Time::Min(), continuation_params,
        /*recluster=*/false,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                       /*begin_time=*/base::Time(),
                       std::make_unique<KeywordMap>(), &all_keywords_cache_));
  } else if ((base::Time::Now() - all_keywords_cache_timestamp_).InSeconds() >
                 10 &&
             (base::Time::Now() - short_keyword_cache_timestamp_).InSeconds() >
                 10) {
    // Update the timestamp right away, to prevent this from running again.
    short_keyword_cache_timestamp_ = base::Time::Now();

    NotifyDebugMessage("Starting short_keywords_cache_ generation.");
    cache_keyword_query_task_ = QueryClusters(
        ClusteringRequestSource::kShortKeywordCacheRefresh,
        QueryClustersFilterParams(),
        /*begin_time=*/all_keywords_cache_timestamp_, continuation_params,
        /*recluster=*/false,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                       all_keywords_cache_timestamp_,
                       std::make_unique<KeywordMap>(), &short_keyword_cache_));
  } else if (keyword_cache_refresh_callback_for_testing_) {
    std::move(keyword_cache_refresh_callback_for_testing_).Run();
  }
}

void HistoryClustersService::PopulateClusterKeywordCache(
    base::ElapsedTimer total_latency_timer,
    base::Time begin_time,
    std::unique_ptr<KeywordMap> keyword_accumulator,
    KeywordMap* cache,
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
    const size_t visible_visits =
        base::ranges::count_if(cluster.visits, [](const auto& cluster_visit) {
          // Hidden visits shouldn't contribute to the keyword bag, but Done
          // visits still can, since they are searchable.
          return cluster_visit.score > 0 &&
                 cluster_visit.interaction_state !=
                     history::ClusterVisit::InteractionState::kHidden;
        });
    if (visible_visits < 2) {
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
  }

  // Make a continuation request to get the next page of clusters and their
  // keywords only if both 1) there is more clusters remaining, and 2) we
  // haven't reached the soft cap `max_keyword_phrases` (or there is no cap).
  constexpr char kKeywordCacheThreadTimeUmaName[] =
      "History.Clusters.KeywordCache.ThreadTime";
  if (!continuation_params.exhausted_all_visits &&
      keyword_accumulator->size() < max_keyword_phrases) {
    const ClusteringRequestSource clustering_request_source =
        cache == &all_keywords_cache_
            ? ClusteringRequestSource::kAllKeywordCacheRefresh
            : ClusteringRequestSource::kShortKeywordCacheRefresh;
    cache_keyword_query_task_ = QueryClusters(
        clustering_request_source, QueryClustersFilterParams(), begin_time,
        continuation_params,
        /*recluster=*/false,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(total_latency_timer), begin_time,
                       // Pass on the accumulator sets to the next callback.
                       std::move(keyword_accumulator), cache));
    // Log this even if we go back for more clusters.
    base::UmaHistogramTimes(kKeywordCacheThreadTimeUmaName,
                            populate_keywords_thread_timer.Elapsed());
    return;
  }

  // We've got all the keywords now. Move them all into the flat_set at once
  // via the constructor for efficiency (as recommended by the flat_set docs).
  // De-duplication is handled by the flat_set itself.
  *cache = std::move(*keyword_accumulator);
  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("Cache construction complete; keyword cache:");
    NotifyDebugMessage(GetDebugJSONForKeywordMap(*cache));
  }

  // Record keyword phrase & keyword counts for the appropriate cache.
  if (cache == &all_keywords_cache_) {
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.AllKeywordsCount",
        static_cast<int>(cache->size()));
    WriteAllCacheToPrefs();
  } else {
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.ShortKeywordsCount",
        static_cast<int>(cache->size()));
    WriteShortCacheToPrefs();
  }

  base::UmaHistogramTimes(kKeywordCacheThreadTimeUmaName,
                          populate_keywords_thread_timer.Elapsed());
  base::UmaHistogramMediumTimes("History.Clusters.KeywordCache.Latency",
                                total_latency_timer.Elapsed());

  if (keyword_cache_refresh_callback_for_testing_) {
    std::move(keyword_cache_refresh_callback_for_testing_).Run();
  }
}

void HistoryClustersService::LoadCachesFromPrefs() {
  if (!pref_service_ || !persist_caches_to_prefs_) {
    return;
  }

  base::ElapsedTimer load_caches_timer;
  const base::Value::Dict& short_cache_dict =
      pref_service_->GetDict(prefs::kShortCache);
  const base::Value::Dict* short_keywords_dict =
      short_cache_dict.FindDict("short_keywords");
  short_keyword_cache_ = DictToKeywordsCache(short_keywords_dict);
  short_keyword_cache_timestamp_ =
      base::ValueToTime(short_cache_dict.Find("short_timestamp"))
          .value_or(short_keyword_cache_timestamp_);

  const base::Value::Dict& all_cache_dict =
      pref_service_->GetDict(prefs::kAllCache);
  const base::Value::Dict* all_keywords_dict =
      all_cache_dict.FindDict("all_keywords");
  all_keywords_cache_ = DictToKeywordsCache(all_keywords_dict);
  // When loading `all_keywords_cache_` from the prefs, make sure it will be
  // refreshed after 15 seconds, regardless of the persisted timestamp. This is
  // to account for new synced visits, and to flush away stale data on restart.
  // Extra 15 seconds is to avoid impacting startup. https://crbug.com/1444256.
  all_keywords_cache_timestamp_ = std::min(
      base::ValueToTime(all_cache_dict.Find("all_timestamp"))
          .value_or(all_keywords_cache_timestamp_),
      base::Time::Now() - kAllKeywordsCacheRefreshAge + base::Seconds(15));

  base::UmaHistogramCustomTimes(
      "History.Clusters.KeywordCache.LoadCachesFromPrefs.Latency",
      load_caches_timer.Elapsed(), base::Microseconds(50), base::Seconds(2),
      50);
}

void HistoryClustersService::WriteShortCacheToPrefs() {
  if (!pref_service_ || !persist_caches_to_prefs_) {
    return;
  }

  base::ElapsedTimer write_short_cache_timer;
  base::Value::Dict short_cache_dict;
  short_cache_dict.Set("short_keywords",
                       KeywordsCacheToDict(&short_keyword_cache_));
  short_cache_dict.Set("short_timestamp",
                       base::TimeToValue(short_keyword_cache_timestamp_));
  pref_service_->SetDict(prefs::kShortCache, std::move(short_cache_dict));
  base::UmaHistogramCustomTimes(
      "History.Clusters.KeywordCache.WriteCache.Short.Latency",
      write_short_cache_timer.Elapsed(), base::Microseconds(50),
      base::Seconds(2), 50);
}

void HistoryClustersService::WriteAllCacheToPrefs() {
  if (!pref_service_ || !persist_caches_to_prefs_) {
    return;
  }

  base::ElapsedTimer write_all_cache_timer;
  base::Value::Dict all_cache_dict;
  all_cache_dict.Set("all_keywords", KeywordsCacheToDict(&all_keywords_cache_));
  all_cache_dict.Set("all_timestamp",
                     base::TimeToValue(all_keywords_cache_timestamp_));
  pref_service_->SetDict(prefs::kAllCache, std::move(all_cache_dict));
  base::UmaHistogramCustomTimes(
      "History.Clusters.KeywordCache.WriteCache.All.Latency",
      write_all_cache_timer.Elapsed(), base::Microseconds(50), base::Seconds(2),
      50);
}

}  // namespace history_clusters
