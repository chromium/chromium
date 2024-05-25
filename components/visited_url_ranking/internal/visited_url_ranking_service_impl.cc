// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"

#include <map>
#include <memory>
#include <queue>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

using segmentation_platform::AnnotatedNumericResult;
using segmentation_platform::InputContext;
using segmentation_platform::PredictionOptions;
using segmentation_platform::PredictionStatus;
using segmentation_platform::processing::ProcessedValue;
using visited_url_ranking::URLVisit;

namespace visited_url_ranking {

namespace {
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

void SortScoredAggregatesAndCallback(
    std::vector<URLVisitAggregate> scored_visits,
    VisitedURLRankingService::RankURLVisitAggregatesCallback callback) {
  base::ranges::stable_sort(scored_visits, [](const auto& c1, const auto& c2) {
    // Sort such that higher scored entries precede lower scored entries.
    return c1.score > c2.score;
  });
  std::move(callback).Run(ResultStatus::kSuccess, std::move(scored_visits));
}

}  // namespace

VisitedURLRankingServiceImpl::VisitedURLRankingServiceImpl(
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers,
    std::map<URLVisitAggregatesTransformType,
             std::unique_ptr<URLVisitAggregatesTransformer>> transformers)
    : segmentation_platform_service_(segmentation_platform_service),
      data_fetchers_(std::move(data_fetchers)),
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
    std::vector<URLVisitAggregate> visit_aggregates,
    RankURLVisitAggregatesCallback callback) {
  if (visit_aggregates.empty()) {
    std::move(callback).Run(ResultStatus::kSuccess, {});
    return;
  }

  if (!segmentation_platform_service_) {
    std::move(callback).Run(ResultStatus::kError, {});
    return;
  }

  std::deque<URLVisitAggregate> visits_queue;
  for (auto& visit : visit_aggregates) {
    visits_queue.push_back(std::move(visit));
  }
  visit_aggregates.clear();

  GetNextResult(config.key, std::move(visits_queue), {}, std::move(callback));
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

void VisitedURLRankingServiceImpl::GetNextResult(
    const std::string& segmentation_key,
    std::deque<URLVisitAggregate> visit_aggregates,
    std::vector<URLVisitAggregate> scored_visits,
    RankURLVisitAggregatesCallback callback) {
  if (visit_aggregates.empty()) {
    SortScoredAggregatesAndCallback(std::move(scored_visits),
                                    std::move(callback));
    return;
  }

  PredictionOptions options;
  options.on_demand_execution = true;
  scoped_refptr<InputContext> input_context =
      AsInputContext(kURLVisitAggregateSchema, visit_aggregates.front());
  segmentation_platform_service_->GetAnnotatedNumericResult(
      segmentation_key, options, input_context,
      base::BindOnce(&VisitedURLRankingServiceImpl::OnGetResult,
                     weak_ptr_factory_.GetWeakPtr(), segmentation_key,
                     std::move(visit_aggregates), std::move(scored_visits),
                     std::move(callback)));
}

void VisitedURLRankingServiceImpl::OnGetResult(
    const std::string& segmentation_key,
    std::deque<URLVisitAggregate> visit_aggregates,
    std::vector<URLVisitAggregate> scored_visits,
    RankURLVisitAggregatesCallback callback,
    const AnnotatedNumericResult& result) {
  float model_score = -1;
  if (result.status == PredictionStatus::kSucceeded) {
    model_score = *result.GetResultForLabel(segmentation_key);
  }
  auto visit = std::move(visit_aggregates.front());
  visit.score = model_score;
  visit_aggregates.pop_front();
  scored_visits.emplace_back(std::move(visit));

  GetNextResult(segmentation_key, std::move(visit_aggregates),
                std::move(scored_visits), std::move(callback));
}

}  // namespace visited_url_ranking
