// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_buildflags.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/remote_clustering_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/time_format.h"

#if BUILDFLAG(BUILD_WITH_ON_DEVICE_CLUSTERING_BACKEND)
#include "components/history_clusters/internal/on_device_clustering_backend.h"
#endif

namespace history_clusters {

namespace {

// Filter `clusters` matching `query`. There are additional filters (e.g.
// `max_time`) used when requesting `QueryClusters()`, but this function is only
// responsible for matching `query`.
std::vector<history::Cluster> FilterClustersMatchingQuery(
    std::string query,
    const std::vector<history::Cluster>& clusters) {
  if (query.empty())
    return clusters;

  // Extract query nodes from the query string.
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query),
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH, &query_nodes);

  std::vector<history::Cluster> matching_clusters;
  base::ranges::copy_if(clusters, std::back_inserter(matching_clusters),
                        [&](const auto& cluster) {
                          query_parser::QueryWordVector find_in_words;
                          for (auto& keyword : cluster.keywords) {
                            // Each `keyword` may itself have multiple terms
                            // that we need to extract and append to
                            // `find_in_words`.
                            query_parser::QueryParser::ExtractQueryWords(
                                base::i18n::ToLower(keyword), &find_in_words);
                          }

                          return query_parser::QueryParser::DoesQueryMatch(
                              find_in_words, query_nodes);
                        });
  return matching_clusters;
}

// Enforces the reverse-chronological invariant of clusters, as well the
// by-score sorting of visits within clusters.
std::vector<history::Cluster> SortClusters(
    std::vector<history::Cluster> clusters) {
  // Within each cluster, sort visits from best to worst using score.
  // TODO(tommycli): Once cluster persistence is done, maybe we can eliminate
  //  this sort step, if they are stored in-order.
  for (auto& cluster : clusters) {
    base::ranges::sort(cluster.scored_annotated_visits, [](auto& v1, auto& v2) {
      if (v1.score != v2.score) {
        // Use v1 > v2 to get higher scored visits BEFORE lower scored visits.
        return v1.score > v2.score;
      }

      // Use v1 > v2 to get more recent visits BEFORE older visits.
      return v1.annotated_visit.visit_row.visit_time >
             v2.annotated_visit.visit_row.visit_time;
    });
  }

  // After that, sort clusters reverse-chronologically based on their highest
  // scored visit.
  base::ranges::sort(clusters, [&](auto& c1, auto& c2) {
    // TODO(tommycli): If we can establish an invariant that no backend will
    //  ever return an empty cluster, we can simplify the below code.
    base::Time c1_time;
    if (!c1.scored_annotated_visits.empty()) {
      c1_time = c1.scored_annotated_visits.front()
                    .annotated_visit.visit_row.visit_time;
    }
    base::Time c2_time;
    if (!c1.scored_annotated_visits.empty()) {
      c2_time = c2.scored_annotated_visits.front()
                    .annotated_visit.visit_row.visit_time;
    }

    // Use c1 > c2 to get more recent clusters BEFORE older clusters.
    return c1_time > c2_time;
  });

  return clusters;
}

}  // namespace

HistoryClustersService::QueryClustersResult::QueryClustersResult() = default;

HistoryClustersService::QueryClustersResult::~QueryClustersResult() = default;

HistoryClustersService::QueryClustersResult::QueryClustersResult(
    const QueryClustersResult&) = default;

