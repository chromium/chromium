// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/decoration.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
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

// Default sampling rate for kSeen events recording. 1 in
// `kSeenRecordsSamplingRate` events are recorded randomly.
constexpr int kSeenRecordsSamplingRate = 1;

const char* EventNameForAction(ScoredURLUserAction action) {
  switch (action) {
    case kSeen:
      return kURLVisitSeenEventName;
    case kActivated:
      return kURLVisitActivatedEventName;
    case kDismissed:
      return kURLVisitDismissedEventName;
    default:
      NOTREACHED();
  }
}

// Update URLVisitAggregatesTransformType in tools/metrics/histograms
// /metadata/visited_url_ranking/histogram.xml for them to be in sync.
const char* URLVisitAggregatesTransformTypeName(
    URLVisitAggregatesTransformType type) {
  switch (type) {
    case URLVisitAggregatesTransformType::kUnspecified:
      return "Unspecified";
    case URLVisitAggregatesTransformType::kBookmarkData:
      return "BookmarkData";
    case URLVisitAggregatesTransformType::kShoppingData:
      return "ShoppingData";
    case URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter:
      return "HistoryVisibilityScoreFilter";
    case URLVisitAggregatesTransformType::kHistoryCategoriesFilter:
      return "HistoryCategoriesFilter";
    case URLVisitAggregatesTransformType::kDefaultAppUrlFilter:
      return "DefaultAppUrlFilter";
    case URLVisitAggregatesTransformType::kRecencyFilter:
      return "RecencyFilter";
    case URLVisitAggregatesTransformType::kSegmentationMetricsData:
      return "SegmentationMetricsData";
    case URLVisitAggregatesTransformType::kHistoryBrowserTypeFilter:
      return "HistoryBrowserTypeFilter";
  }
}

const char* URLVisitAggregatesFetcherName(Fetcher fetcher) {
  switch (fetcher) {
    case Fetcher::kTabModel:
      return "TabModel";
    case Fetcher::kSession:
      return "Session";
    case Fetcher::kHistory:
      return "History";
  }
}

