// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_VISITED_URL_RANKING_SERVICE_IMPL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_VISITED_URL_RANKING_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace segmentation_platform {

struct AnnotatedNumericResult;
class SegmentationPlatformService;

}  // namespace segmentation_platform

namespace visited_url_ranking {

enum class Status;

// The internal implementation of the VisitedURLRankingService.
class VisitedURLRankingServiceImpl : public VisitedURLRankingService {
 public:
  // Wait time before which we record kSeen events as feedback.
  constexpr static int kSeenRecordDelaySec = 300;

  VisitedURLRankingServiceImpl(
      segmentation_platform::SegmentationPlatformService*
          segmentation_platform_service,
      std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers,
      std::map<URLVisitAggregatesTransformType,
               std::unique_ptr<URLVisitAggregatesTransformer>> transformers);
  ~VisitedURLRankingServiceImpl() override;

  // Disallow copy/assign.
  VisitedURLRankingServiceImpl(const VisitedURLRankingServiceImpl&) = delete;
  VisitedURLRankingServiceImpl& operator=(const VisitedURLRankingServiceImpl&) =
      delete;

  // VisitedURLRankingService:
  void FetchURLVisitAggregates(const FetchOptions& options,
                               GetURLVisitAggregatesCallback callback) override;
  void RankURLVisitAggregates(const Config& config,
                              std::vector<URLVisitAggregate> visits,
                              RankURLVisitAggregatesCallback callback) override;
  void RecordAction(
      ScoredURLUserAction action,
      const std::string& visit_id,
      segmentation_platform::TrainingRequestId visit_request_id) override;

 private:
  // Trigger training data collection with the user action.
  void TriggerTrainingData(
      ScoredURLUserAction action,
      const std::string& visit_id,
      segmentation_platform::TrainingRequestId visit_request_id);

  // Callback invoked when the various fetcher instances have completed.
  void MergeVisitsAndCallback(
      GetURLVisitAggregatesCallback callback,
      const FetchOptions& options,
      const std::vector<URLVisitAggregatesTransformType>& ordered_transforms,
      std::vector<FetchResult> fetcher_visits);

  // Callback invoked when the various transformers have completed.
  void TransformVisitsAndCallback(
      GetURLVisitAggregatesCallback callback,
      const FetchOptions& options,
      std::queue<URLVisitAggregatesTransformType> transform_type_queue,
      URLVisitAggregatesTransformer::Status status,
      std::vector<URLVisitAggregate> aggregates);

  // Invoked to get the score (i.e. numeric result) for a given URL visit
  // aggregate.
  void GetNextResult(const std::string& segmentation_key,
                     std::deque<URLVisitAggregate> visit_aggregates,
                     std::vector<URLVisitAggregate> scored_visits,
                     RankURLVisitAggregatesCallback callback);

  // Callback invoked when a score (i.e. numeric result) has been obtained for a
  // given URL visit aggregate.
  void OnGetResult(const std::string& segmentation_key,
                   std::deque<URLVisitAggregate> visit_aggregates,
                   std::vector<URLVisitAggregate> scored_visits,
                   RankURLVisitAggregatesCallback callback,
                   const segmentation_platform::AnnotatedNumericResult& result);

  // The service to use to execute URL visit score prediction.
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;

  // A map of supported URL visit data fetchers that may participate in the
  // computation of `URLVisitAggregate` objects.
  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers_;

  // A map of supported transformers for transform types.
  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers_;

  // Time delay to record kSeen events in case kActivation events are recorded.
  const base::TimeDelta seen_record_delay_;

  // Sampling rate for kSeen events to balance training collection.
  const int seen_records_sampling_rate_;

  base::WeakPtrFactory<VisitedURLRankingServiceImpl> weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_VISITED_URL_RANKING_SERVICE_IMPL_H_
