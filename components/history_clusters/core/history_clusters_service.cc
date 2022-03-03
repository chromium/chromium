// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_buildflags.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/search_engines/template_url_service.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/time_format.h"

#if BUILDFLAG(BUILD_WITH_ON_DEVICE_CLUSTERING_BACKEND)
#include "components/history_clusters/core/on_device_clustering_backend.h"
#endif

namespace history_clusters {

namespace {

// Gets a loggable JSON representation of `visits`.
std::string GetDebugJSONForVisits(
    const std::vector<history::AnnotatedVisit>& visits) {
  base::ListValue debug_visits_list;
  for (auto& visit : visits) {
    base::DictionaryValue debug_visit;
    debug_visit.SetIntKey("visitId", visit.visit_row.visit_id);
    debug_visit.SetStringKey("url", visit.url_row.url().spec());
    debug_visit.SetStringKey("title", visit.url_row.title());
    debug_visit.SetIntKey("foreground_time_secs",
                          visit.visit_row.visit_duration.InSeconds());
    debug_visit.SetIntKey(
        "navigationTimeMs",
        visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
    debug_visit.SetIntKey("pageEndReason",
                          visit.context_annotations.page_end_reason);
    debug_visit.SetIntKey("pageTransition",
                          static_cast<int>(visit.visit_row.transition));
    debug_visit.SetIntKey("referringVisitId",
                          visit.referring_visit_of_redirect_chain_start);
    debug_visit.SetIntKey("openerVisitId",
                          visit.opener_visit_of_redirect_chain_start);
    debug_visits_list.Append(std::move(debug_visit));
  }

  base::DictionaryValue debug_value;
  debug_value.SetKey("visits", std::move(debug_visits_list));
  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &debug_string)) {
    debug_string = "Error: Could not write visits to JSON.";
  }
  return debug_string;
}

// Gets a loggable JSON representation of `clusters`.
std::string GetDebugJSONForClusters(
    const std::vector<history::Cluster>& clusters) {
  // TODO(manukh): `ListValue` is deprecated; replace with `std::vector`.
  base::ListValue debug_clusters_list;
  for (const auto& cluster : clusters) {
    base::DictionaryValue debug_cluster;

    debug_cluster.SetStringKey("label", cluster.label.value_or(u""));
    base::ListValue debug_keywords;
    for (const auto& keyword : cluster.keywords) {
      debug_keywords.Append(keyword);
    }
    debug_cluster.SetKey("keywords", std::move(debug_keywords));
    debug_cluster.SetBoolKey("should_show_on_prominent_ui_surfaces",
                             cluster.should_show_on_prominent_ui_surfaces);

    base::ListValue debug_visits;
    for (const auto& visit : cluster.visits) {
      base::DictionaryValue debug_visit;
      debug_visit.SetIntKey("visit_id",
                            visit.annotated_visit.visit_row.visit_id);
      debug_visit.SetDoubleKey("score", visit.score);
      base::ListValue debug_categories;
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        base::DictionaryValue debug_category;
        debug_category.SetStringKey("name", category.id);
        debug_category.SetIntKey("value", category.weight);
        debug_categories.Append(std::move(debug_category));
      }
      debug_visit.SetKey("categories", std::move(debug_categories));
      base::ListValue debug_entities;
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
        base::DictionaryValue debug_entity;
        debug_entity.SetStringKey("name", entity.id);
        debug_entity.SetIntKey("value", entity.weight);
        debug_entities.Append(std::move(debug_entity));
      }
      debug_visit.SetKey("entities", std::move(debug_entities));

      base::ListValue debug_duplicate_visits;
      for (const auto& duplicate_visit : visit.duplicate_visits) {
        debug_duplicate_visits.Append(static_cast<int>(
            duplicate_visit.annotated_visit.visit_row.visit_id));
      }
      debug_visit.SetKey("duplicate_visits", std::move(debug_duplicate_visits));

      debug_visits.Append(std::move(debug_visit));
    }
    debug_cluster.SetKey("visits", std::move(debug_visits));

