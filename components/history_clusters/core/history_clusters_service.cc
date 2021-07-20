// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
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

using AnnotatedVisitCallback =
    base::OnceCallback<void(const std::vector<history::AnnotatedVisit>&)>;

// Gets `AnnotatedVisit`s to cluster including both persisted visits from the
// history db and incomplete visits.
// - We don't want incomplete visits to be mysteriously missing from the
// Clusters UI. They haven't recorded the page end metrics yet, but that's fine.
// - The history backend will return persisted visits with already computed
// `referring_visit_of_redirect_chain_start`, while incomplete visits will have
// to independently invoke `GetRedirectChainStart()` to get it.
class GetAnnotatedVisitsToCluster : public history::HistoryDBTask {
 public:
  GetAnnotatedVisitsToCluster(history::QueryOptions options,
                              std::vector<IncompleteVisitContextAnnotations>
                                  incomplete_visit_context_annotations,
                              AnnotatedVisitCallback callback)
      : options_(options),
        incomplete_visit_context_annotations_(
            incomplete_visit_context_annotations),
        callback_(std::move(callback)) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Get persisted visits, which will have
    // `referring_visit_of_redirect_chain_start` computed.
    annotated_visits_ = backend->GetAnnotatedVisits(options_);

    // Compute `referring_visit_of_redirect_chain_start` for each incomplete
    // visit.
    base::ranges::transform(
        incomplete_visit_context_annotations_,
        std::back_inserter(annotated_visits_),
        [&](const auto& incomplete_visit_context_annotations) {
          const auto& first_redirect = backend->GetRedirectChainStart(
              incomplete_visit_context_annotations.visit_row);
          return history::AnnotatedVisit{
              incomplete_visit_context_annotations.url_row,
              incomplete_visit_context_annotations.visit_row,
              incomplete_visit_context_annotations.context_annotations,
              // Content annotations not provided, but it's not provided for
              // complete visits either.
              {},
              first_redirect.referring_visit,
          };
        });
    return true;
  }

  void DoneRunOnMainThread() override {
    std::move(callback_).Run(annotated_visits_);
  }

 private:
  history::QueryOptions options_;
  // Incomplete visits not yet persisted passed in in the constructor.
  std::vector<IncompleteVisitContextAnnotations>
      incomplete_visit_context_annotations_;
  // Persisted visits retrieved from the history DB thread.
  std::vector<history::AnnotatedVisit> annotated_visits_;
  AnnotatedVisitCallback callback_;
};

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

HistoryClustersService::QueryClustersResult MakeQueryClustersResult(
    std::vector<history::Cluster> clusters) {
  HistoryClustersService::QueryClustersResult result;
  result.clusters = std::move(clusters);
  // TODO(tommycli): Fill `continuation_end_time` once pagination done.
  return result;
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
  // `QueryClusters` has 5 steps:
  // 1. Filters incomplete visits.
  // 2. Prepend persisted visits from the history DB.
  // 3. Ask `backend_` to convert the visits to clusters.
  // 4. Filter clusters matching the query params.
  // 5. Run `callback` with the continuation query params and matched clusters.

  NotifyDebugMessage("HistoryClustersService::QueryClusters()");

  if (!backend_ || !backend_weak_factory_) {
    NotifyDebugMessage(
        "HistoryClustersService::QueryClusters Error: ClusteringBackend is "
        "nullptr. Returning empty cluster vector.");
    std::move(callback).Run({});
    return;
  }

  // TODO(crbug.com/1220765): Fully support pagination using `end_time` and
  //  `max_count`.
  auto on_visits_callback = base::BindOnce(
      &ClusteringBackend::GetClusters, backend_weak_factory_->GetWeakPtr(),
      base::BindOnce(&FilterClustersMatchingQuery, query)
          .Then(base::BindOnce(&SortClusters))
          .Then(base::BindOnce(&MakeQueryClustersResult))
          .Then(std::move(callback)));

  // TODO(tommycli): Support pagination by setting `begin_time` on `options`.
  history::QueryOptions options;
  options.max_count = kMaxVisitsToCluster.Get();

  // Filter incomplete visits to those that have a `url_row`, have a
  // `visit_row`, and match `options`.
  std::vector<IncompleteVisitContextAnnotations>
      filtered_incomplete_visit_context_annotations;
  for (const auto& item : incomplete_visit_context_annotations_) {
    auto& incomplete_visit_context_annotation = item.second;
    // Discard incomplete visits that don't have a `url_row` and `visit_row`.
    // It's possible that the `url_row` and `visit_row` will be available
    // before they're needed (i.e. before
    // `GetAnnotatedVisitsToCluster::RunOnDBThread()`). But since it'll only
    // have a copy of the incomplete context annotations, the copy won't have
    // the fields updated. A weak ptr won't help since it can't be accessed on
    // different threads. A `scoped_refptr` could work. However, only very
    // recently opened tabs won't have the rows set, so we don't bother using
    // `scoped_refptr`s.
    if (!incomplete_visit_context_annotation.status.history_rows)
      continue;

    // Discard incomplete visits are ooutside the `options` time bounds.
    // `begin_time` is inclusive, and `end_time` is exclusive.
    // TODO(manukh): `end_time` is intended for the WebUI pagination and should
    //  not affect which visits are clustered. Once we have a feature param to
    //  toggle on-the-fly clustering, we should move the `end_time` check to
    //  `GetClusters()` when using persisted clusters.
    const auto& visit_time =
        incomplete_visit_context_annotation.visit_row.visit_time;
    if (visit_time < options.begin_time ||
        (!options.end_time.is_null() && visit_time >= options.end_time)) {
      continue;
    }

    filtered_incomplete_visit_context_annotations.push_back(
        incomplete_visit_context_annotation);
  }

  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<GetAnnotatedVisitsToCluster>(
          options, filtered_incomplete_visit_context_annotations,
          std::move(on_visits_callback)),
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

}  // namespace history_clusters
