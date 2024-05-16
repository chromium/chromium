// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"

#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace visited_url_ranking {

namespace {

base::Time GetVisitTime(const URLVisitAggregate::URLVisitVariant& visit) {
  const URLVisitAggregate::TabData* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&visit);
  if (tab_data) {
    return tab_data->last_active;
  }

  const URLVisitAggregate::HistoryData* history_data =
      std::get_if<URLVisitAggregate::HistoryData>(&visit);
  if (history_data) {
    return history_data->last_visited.visit_row.visit_time;
  }

  return base::Time::Max();
}

}  // namespace

// Combines `URLVisitVariant` data obtained from various fetchers into
// `URLVisitAggregate` objects. Leverages the `URLMergeKey` in order to
// reconcile what data belongs to the same aggregate object.
std::vector<URLVisitAggregate> ComputeURLVisitAggregates(
    std::vector<FetchResult> fetch_results) {
  std::map<URLMergeKey, URLVisitAggregate> url_visit_map = {};
  for (FetchResult& result : fetch_results) {
    if (result.status != FetchResult::Status::kSuccess) {
      // TODO(romanarora): Capture a metric for how often a specific fetcher
      // failed.
      continue;
    }

    for (std::pair<const URLMergeKey, URLVisitAggregate::URLVisitVariant>&
             url_data : result.data) {
      URLVisitAggregate& aggregate = url_visit_map[url_data.first];
      std::visit(
          URLVisitVariantHelper{
              [&aggregate](URLVisitAggregate::TabData& tab_data) {
                aggregate.fetcher_data_map.emplace(
                    tab_data.last_active_tab.session_name.has_value()
                        ? Fetcher::kSession
                        : Fetcher::kTabModel,
                    std::move(tab_data));
              },
              [&aggregate](URLVisitAggregate::HistoryData& history_data) {
                aggregate.fetcher_data_map.emplace(Fetcher::kHistory,
                                                   std::move(history_data));
              }},
          url_data.second);
    }
  }

  std::vector<URLVisitAggregate> url_visits;
  url_visits.reserve(url_visit_map.size());
  for (auto& url_visit_pair : url_visit_map) {
    url_visits.push_back(std::move(url_visit_pair.second));
  }
  url_visit_map.clear();

  return url_visits;
}

VisitedURLRankingServiceImpl::VisitedURLRankingServiceImpl(
    std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers,
    std::map<URLVisitAggregatesTransformType,
             std::unique_ptr<URLVisitAggregatesTransformer>> transformers)
    : data_fetchers_(std::move(data_fetchers)),
      transformers_(std::move(transformers)) {}

VisitedURLRankingServiceImpl::~VisitedURLRankingServiceImpl() = default;

void VisitedURLRankingServiceImpl::FetchURLVisitAggregates(
    const FetchOptions& options,
    GetURLVisitAggregatesCallback callback) {
  auto merge_visits_and_callback = base::BindOnce(
      &VisitedURLRankingServiceImpl::MergeVisitsAndCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), options.transforms);

  const auto fetch_barrier_callback = base::BarrierCallback<FetchResult>(
      options.fetcher_sources.size(), std::move(merge_visits_and_callback));

  for (const auto& fetcher_entry : options.fetcher_sources) {
    const auto& data_fetcher = data_fetchers_.at(fetcher_entry.first);
    data_fetcher->FetchURLVisitData(options, fetch_barrier_callback);
  }
}

void VisitedURLRankingServiceImpl::RankURLVisitAggregates(
    const Config& config,
    std::vector<URLVisitAggregate> visits,
    RankVisitAggregatesCallback callback) {
  // TODO(crbug.com/330577142): Implement `URLVisitAggregate` ranking logic.

  // To enable development and testing, below we implement a stub implementation
  // that simply sorts |visits| by minimal |last_active|.
  size_t num_visits = visits.size();

  // Extract sort keys (|fetcher_data_map| may have any size) and create sort
  // permutation, which also avoids object movement churn from direct sort.
  std::vector<std::pair<base::Time, size_t>> keys;
  keys.reserve(num_visits);
  for (size_t i = 0; i < num_visits; ++i) {
    const auto& aggregate = visits[i];
    base::Time min_last_visit_time = base::Time::Max();
    for (auto& key_value : aggregate.fetcher_data_map) {
      min_last_visit_time =
          std::min(min_last_visit_time, GetVisitTime(key_value.second));
    }
    keys.emplace_back(min_last_visit_time, i);
  }
  // Sort from oldest to newest.
  std::sort(keys.begin(), keys.end());

  // Apply permutation, and reverse, so newest comes first.
  std::vector<URLVisitAggregate> ranked_visits(num_visits);
  for (size_t i = 0; i < num_visits; ++i) {
    ranked_visits[num_visits - 1 - i] = std::move(visits[keys[i].second]);
  }
  std::move(callback).Run(ResultStatus::kSuccess, std::move(ranked_visits));
}

void VisitedURLRankingServiceImpl::MergeVisitsAndCallback(
    GetURLVisitAggregatesCallback callback,
    const std::vector<URLVisitAggregatesTransformType>& ordered_transforms,
    std::vector<FetchResult> fetcher_results) {
  std::queue<URLVisitAggregatesTransformType> transform_type_queue;
  for (const auto& transform_type : ordered_transforms) {
    transform_type_queue.push(transform_type);
  }

  TransformVisitsAndCallback(
      std::move(callback), std::move(transform_type_queue),
      URLVisitAggregatesTransformer::Status::kSuccess,
      ComputeURLVisitAggregates(std::move(fetcher_results)));
}

void VisitedURLRankingServiceImpl::TransformVisitsAndCallback(
    GetURLVisitAggregatesCallback callback,
    std::queue<URLVisitAggregatesTransformType> transform_type_queue,
    URLVisitAggregatesTransformer::Status status,
    std::vector<URLVisitAggregate> aggregates) {
  if (status == URLVisitAggregatesTransformer::Status::kError) {
    std::move(callback).Run(ResultStatus::kError, {});
    return;
  }

  if (transform_type_queue.empty() || aggregates.empty()) {
    std::move(callback).Run(ResultStatus::kSuccess, std::move(aggregates));
    return;
  }

  auto transform_type = transform_type_queue.front();
  transform_type_queue.pop();
  const auto it = transformers_.find(transform_type);
  if (it == transformers_.end()) {
    std::move(callback).Run(ResultStatus::kError, {});
    return;
  }
  it->second->Transform(
      std::move(aggregates),
      base::BindOnce(&VisitedURLRankingServiceImpl::TransformVisitsAndCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(transform_type_queue)));
}

}  // namespace visited_url_ranking
