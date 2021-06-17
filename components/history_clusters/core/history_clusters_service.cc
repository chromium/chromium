// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/remote_clustering_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history_clusters {

namespace {

// Filter `clusters` matching `query`. There are additional filters (e.g.
// `recency_threshold`) used when requesting `QueryMemories()`, but this
// function is only responsible for matching `query`.
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

// TODO(manukh): Move mojom translation to `HistoryClustersHandler` once we've
// created
//  a mirror cpp struct the `MemoryService` can return instead. There's no need
//  to do this yet, since the `HistoryClustersHandler` is the only consumer of
//  `HistoryClustersService`, but once the omnibox comes into play, we'll need a
//  common non-mojom response.
// TODO(crbug.com/1179069): fill out the remaining Memories mojom fields.
// Translate a `AnnotatedVisit` to `mojom::VisitPtr`.
history_clusters::mojom::URLVisitPtr VisitToMojom(
    const history::AnnotatedVisit& visit) {
  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->id = visit.visit_row.visit_id;
  visit_mojom->url = visit.url_row.url();
  visit_mojom->time = visit.visit_row.visit_time;
  visit_mojom->page_title = base::UTF16ToUTF8(visit.url_row.title());
  return visit_mojom;
}

// Translate a vector of `Cluster`s to a vector of `mojom::ClusterPtr`s.
std::vector<history_clusters::mojom::ClusterPtr> ClustersToMojom(
    const std::vector<history::Cluster>& clusters) {
  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  for (const auto& cluster : clusters) {
    auto cluster_mojom = history_clusters::mojom::Cluster::New();
    cluster_mojom->id = cluster.cluster_id;
    for (const auto& keyword : cluster.keywords)
      cluster_mojom->keywords.push_back(keyword);
    for (const auto& visit : cluster.annotated_visits)
      cluster_mojom->visits.push_back(VisitToMojom(visit));
    clusters_mojom.emplace_back(std::move(cluster_mojom));
  }
  return clusters_mojom;
}

// Form a `QueryMemoriesResponse` containing `clusters` and continuation query
// params meant to be used in a follow-up request. `query_params` are the params
// used to get `clusters` from `QueryClusters()`.
// TODO(tommycli): At the moment, the recency threshold of `query_params` is
// ignored and continuation query params is set to nullptr. The service does
// not support paging.
HistoryClustersService::QueryMemoriesResponse FormQueryMemoriesResponse(
    mojom::QueryParamsPtr query_params,
    const std::vector<history::Cluster>& clusters) {
  return {nullptr, ClustersToMojom(clusters)};
}

}  // namespace

HistoryClustersService::QueryMemoriesResponse::QueryMemoriesResponse(
    mojom::QueryParamsPtr query_params,
    std::vector<mojom::ClusterPtr> clusters)
    : query_params(std::move(query_params)), clusters(std::move(clusters)) {}

HistoryClustersService::QueryMemoriesResponse::QueryMemoriesResponse(
    QueryMemoriesResponse&& other) = default;

HistoryClustersService::QueryMemoriesResponse::~QueryMemoriesResponse() =
    default;

HistoryClustersService::HistoryClustersService(
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : history_service_(history_service) {
  backend_ = std::make_unique<RemoteClusteringBackend>(
      url_loader_factory,
      base::BindRepeating(&HistoryClustersService::NotifyDebugMessage,
                          weak_ptr_factory_.GetWeakPtr()));
  backend_weak_factory_ =
      std::make_unique<base::WeakPtrFactory<ClusteringBackend>>(backend_.get());
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

void HistoryClustersService::QueryMemories(
    mojom::QueryParamsPtr query_params,
    base::OnceCallback<void(QueryMemoriesResponse)> callback,
    base::CancelableTaskTracker* task_tracker) {
  // `QueryMemories` has 4 steps:
  // 1. Get visits either asynchronously from the history db or synchronously
  //    from `visits_`.
  // 2. Ask `backend_` to convert the visits to memories.
  // 3. Filter memories matching `query_params` and create.
  // 4. Run `callback` with the continuation query params and matched memories.

  // Copy `query_params->query` because `query_params` is about to be moved.
  auto query_string = query_params->query;
  auto on_visits_callback = base::BindOnce(
      &ClusteringBackend::GetClusters, backend_weak_factory_->GetWeakPtr(),
      base::BindOnce(&FilterClustersMatchingQuery, query_string)
          .Then(base::BindOnce(&FormQueryMemoriesResponse,
                               std::move(query_params)))
          .Then(std::move(callback)));

  history_service_->GetAnnotatedVisits(
      kMaxVisitsToCluster.Get(),
      base::BindOnce(
          // This echo callback is necessary to copy the `AnnotatedVisit`
          // refs.
          [](std::vector<history::AnnotatedVisit> visits) { return visits; })
          .Then(std::move(on_visits_callback)),
      task_tracker);
}

void HistoryClustersService::RemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list,
    base::OnceClosure closure,
    base::CancelableTaskTracker* task_tracker) {
  std::move(closure).Run();
  // TODO(crbug.com/1203789): Remove the visits from relevant history tables.
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
    QueryMemories(
        mojom::QueryParams::New(),
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
    QueryMemoriesResponse response) {
  all_keywords_cache_.clear();
  for (auto& cluster : response.clusters) {
    for (auto& keyword : cluster->keywords) {
      // Each `keyword` may itself have multiple terms that we need to extract.
      query_parser::QueryParser::ExtractQueryWords(base::i18n::ToLower(keyword),
                                                   &all_keywords_cache_);
    }
  }

  all_keywords_cache_timestamp_ = base::Time::Now();
}

}  // namespace history_clusters