    debug_clusters_list.Append(std::move(debug_cluster));
  }

  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_clusters_list, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write clusters to JSON.";
  }
  return debug_string;
}

std::string GetDebugJSONForKeywordSet(
    const HistoryClustersService::KeywordSet& keyword_set) {
  std::vector<base::Value> keyword_list;
  for (const auto& keyword : keyword_set) {
    keyword_list.emplace_back(keyword);
  }

  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          base::Value(keyword_list), base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write keywords list to JSON.";
  }
  return debug_string;
}

}  // namespace

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
    TemplateURLService* template_url_service,
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider)
    : is_journeys_enabled_(
          GetConfig().is_journeys_enabled_no_locale_check &&
          IsApplicationLocaleSupportedByJourneys(application_locale)),
      history_service_(history_service),
      visit_deletion_observer_(this) {
  DCHECK(history_service_);

  visit_deletion_observer_.AttachToHistoryService(history_service);

#if BUILDFLAG(BUILD_WITH_ON_DEVICE_CLUSTERING_BACKEND)
  backend_ = std::make_unique<OnDeviceClusteringBackend>(
      template_url_service, entity_metadata_provider,
      engagement_score_provider);
#endif
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
      history_service_->AddContextAnnotationsForVisit(
          visit_context_annotations.visit_row.visit_id,
          visit_context_annotations.context_annotations);
    }
    incomplete_visit_context_annotations_.erase(nav_id);
  }
}

void HistoryClustersService::QueryClusters(
    ClusteringRequestSource clustering_request_source,
    base::Time begin_time,
    base::Time end_time,
    QueryClustersCallback callback,
    base::CancelableTaskTracker* task_tracker) {
  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("HistoryClustersService::QueryClusters()");
    NotifyDebugMessage(
        "  begin_time = " +
        (begin_time.is_null() ? "null" : base::TimeToISO8601(begin_time)));
    NotifyDebugMessage("  end_time = " + (end_time.is_null()
                                              ? "null"
                                              : base::TimeToISO8601(end_time)));
  }

  if (!backend_) {
    NotifyDebugMessage(
        "HistoryClustersService::QueryClusters Error: ClusteringBackend is "
        "nullptr. Returning empty cluster vector.");
    std::move(callback).Run({}, base::Time());
    return;
  }

  DCHECK(history_service_);
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<GetAnnotatedVisitsToCluster>(
          incomplete_visit_context_annotations_, begin_time, end_time,
          base::BindOnce(&HistoryClustersService::OnGotHistoryVisits,
                         weak_ptr_factory_.GetWeakPtr(),
                         clustering_request_source, base::TimeTicks::Now(),
                         std::move(callback))),
      task_tracker);
}

void HistoryClustersService::RemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list,
    base::OnceClosure closure,
    base::CancelableTaskTracker* task_tracker) {
  // We expect HistoryService to internally delete any associated annotations
  // and cluster rows. In the future we may remove this indirection entirely.
  history_service_->ExpireHistory(expire_list, std::move(closure),
                                  task_tracker);
}