// Combines `URLVisitVariant` data obtained from various fetchers into
// `URLVisitAggregate` objects. Leverages the `URLMergeKey` in order to
// reconcile what data belongs to the same aggregate object.
std::pair<std::vector<URLVisitAggregate>, URLVisitsMetadata>
ComputeURLVisitAggregates(
    std::vector<std::pair<Fetcher, FetchResult>> fetcher_results) {
  std::map<URLMergeKey, URLVisitAggregate> url_visit_map = {};
  for (auto& result_pair : fetcher_results) {
    FetchResult& result = result_pair.second;
    base::UmaHistogramEnumeration(
        "VisitedURLRanking.Request.Step.Fetch.Status",
        result.status == FetchResult::Status::kSuccess
            ? VisitedURLRankingRequestStepStatus::kSuccess
            : VisitedURLRankingRequestStepStatus::kFailed);
    base::UmaHistogramBoolean(
        base::StringPrintf("VisitedURLRanking.Fetch.%s.Success",
                           URLVisitAggregatesFetcherName(result_pair.first)),
        result.status == FetchResult::Status::kSuccess);

    if (result.status != FetchResult::Status::kSuccess) {
      continue;
    }

    for (std::pair<const URLMergeKey, URLVisitAggregate::URLVisitVariant>&
             url_data : result.data) {
      if (url_visit_map.find(url_data.first) == url_visit_map.end()) {
        url_visit_map.emplace(url_data.first,
                              URLVisitAggregate(url_data.first));
      }

      URLVisitAggregate& aggregate = url_visit_map.at(url_data.first);
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
  URLVisitsMetadata url_visits_metadata;
  url_visits_metadata.aggregates_count_before_transforms = url_visit_map.size();
  url_visits.reserve(url_visit_map.size());
  for (auto& url_visit_pair : url_visit_map) {
    if (!url_visits_metadata.most_recent_timestamp.has_value() ||
        url_visits_metadata.most_recent_timestamp <
            url_visit_pair.second.GetLastVisitTime()) {
      url_visits_metadata.most_recent_timestamp =
          url_visit_pair.second.GetLastVisitTime();
    }
    url_visits.push_back(std::move(url_visit_pair.second));
  }
  url_visit_map.clear();

  return std::make_pair(std::move(url_visits), url_visits_metadata);
}

void SortScoredAggregatesAndCallback(
    std::vector<URLVisitAggregate> scored_visits,
    VisitedURLRankingService::RankURLVisitAggregatesCallback callback) {
  base::ranges::stable_sort(scored_visits, [](const auto& c1, const auto& c2) {
    // Sort such that higher scored entries precede lower scored entries.
    return c1.score > c2.score;
  });
  VLOG(2) << "visited_url_ranking: result size " << scored_visits.size();
  if (VLOG_IS_ON(2)) {
    for (const auto& visit : scored_visits) {
      VLOG(2) << "visited_url_ranking: Ordered ranked visit: " << visit.url_key
              << " " << *visit.score;
    }
  }

  base::UmaHistogramEnumeration("VisitedURLRanking.Request.Step.Rank.Status",
                                VisitedURLRankingRequestStepStatus::kSuccess);
  base::UmaHistogramCounts100("VisitedURLRanking.Rank.NumVisits",
                              scored_visits.size());
  std::move(callback).Run(ResultStatus::kSuccess, std::move(scored_visits));
}

void AddMostRecentDecoration(URLVisitAggregate& url_visit_aggregate,
                             base::Time most_recent_timestamp) {
  if (url_visit_aggregate.GetLastVisitTime() == most_recent_timestamp) {
    url_visit_aggregate.decorations.emplace_back(
        DecorationType::kMostRecent,
        GetStringForDecoration(DecorationType::kMostRecent));
  }
}

void AddFrequentlyVisitedDecoration(URLVisitAggregate& url_visit_aggregate) {
  int total_visits = 0;
  for (const auto& fetcher_entry : url_visit_aggregate.fetcher_data_map) {
    switch (fetcher_entry.first) {
      case Fetcher::kTabModel:
        total_visits += static_cast<int>(
            std::get<URLVisitAggregate::TabData>(fetcher_entry.second)
                .tab_count);
        break;
      case Fetcher::kSession:
        total_visits += static_cast<int>(
            std::get<URLVisitAggregate::TabData>(fetcher_entry.second)
                .tab_count);
        break;
      case Fetcher::kHistory:
        total_visits += static_cast<int>(
            std::get<URLVisitAggregate::HistoryData>(fetcher_entry.second)
                .visit_count);
        break;
    }
  }
  if (total_visits >
      features::kVisitedURLRankingFrequentlyVisitedThreshold.Get()) {
    url_visit_aggregate.decorations.emplace_back(
        DecorationType::kFrequentlyVisited,
        GetStringForDecoration(DecorationType::kFrequentlyVisited));
  }
}

void AddFrequentlyVisitedAtTimeDecoration(
    URLVisitAggregate& url_visit_aggregate) {
  const auto& fetcher_data_map = url_visit_aggregate.fetcher_data_map;
  if (fetcher_data_map.find(Fetcher::kHistory) != fetcher_data_map.end()) {
    const URLVisitAggregate::HistoryData* history_data =
        std::get_if<URLVisitAggregate::HistoryData>(
            &fetcher_data_map.at(Fetcher::kHistory));
    if (history_data) {
      if (static_cast<int>(history_data->same_time_group_visit_count) >
          features::kVisitedURLRankingDecorationTimeOfDay.Get()) {
        url_visit_aggregate.decorations.emplace_back(
            DecorationType::kFrequentlyVisitedAtTime,
            GetStringForDecoration(DecorationType::kFrequentlyVisitedAtTime));
      }
    }
  }
}

void AddVisitedXAgoDecoration(
    URLVisitAggregate& url_visit_aggregate,
    base::TimeDelta recently_visited_minutes_threshold) {
  url_visit_aggregate.decorations.emplace_back(
      DecorationType::kVisitedXAgo, GetStringForRecencyDecorationWithTime(
                                        url_visit_aggregate.GetLastVisitTime(),
                                        recently_visited_minutes_threshold));
}

}  // namespace

VisitedURLRankingServiceImpl::VisitedURLRankingServiceImpl(
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers,
    std::map<URLVisitAggregatesTransformType,
             std::unique_ptr<URLVisitAggregatesTransformer>> transformers,
    std::unique_ptr<url_deduplication::URLDeduplicationHelper>
        deduplication_helper)
    : segmentation_platform_service_(segmentation_platform_service),
      data_fetchers_(std::move(data_fetchers)),
      transformers_(std::move(transformers)),
      seen_record_delay_(base::Seconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          "seen_record_action_delay_sec",
          kSeenRecordDelaySec))),
      seen_records_sampling_rate_(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          "seen_record_action_sampling_rate",
          kSeenRecordsSamplingRate)),
      recently_visited_minutes_threshold_(base::Minutes(
          features::kVisitedURLRankingDecorationRecentlyVisitedMinutesThreshold
              .Get())),
      deduplication_helper_(std::move(deduplication_helper)) {}

