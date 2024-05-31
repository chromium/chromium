// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_TESTING_MOCK_VISITED_URL_RANKING_SERVICE_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_TESTING_MOCK_VISITED_URL_RANKING_SERVICE_H_

#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace visited_url_ranking {

class MockVisitedURLRankingService : public VisitedURLRankingService {
 public:
  MockVisitedURLRankingService();
  MockVisitedURLRankingService(const MockVisitedURLRankingService&) = delete;
  MockVisitedURLRankingService& operator=(const MockVisitedURLRankingService&) =
      delete;
  ~MockVisitedURLRankingService() override;

  MOCK_METHOD2(FetchURLVisitAggregates,
               void(const FetchOptions& options,
                    GetURLVisitAggregatesCallback callback));

  MOCK_METHOD3(RankURLVisitAggregates,
               void(const Config& config,
                    std::vector<URLVisitAggregate> visits,
                    RankURLVisitAggregatesCallback callback));

  MOCK_METHOD3(RecordAction,
               void(ScoredURLUserAction action,
                    const std::string& visit_id,
                    segmentation_platform::TrainingRequestId visit_request_id));
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_TESTING_MOCK_VISITED_URL_RANKING_SERVICE_H_