bool HistoryClustersService::DoesQueryMatchAnyCluster(
    const std::string& query) {
  if (!IsJourneysEnabled())
    return false;

  // We don't want any omnibox jank for low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    return false;

  // If `all_keywords_cache_` is older than 2 hours, update it with the keywords
  // of all clusters. Otherwise, update `short_keyword_cache_` with the
  // keywords of only the clusters not represented in all_keywords_cache_.

  // 2 hour threshold chosen arbitrarily for cache refresh time.
  if ((base::Time::Now() - all_keywords_cache_timestamp_) > base::Hours(2) &&
      !cache_query_task_tracker_.HasTrackedTasks()) {
    // Update the timestamp right away, to prevent this from running again.
    // (The cache_query_task_tracker_ should also do this.)
    all_keywords_cache_timestamp_ = base::Time::Now();

    NotifyDebugMessage("Starting all_keywords_cache_ generation.");
    QueryClusters(
        ClusteringRequestSource::kKeywordCacheGeneration,
        /*begin_time=*/base::Time(),
        /*end_time=*/base::Time(),
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                       /*begin_time=*/base::Time(),
                       std::make_unique<std::vector<std::u16string>>(),
                       &all_keywords_cache_),
        &cache_query_task_tracker_);
  } else if (!cache_query_task_tracker_.HasTrackedTasks() &&
             (base::Time::Now() - all_keywords_cache_timestamp_).InSeconds() >
                 10 &&
             (base::Time::Now() - short_keyword_cache_timestamp_).InSeconds() >
                 10) {
    // Update the timestamp right away, to prevent this from running again.
    short_keyword_cache_timestamp_ = base::Time::Now();

    NotifyDebugMessage("Starting short_keywords_cache_ generation.");
    QueryClusters(
        ClusteringRequestSource::kKeywordCacheGeneration,
        /*begin_time=*/all_keywords_cache_timestamp_, /*end_time=*/base::Time(),
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                       all_keywords_cache_timestamp_,
                       std::make_unique<std::vector<std::u16string>>(),
                       &short_keyword_cache_),
        &cache_query_task_tracker_);
  }

  // Early exit for single-character queries, even if it's an exact match.
  // We still want to allow for two-character exact matches like "uk".
  if (query.length() <= 1)
    return false;

  auto query_lower = base::i18n::ToLower(base::UTF8ToUTF16(query));

  return short_keyword_cache_.contains(query_lower) ||
         all_keywords_cache_.contains(query_lower);
}

void HistoryClustersService::ClearKeywordCache() {
  all_keywords_cache_timestamp_ = base::Time();
  short_keyword_cache_timestamp_ = base::Time();
  all_keywords_cache_.clear();
  short_keyword_cache_.clear();
  cache_query_task_tracker_.TryCancelAll();
}

void HistoryClustersService::PopulateClusterKeywordCache(
    base::ElapsedTimer total_latency_timer,
    base::Time begin_time,
    std::unique_ptr<std::vector<std::u16string>> keyword_accumulator,
    KeywordSet* cache,
    std::vector<history::Cluster> clusters,
    base::Time continuation_end_time) {
  base::ElapsedThreadTimer populate_keywords_thread_timer;
  const size_t max_keyword_phrases = GetConfig().max_keyword_phrases;

  // Copy keywords from every cluster into a the accumulator set.
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
    for (auto& keyword : cluster.keywords) {
      keyword_accumulator->push_back(base::i18n::ToLower(keyword));
    }

    // Limit the cache size. It's possible for the `cache.size()` to exceed
    // `max_keyword_phrases` since:
    // 1) We cache all of a particular cluster's keywords if we haven't yet
    //    reached the cap, even if doing so will exceed the cap.
    // 2) We have 2 caches which are capped separately.
    // 3) We cap the # of phrase, each of which may contain multiple words,
    //    whereas `cache` contains individual words.
    if (max_keyword_phrases != 0 &&
        keyword_accumulator->size() >= max_keyword_phrases) {
      break;
    }
  }

  // Make a continuation request to get the next page of clusters and their
  // keywords only if both 1) there is more clusters remaining, and 2) we
  // haven't reached the soft cap `max_keyword_phrases` (or there is no cap).
  constexpr char kKeywordCacheThreadTimeUmaName[] =
      "History.Clusters.KeywordCache.ThreadTime";
  if (!continuation_end_time.is_null() &&
      (max_keyword_phrases == 0 ||
       keyword_accumulator->size() < max_keyword_phrases)) {
    QueryClusters(
        ClusteringRequestSource::kKeywordCacheGeneration, begin_time,
        continuation_end_time,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(total_latency_timer), begin_time,
                       // Pass on the accumulator set to the next callback.
                       std::move(keyword_accumulator), cache),
        &cache_query_task_tracker_);
    // Log this even if we go back for more clusters.
    base::UmaHistogramTimes(kKeywordCacheThreadTimeUmaName,
                            populate_keywords_thread_timer.Elapsed());
    return;
  }

  // We've got all the keywords now. Move them all into the flat_set at once
  // via the constructor for efficiency (as recommended by the flat_set docs).
  // De-duplication is handled by the flat_set itself.
  *cache = KeywordSet(*keyword_accumulator);
  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("Cache construction complete:");
    NotifyDebugMessage(GetDebugJSONForKeywordSet(*cache));
  }

  // Record keyword phrase & keyword counts for the appropriate cache.
  if (cache == &all_keywords_cache_) {
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.AllKeywordPhraseCount",
        static_cast<int>(keyword_accumulator->size()));
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.AllKeywordsCount",
        static_cast<int>(cache->size()));
  } else {
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.ShortKeywordPhraseCount",
        static_cast<int>(keyword_accumulator->size()));
    base::UmaHistogramCounts100000(
        "History.Clusters.Backend.KeywordCache.ShortKeywordsCount",
        static_cast<int>(cache->size()));
  }

  base::UmaHistogramTimes(kKeywordCacheThreadTimeUmaName,
                          populate_keywords_thread_timer.Elapsed());
  base::UmaHistogramMediumTimes("History.Clusters.KeywordCache.Latency",
                                total_latency_timer.Elapsed());
}

