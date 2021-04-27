// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_service.h"

#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/query_parser/query_parser.h"

namespace history_clusters {

namespace {

// Uses the bound |query_params| parameter to filter |memories|. Returns matched
// memories and continuation query params meant to be used in a follow-up
// request to get older memories.
// TODO(mahmadi): At the moment, the recency threshold of |query_params| is
//  ignored and |callback| is invoked with nullptr continuation query params as
//  the service does not support paging.
std::pair<mojom::QueryParamsPtr, Memories> FilterMemoriesMatchingQuery(
    mojom::QueryParamsPtr query_params,
    std::vector<mojom::MemoryPtr> memories) {
  if (query_params->query.empty())
    return {nullptr, std::move(memories)};

  // Extract query nodes from the query string.
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query_params->query),
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH, &query_nodes);

  std::vector<mojom::MemoryPtr> matching_memories;
  std::copy_if(
      std::make_move_iterator(memories.begin()),
      std::make_move_iterator(memories.end()),
      std::back_inserter(matching_memories), [&](const auto& memory) {
        // Combine lowercase keywords into a string to extract query
        // words from.
        std::u16string keywords = std::accumulate(
            memory->keywords.begin(), memory->keywords.end(), std::u16string(),
            [](std::u16string accumulated, std::u16string str) {
              return accumulated + u" " + str;
            });
        query_parser::QueryWordVector query_words;
        query_parser::QueryParser::ExtractQueryWords(keywords, &query_words);
        return query_parser::QueryParser::DoesQueryMatch(query_words,
                                                         query_nodes);
      });
  return {nullptr, std::move(matching_memories)};
}

}  // namespace

MemoriesService::MemoriesService(
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : history_service_(history_service) {
  // Can't do this in initialization list, because |weak_ptr_factory_| usage.
  // TODO(tommycli): Investigate if we can simplify some lifetime issues with
  // using free-lambdas within |remote_model_helper_|.
  remote_model_helper_ = std::make_unique<MemoriesRemoteModelHelper>(
      url_loader_factory,
      base::BindRepeating(&MemoriesService::NotifyDebugMessage,
                          weak_ptr_factory_.GetWeakPtr()));
  remote_model_helper_weak_factory_ =
      std::make_unique<base::WeakPtrFactory<MemoriesRemoteModelHelper>>(
          remote_model_helper_.get());
}

MemoriesService::~MemoriesService() = default;

void MemoriesService::Shutdown() {}

void MemoriesService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void MemoriesService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void MemoriesService::NotifyDebugMessage(const std::string& message) const {
  for (Observer& obs : observers_) {
    obs.OnMemoriesDebugMessage(message);
  }
}

IncompleteVisit& MemoriesService::GetIncompleteVisit(int64_t nav_id) {
  DCHECK(HasIncompleteVisit(nav_id));
  return GetOrCreateIncompleteVisit(nav_id);
}

IncompleteVisit& MemoriesService::GetOrCreateIncompleteVisit(int64_t nav_id) {
  return incomplete_visits_[nav_id];
}

bool MemoriesService::HasIncompleteVisit(int64_t nav_id) {
  return incomplete_visits_.count(nav_id);
}

void MemoriesService::CompleteVisitIfReady(int64_t nav_id) {
  auto& visit = GetIncompleteVisit(nav_id);
  DCHECK((visit.status.history_rows && visit.status.navigation_ended) ||
         !visit.status.navigation_end_signals);
  DCHECK(visit.status.expect_ukm_page_end_signals ||
         !visit.status.ukm_page_end_signals);
  if (visit.status.history_rows && visit.status.navigation_end_signals &&
      (visit.status.ukm_page_end_signals ||
       !visit.status.expect_ukm_page_end_signals)) {
    if (base::FeatureList::IsEnabled(kMemories)) {
      if (StoreVisitsInHistoryDb())
        history_service_->AddClusterVisit(history::ClusterVisitRow(visit));
      else
        visits_.push_back(visit);
    }
    incomplete_visits_.erase(nav_id);
  }
}

void MemoriesService::QueryMemories(mojom::QueryParamsPtr query_params,
                                    QueryMemoriesCallback callback) {
  // |QueryMemories| has 4 steps:
  // 1. Get visits either asynchronously from the history db or synchronously
  //    from |visits_|.
  // 2. Ask |remote_model_helper_| to convert the visits to memories.
  // 3. Filter memories matching |query_params| and create.
  // 4. Run |callback| with the continuation query params and matched memories.

  auto on_visits_callback = base::BindOnce(
      &MemoriesRemoteModelHelper::GetMemories,
      remote_model_helper_weak_factory_->GetWeakPtr(),
      base::BindOnce(&FilterMemoriesMatchingQuery, std::move(query_params))
          .Then(base::BindOnce(
              [](QueryMemoriesCallback callback,
                 std::pair<mojom::QueryParamsPtr, Memories> pair) {
                std::move(callback).Run(std::move(pair.first),
                                        std::move(pair.second));
              },
              std::move(callback))));

  if (StoreVisitsInHistoryDb()) {
    history_service_->GetClusterVisits(
        MaxVisitsToCluster(),
        base::BindOnce(
            // This echo callback is necessary to copy the |ClusterVisit| refs.
            [](std::vector<history::ClusterVisit> visits) { return visits; })
            .Then(std::move(on_visits_callback)),
        &task_tracker_);
  } else
    std::move(on_visits_callback).Run(visits_);
}

}  // namespace history_clusters