VisitedURLRankingServiceImpl::~VisitedURLRankingServiceImpl() = default;

void VisitedURLRankingServiceImpl::FetchURLVisitAggregates(
    const FetchOptions& options,
    GetURLVisitAggregatesCallback callback) {
  auto merge_visits_and_callback =
      base::BindOnce(&VisitedURLRankingServiceImpl::MergeVisitsAndCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     options, options.transforms);

  const auto fetch_barrier_callback =
      base::BarrierCallback<std::pair<Fetcher, FetchResult>>(
          options.fetcher_sources.size(), std::move(merge_visits_and_callback));

  for (const auto& fetcher_entry : options.fetcher_sources) {
    if (!data_fetchers_.count(fetcher_entry.first)) {
      // Some fetchers may not be available (e.g. due to policy) and the client
      // of the service may not know it, so handle the case silently for now.
      // TODO(crbug/346822243): check if there is a better fallback behavior.
      fetch_barrier_callback.Run(std::make_pair(
          fetcher_entry.first, FetchResult(FetchResult::Status::kSuccess, {})));
      continue;
    }
    const auto& data_fetcher = data_fetchers_.at(fetcher_entry.first);
    data_fetcher->FetchURLVisitData(
        options, FetcherConfig(deduplication_helper_.get()),
        base::BindOnce(
            [](base::RepeatingCallback<void(std::pair<Fetcher, FetchResult>)>
                   barrier_callback,
               Fetcher fetcher, FetchResult result) {
              barrier_callback.Run(std::make_pair(fetcher, std::move(result)));
            },
            fetch_barrier_callback, fetcher_entry.first));
  }
}