HistoryClustersService::HistoryClustersService(
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : history_service_(history_service) {
  DCHECK(history_service_);

#if BUILDFLAG(BUILD_WITH_ON_DEVICE_CLUSTERING_BACKEND)
  if (kUseOnDeviceClusteringBackend.Get()) {
    backend_ = std::make_unique<OnDeviceClusteringBackend>();
  }
#endif

  if (!backend_ && RemoteModelEndpoint().is_valid() && url_loader_factory) {
    backend_ = std::make_unique<RemoteClusteringBackend>(
        url_loader_factory,
        base::BindRepeating(&HistoryClustersService::NotifyDebugMessage,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Create a WeakPtrFactory for calling into the backend.
  // TODO(tommycli): It seems weird that we need both this and `backend_`.
  if (backend_) {
    backend_weak_factory_ =
        std::make_unique<base::WeakPtrFactory<ClusteringBackend>>(
            backend_.get());
  }
}

HistoryClustersService::~HistoryClustersService() = default;

void HistoryClustersService::Shutdown() {}

void HistoryClustersService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void HistoryClustersService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void HistoryClustersService::NotifyDebugMessage(
    const std::string& message) const {
  for (Observer& obs : observers_) {
    obs.OnMemoriesDebugMessage(message);
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
    // If the main kMemories feature is enabled, we want to persist visits.
    // And if the persist-only switch is enabled, we also want to persist them.
    if (base::FeatureList::IsEnabled(kMemories) ||
        base::FeatureList::IsEnabled(kPersistContextAnnotationsInHistoryDb)) {
      history_service_->AddContextAnnotationsForVisit(
          visit_context_annotations.visit_row.visit_id,
          visit_context_annotations.context_annotations);
    }
    incomplete_visit_context_annotations_.erase(nav_id);
  }
}

void HistoryClustersService::QueryClusters(
    const std::string& query,
    base::Time end_time,
    const size_t max_count,
    QueryClustersCallback callback,
    base::CancelableTaskTracker* task_tracker) {
  NotifyDebugMessage("HistoryClustersService::QueryClusters()");
  NotifyDebugMessage("  end_time = " + (end_time.is_null()
                                            ? "null"
                                            : base::TimeToISO8601(end_time)));

  if (!backend_ || !backend_weak_factory_) {
    NotifyDebugMessage(
        "HistoryClustersService::QueryClusters Error: ClusteringBackend is "
        "nullptr. Returning empty cluster vector.");
    std::move(callback).Run({});
    return;
  }

  size_t max_visit_count = kMaxVisitsToCluster.Get();
  if (max_count > 0) {
    // As a primitive heuristic, fetch 3x the amount of visits as requested
    // clusters. We don't know in advance how big the clusters will be.
    max_visit_count = max_count * 3;
  }

  StartOnTheFlyClustering(query, end_time, max_visit_count, std::move(callback),
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
  if (!base::FeatureList::IsEnabled(kMemories))
    return false;

  // 2 hour threshold chosen arbitrarily for cache refresh time.
  if ((base::Time::Now() - all_keywords_cache_timestamp_) >
          base::TimeDelta::FromHours(2) &&
      !cache_query_task_tracker_.HasTrackedTasks()) {
    // TODO(tommycli): Make sure we are hitting the local database, and not the
    //  remote model service once cluster persistence is ready.
    QueryClusters(
        /*query=*/"", /*end_time=*/base::Time(), /*max_count=*/0,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr()),
        &cache_query_task_tracker_);
  }

  // Early exit for short queries after kicking off the populate request.
  if (query.length() <= 3)
    return false;

  // Use `ALWAYS_PREFIX_SEARCH` to avoid flickering the omnibox when typing:
  // "iron " (visible) to "iron s" (not visible) to "iron sto" (visible).
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query),
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH, &query_nodes);

  return query_parser::QueryParser::DoesQueryMatch(all_keywords_cache_,
                                                   query_nodes);
}

void HistoryClustersService::PopulateClusterKeywordCache(
    QueryClustersResult result) {
  all_keywords_cache_.clear();
  for (auto& cluster : result.clusters) {
    for (auto& keyword : cluster.keywords) {
      // Each `keyword` may itself have multiple terms that we need to extract.
      query_parser::QueryParser::ExtractQueryWords(base::i18n::ToLower(keyword),
                                                   &all_keywords_cache_);
    }
  }

  all_keywords_cache_timestamp_ = base::Time::Now();
}

void HistoryClustersService::StartOnTheFlyClustering(
    const std::string& query,
    base::Time end_time,
    size_t max_visit_count,
    QueryClustersCallback callback,
    base::CancelableTaskTracker* task_tracker) const {
  DCHECK(history_service_);

  history::QueryOptions options;
  options.end_time = end_time;

  // Super simple method of pagination: one day a time, broken up at 4AM.
  {
    // Get 4AM yesterday in the morning, and 4AM today in the afternoon.
    base::Time begin = end_time.is_null() ? base::Time::Now() : end_time;
    begin -= base::TimeDelta::FromHours(12);

    base::Time::Exploded exploded_begin;
    begin.LocalExplode(&exploded_begin);
    exploded_begin.hour = 4;
    exploded_begin.minute = 0;
    exploded_begin.second = 0;
    exploded_begin.millisecond = 0;

    // If for some reason this fails, fallback to 24 hours ago.
    if (!base::Time::FromLocalExploded(exploded_begin, &options.begin_time)) {
      options.begin_time = end_time - base::TimeDelta::FromDays(1);
    }
  }

  NotifyDebugMessage("HistoryClustersService::StartOnTheFlyClustering()");
  NotifyDebugMessage("  begin_time = " +
                     (options.begin_time.is_null()
                          ? "null"
                          : base::TimeToISO8601(options.begin_time)));
  NotifyDebugMessage("  end_time = " +
                     (options.end_time.is_null()
                          ? "null"
                          : base::TimeToISO8601(options.end_time)));
  history_service_->GetAnnotatedVisits(
      options,
      base::BindOnce(&HistoryClustersService::OnGotHistoryVisits,
                     weak_ptr_factory_.GetWeakPtr(), query,
                     /*original_end_time=*/end_time, max_visit_count,
                     std::move(callback), options, task_tracker,
                     std::vector<history::AnnotatedVisit>()),
      task_tracker);
}

void HistoryClustersService::OnGotHistoryVisits(
    const std::string& query,
    base::Time original_end_time,
    size_t max_visit_count,
    QueryClustersCallback callback,
    history::QueryOptions options,
    base::CancelableTaskTracker* task_tracker,
    std::vector<history::AnnotatedVisit> accumulated_visits,
    std::vector<history::AnnotatedVisit> newly_fetched_visits) const {
  NotifyDebugMessage("HistoryClustersService::OnGotHistoryVisits()");
  NotifyDebugMessage(base::StringPrintf("  newly_fetched_visits.size() = %zu",
                                        newly_fetched_visits.size()));

  // Tack on all the newly fetched visits onto our accumulator vector.
  base::ranges::move(newly_fetched_visits,
                     std::back_inserter(accumulated_visits));

  // TODO(tommycli): Connect this to History's limit defined internally in
  // components/history.
  bool exhausted_history =
      (base::Time::Now() - options.begin_time) >= base::TimeDelta::FromDays(90);
  NotifyDebugMessage(
      base::StringPrintf("  exhausted_history = %d", exhausted_history));

  // If we didn't get enough visits, ask for another day's worth from
  // History and call this method again when done.
  if (!exhausted_history && accumulated_visits.size() < max_visit_count) {
    options.end_time = options.begin_time;
    options.begin_time = options.end_time - base::TimeDelta::FromDays(1);

    NotifyDebugMessage("Starting History Query:");
    NotifyDebugMessage("  begin_time = " +
                       (options.begin_time.is_null()
                            ? "null"
                            : base::TimeToISO8601(options.begin_time)));
    NotifyDebugMessage("  end_time = " +
                       (options.end_time.is_null()
                            ? "null"
                            : base::TimeToISO8601(options.end_time)));
    history_service_->GetAnnotatedVisits(
        options,
        base::BindOnce(&HistoryClustersService::OnGotHistoryVisits,
                       weak_ptr_factory_.GetWeakPtr(), query, original_end_time,
                       max_visit_count, std::move(callback), options,
                       task_tracker, std::move(accumulated_visits)),
        task_tracker);
    return;
  }

  // Assuming we didn't completely exhaust history, the `continuation_end_time`
  // is the `options.begin_time` of the latest History query we completed.
  base::Time continuation_end_time;
  if (!exhausted_history)
    continuation_end_time = options.begin_time;

  // Now we have enough visits for clustering, add all incomplete visits between
  // the current `options.begin_time` and `original_end_time`, as otherwise they
  // will be mysteriously missing from the Clusters UI. They haven't recorded
  // the page end metrics yet, but that's fine.
  for (const auto& item : incomplete_visit_context_annotations_) {
    auto& incomplete_visit = item.second;
    if (incomplete_visit.url_row.id() == 0 ||
        incomplete_visit.visit_row.visit_id == 0) {
      // Discard incomplete visits that don't have visit_ids yet.
      continue;
    }

    const auto& visit_time = incomplete_visit.visit_row.visit_time;
    if (visit_time < options.begin_time ||
        (!original_end_time.is_null() && visit_time >= original_end_time)) {
      // Discard incomplete visits outside the `options` time bounds.
      // `begin_time` is inclusive, and `end_time` is exclusive.
      continue;
    }

    accumulated_visits.push_back({
        incomplete_visit.url_row,
        incomplete_visit.visit_row,
        incomplete_visit.context_annotations,
        // TODO(tommycli): Add content annotations.
        {},
    });
  }

  NotifyDebugMessage("Calling backend_->GetClusters()");
  backend_->GetClusters(
      base::BindOnce(
          [](const std::string& query, base::Time continuation_end_time,
             QueryClustersCallback callback,
             const std::vector<history::Cluster>& clusters) {
            HistoryClustersService::QueryClustersResult result;
            result.continuation_end_time = continuation_end_time;
            result.clusters = FilterClustersMatchingQuery(query, clusters);
            result.clusters = SortClusters(result.clusters);
            std::move(callback).Run(std::move(result));
          },
          query, continuation_end_time, std::move(callback)),
      accumulated_visits);
}

}  // namespace history_clusters