void HistoryClustersService::OnGotHistoryVisits(
    ClusteringRequestSource clustering_request_source,
    base::TimeTicks query_visits_start,
    QueryClustersCallback callback,
    std::vector<history::AnnotatedVisit> annotated_visits,
    base::Time continuation_end_time) const {
  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("HistoryClustersService::OnGotHistoryVisits()");
    NotifyDebugMessage(base::StringPrintf("  annotated_visits.size() = %zu",
                                          annotated_visits.size()));
    NotifyDebugMessage("  continuation_end_time = " +
                       (continuation_end_time.is_null()
                            ? "null (i.e. exhausted history)"
                            : base::TimeToISO8601(continuation_end_time)));
  }

  base::UmaHistogramTimes(
      "Histogram.Clusters.Backend.QueryAnnotatedVisitsLatency",
      base::TimeTicks::Now() - query_visits_start);

  if (annotated_visits.empty()) {
    // Early exit without calling backend if there's no annotated visits.
    std::move(callback).Run({}, continuation_end_time);
    return;
  }

  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("  Visits JSON follows:");
    NotifyDebugMessage(GetDebugJSONForVisits(annotated_visits));
    NotifyDebugMessage("Calling backend_->GetClusters()");
  }
  base::UmaHistogramCounts1000("History.Clusters.Backend.NumVisitsToCluster",
                               static_cast<int>(annotated_visits.size()));

  backend_->GetClusters(
      clustering_request_source,
      base::BindOnce(&HistoryClustersService::OnGotRawClusters,
                     weak_ptr_factory_.GetWeakPtr(), continuation_end_time,
                     base::TimeTicks::Now(), std::move(callback)),
      std::move(annotated_visits));
}

void HistoryClustersService::OnGotRawClusters(
    base::Time continuation_end_time,
    base::TimeTicks cluster_start_time,
    QueryClustersCallback callback,
    std::vector<history::Cluster> clusters) const {
  base::UmaHistogramTimes("History.Clusters.Backend.GetClustersLatency",
                          base::TimeTicks::Now() - cluster_start_time);
  base::UmaHistogramCounts1000("History.Clusters.Backend.NumClustersReturned",
                               clusters.size());

  if (ShouldNotifyDebugMessage()) {
    NotifyDebugMessage("HistoryClustersService::OnGotRawClusters()");
    NotifyDebugMessage("  Raw Clusters from Backend JSON follows:");
    NotifyDebugMessage(GetDebugJSONForClusters(clusters));
  }

  std::move(callback).Run(clusters, continuation_end_time);
}

}  // namespace history_clusters