void VisitedURLRankingServiceImpl::RankURLVisitAggregates(
    const Config& config,
    std::vector<URLVisitAggregate> visit_aggregates,
    RankURLVisitAggregatesCallback callback) {
  if (visit_aggregates.empty()) {
    base::UmaHistogramEnumeration(
        "VisitedURLRanking.Request.Step.Rank.Status",
        VisitedURLRankingRequestStepStatus::kSuccessEmpty);
    std::move(callback).Run(ResultStatus::kSuccess, {});
    return;
  }

  if (!segmentation_platform_service_ ||
      !base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformURLVisitResumptionRanker)) {
    base::UmaHistogramEnumeration(
        "VisitedURLRanking.Request.Step.Rank.Status",
        VisitedURLRankingRequestStepStatus::kFailedMissingBackend);
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

void VisitedURLRankingServiceImpl::DecorateURLVisitAggregates(
    const Config& config,
    std::vector<URLVisitAggregate> visit_aggregates,
    DecorateURLVisitAggregatesCallback callback) {
  URLVisitsMetadata url_visits_metadata;
  DecorateURLVisitAggregates(config, std::move(url_visits_metadata),
                             std::move(visit_aggregates), std::move(callback));
}

void VisitedURLRankingServiceImpl::DecorateURLVisitAggregates(
    const Config& config,
    visited_url_ranking::URLVisitsMetadata url_visits_metadata,
    std::vector<URLVisitAggregate> visit_aggregates,
    DecorateURLVisitAggregatesCallback callback) {
  if (visit_aggregates.empty()) {
    std::move(callback).Run(ResultStatus::kSuccess, {});
    return;
  }

  if (!base::FeatureList::IsEnabled(
          visited_url_ranking::features::kVisitedURLRankingDecorations)) {
    std::move(callback).Run(ResultStatus::kSuccess,
                            std::move(visit_aggregates));
    return;
  }

  for (size_t i = 0; i < visit_aggregates.size(); i++) {
    auto& url_visit_aggregate = visit_aggregates[i];

    if (url_visits_metadata.most_recent_timestamp.has_value()) {
      AddMostRecentDecoration(
          url_visit_aggregate,
          url_visits_metadata.most_recent_timestamp.value());
    }

    AddFrequentlyVisitedDecoration(url_visit_aggregate);

    AddFrequentlyVisitedAtTimeDecoration(url_visit_aggregate);

    // Default decoration
    AddVisitedXAgoDecoration(url_visit_aggregate,
                             recently_visited_minutes_threshold_);
  }

  std::move(callback).Run(ResultStatus::kSuccess, std::move(visit_aggregates));
}

void VisitedURLRankingServiceImpl::RecordAction(
    ScoredURLUserAction action,
    const std::string& visit_id,
    segmentation_platform::TrainingRequestId visit_request_id) {
  DCHECK(!visit_id.empty());
  VLOG(2) << "visited_url_ranking: RecordAction for " << visit_id << " "
          << static_cast<int>(action);
  base::UmaHistogramEnumeration("VisitedURLRanking.ScoredURLAction", action);

  const char* event_name = EventNameForAction(action);
  segmentation_platform::DatabaseClient::StructuredEvent visit_event = {
      event_name, {{visit_id, 1}}};
  segmentation_platform::DatabaseClient* client =
      segmentation_platform_service_->GetDatabaseClient();
  if (client) {
    client->AddEvent(visit_event);
  }

  base::TimeDelta wait_for_activation = base::TimeDelta();
  // If the action is kSeen, then wait for some time before recording this as
  // result, in case the user clicks on the suggestion. Effectively, this
  // would assume if the user clicks on first 5 mins, then it's a success,
  // otherwise failure.
  if (action == ScoredURLUserAction::kSeen) {
    if (base::RandInt(1, seen_records_sampling_rate_) > 1) {
      return;
    }
    wait_for_activation = seen_record_delay_;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VisitedURLRankingServiceImpl::TriggerTrainingData,
                     weak_ptr_factory_.GetWeakPtr(), action, visit_id,
                     visit_request_id),
      wait_for_activation);
}

void VisitedURLRankingServiceImpl::TriggerTrainingData(
    ScoredURLUserAction action,
    const std::string& visit_id,
    segmentation_platform::TrainingRequestId visit_request_id) {
  // Trigger UKM data collection on action.
  auto labels = segmentation_platform::TrainingLabels();
  labels.output_metric = std::make_pair("action", static_cast<int>(action));
  segmentation_platform_service_->CollectTrainingData(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_URL_VISIT_RESUMPTION_RANKER,
      visit_request_id, labels, base::DoNothing());
}

