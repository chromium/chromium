// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_VISITED_URL_RANKING_SERVICE_IMPL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_VISITED_URL_RANKING_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <queue>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace visited_url_ranking {

// The internal implementation of the VisitedURLRankingService.
class VisitedURLRankingServiceImpl : public VisitedURLRankingService {
 public:
  VisitedURLRankingServiceImpl(
      std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers,
      std::map<URLVisitAggregatesTransformType,
               std::unique_ptr<URLVisitAggregatesTransformer>> transformers);
  ~VisitedURLRankingServiceImpl() override;

  // Disallow copy/assign.
  VisitedURLRankingServiceImpl(const VisitedURLRankingServiceImpl&) = delete;
  VisitedURLRankingServiceImpl& operator=(const VisitedURLRankingServiceImpl&) =
      delete;

  // VisitedURLRankingService::
  void FetchURLVisitAggregates(const FetchOptions& options,
                               GetURLVisitAggregatesCallback callback) override;
  void RankURLVisitAggregates(const Config& config,
                              std::vector<URLVisitAggregate> visits,
                              RankVisitAggregatesCallback callback) override;

 private:
  // Callback invoked when the various fetcher instances have completed.
  void MergeVisitsAndCallback(
      GetURLVisitAggregatesCallback callback,
      const std::vector<URLVisitAggregatesTransformType>& ordered_transforms,
      std::vector<FetchResult> fetcher_visits);

  // Callback invoked when the various transformers have completed.
  void TransformVisitsAndCallback(
      GetURLVisitAggregatesCallback callback,
      std::queue<URLVisitAggregatesTransformType> transform_type_queue,
      URLVisitAggregatesTransformer::Status status,
      std::vector<URLVisitAggregate> aggregates);

  // A map of supported URL visit data fetchers that may participate in the
  // computation of `URLVisitAggregate` objects.
  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers_;

  // A map of supported transformers for transform types.
  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers_;

  base::WeakPtrFactory<VisitedURLRankingServiceImpl> weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_VISITED_URL_RANKING_SERVICE_IMPL_H
