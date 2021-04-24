// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_service.h"

#include <numeric>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/memories_remote_model_helper.h"
#include "components/query_parser/query_parser.h"

namespace history_clusters {

MemoriesService::MemoriesService(
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Can't do this in initialization list, because |weak_ptr_factory_| usage.
  // TODO(tommycli): Investigate if we can simplify some lifetime issues with
  // using free-lambdas within |remote_model_helper_|.
  remote_model_helper_ = std::make_unique<MemoriesRemoteModelHelper>(
      url_loader_factory,
      base::BindRepeating(&MemoriesService::NotifyDebugMessage,
                          weak_ptr_factory_.GetWeakPtr()));
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

MemoriesVisit& MemoriesService::GetIncompleteVisit(int64_t nav_id) {
  DCHECK(HasIncompleteVisit(nav_id));
  return GetOrCreateIncompleteVisit(nav_id);
}

MemoriesVisit& MemoriesService::GetOrCreateIncompleteVisit(int64_t nav_id) {
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
    if (base::FeatureList::IsEnabled(history_clusters::kMemories))
      visits_.push_back(visit);
    incomplete_visits_.erase(nav_id);
    // TODO(tommycli/manukh): Persist |visits_| to History, and take out of
    //  in-memory.
  }
}

void MemoriesService::QueryMemories(mojom::QueryParamsPtr query_params,
                                    QueryMemoriesCallback callback) {
  remote_model_helper_->GetMemories(
      visits_, base::BindOnce(&MemoriesService::OnQueryMemoriesResult,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(query_params), std::move(callback)));
}

void MemoriesService::OnQueryMemoriesResult(
    mojom::QueryParamsPtr query_params,
    QueryMemoriesCallback callback,
    std::vector<mojom::MemoryPtr> memories) {
  if (query_params->query.empty()) {
    std::move(callback).Run(nullptr, std::move(memories));
    return;
  }

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
        // Combine lowercase keywords into a string to extract query words from.
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
  std::move(callback).Run(nullptr, std::move(matching_memories));
}

}  // namespace history_clusters