void VisitedURLRankingServiceImpl::MergeVisitsAndCallback(
    GetURLVisitAggregatesCallback callback,
    const FetchOptions& options,
    const std::vector<URLVisitAggregatesTransformType>& ordered_transforms,
    std::vector<std::pair<Fetcher, FetchResult>> fetcher_results) {
  std::queue<URLVisitAggregatesTransformType> transform_type_queue;
  for (const auto& transform_type : ordered_transforms) {
    transform_type_queue.push(transform_type);
  }

  auto url_visit_aggregates_data =
      ComputeURLVisitAggregates(std::move(fetcher_results));

  TransformVisitsAndCallback(
      std::move(callback), options, std::move(transform_type_queue),
      URLVisitAggregatesTransformType::kUnspecified,
      /*previous_aggregates_count=*/0,
      std::move(url_visit_aggregates_data.second), base::Time::Now(),
      URLVisitAggregatesTransformer::Status::kSuccess,
      std::move(url_visit_aggregates_data.first));
}

void VisitedURLRankingServiceImpl::TransformVisitsAndCallback(
    GetURLVisitAggregatesCallback callback,
    const FetchOptions& options,
    std::queue<URLVisitAggregatesTransformType> transform_type_queue,
    URLVisitAggregatesTransformType transform_type,
    size_t previous_aggregates_count,
    URLVisitsMetadata url_visits_metadata,
    base::Time start_time,
    URLVisitAggregatesTransformer::Status status,
    std::vector<URLVisitAggregate> aggregates) {
  if (transform_type != URLVisitAggregatesTransformType::kUnspecified) {
    base::UmaHistogramEnumeration(
        "VisitedURLRanking.Request.Step.Transform.Status",
        status == URLVisitAggregatesTransformer::Status::kSuccess
            ? VisitedURLRankingRequestStepStatus::kSuccess
            : VisitedURLRankingRequestStepStatus::kFailed);
    base::UmaHistogramBoolean(
        base::StringPrintf("VisitedURLRanking.TransformType.%s.Success",
                           URLVisitAggregatesTransformTypeName(transform_type)),
        status == URLVisitAggregatesTransformer::Status::kSuccess);
  }

  if (status == URLVisitAggregatesTransformer::Status::kError) {
    std::move(callback).Run(ResultStatus::kError,
                            std::move(url_visits_metadata), {});
    return;
  }

  if (previous_aggregates_count > 0) {
    base::UmaHistogramCustomCounts(
        base::StringPrintf("VisitedURLRanking.TransformType.%s.InOutPercentage",
                           URLVisitAggregatesTransformTypeName(transform_type)),
        std::round((static_cast<float>(aggregates.size()) /
                    previous_aggregates_count) *
                   100),
        1, 100, 100);

    base::UmaHistogramMediumTimes(
        base::StringPrintf("VisitedURLRanking.TransformType.%s.Latency",
                           URLVisitAggregatesTransformTypeName(transform_type)),
        base::Time::Now() - start_time);
  }

  if (transform_type_queue.empty() || aggregates.empty()) {
    std::move(callback).Run(ResultStatus::kSuccess, url_visits_metadata,
                            std::move(aggregates));
    return;
  }

  transform_type = transform_type_queue.front();
  transform_type_queue.pop();
  const auto it = transformers_.find(transform_type);
  if (it == transformers_.end()) {
    base::UmaHistogramEnumeration(
        "VisitedURLRanking.Request.Step.Transform.Status",
        VisitedURLRankingRequestStepStatus::kFailedNotFound);
    base::UmaHistogramBoolean(
        base::StringPrintf("VisitedURLRanking.TransformType.%s.Success",
                           URLVisitAggregatesTransformTypeName(transform_type)),
        false);
    std::move(callback).Run(ResultStatus::kError, url_visits_metadata, {});
    return;
  }

  size_t aggregates_count = aggregates.size();
  it->second->Transform(
      std::move(aggregates), options,
      base::BindOnce(&VisitedURLRankingServiceImpl::TransformVisitsAndCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     options, std::move(transform_type_queue), transform_type,
                     aggregates_count, url_visits_metadata, base::Time::Now()));
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
  visit.request_id = result.request_id;
  visit.score = model_score;
  visit_aggregates.pop_front();
  scored_visits.emplace_back(std::move(visit));

  GetNextResult(segmentation_key, std::move(visit_aggregates),
                std::move(scored_visits), std::move(callback));
}

}  // namespace visited_url_ranking
