// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_VISITED_URL_RANKING_SERVICE_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_VISITED_URL_RANKING_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

const char kTabResumptionRankerKey[] = "tab_resumption_ranker";

// Settings leveraged for ranking `URLVisitAggregate` objects.
struct Config {
  // A value that identifies the type of model to run.
  std::string key;
};

enum class ResultStatus {
  kError = 0,
  kSuccess = 1,
};

// Provides APIs suitable for combining URL Visit data across various data
// sources and their subsequent ranking via a model.
// Example usage:
//   auto on_rank_callback = base::BindOnce([](ResultStatus status,
//       std::vector<URLVisitAggregate> visits) {
//     if(status == ResultStatus::kSuccess) {
//       // Client logic placeholder.
//     }
//   });
//   auto on_fetch_callback = base::BindOnce([](OnceCallback on_rank_callback,
//       ResultStatus status, std::vector<URLVisitAggregate> visits) {
//     if(status == ResultStatus::kSuccess) {
//       // Client logic placeholder (e.g. filtering, caching, etc.).
//       Config config = {.key = kTabResumptionRankerKey};
//       GetService()->RankURLVisitAggregates(config, std::move(visits),
//           std::move(on_rank_callback));
//     }
//   }, std::move(on_rank_callback));
//   GetService()->FetchURLVisitAggregates(
//       CreateTabResumptionDefaultFetchOptions(),
//       std::move(on_fetch_callback));
//
class VisitedURLRankingService : public KeyedService {
 public:
  VisitedURLRankingService() = default;
  ~VisitedURLRankingService() override = default;

  // Computes `URLVisitAggregate` objects based on a series of
  // `options` from one or more data providers and triggers the `callback` with
  // such data.
  using GetURLVisitAggregatesCallback =
      base::OnceCallback<void(ResultStatus, std::vector<URLVisitAggregate>)>;
  virtual void FetchURLVisitAggregates(
      const FetchOptions& options,
      GetURLVisitAggregatesCallback callback) = 0;

  using RankVisitAggregatesCallback =
      base::OnceCallback<void(ResultStatus, std::vector<URLVisitAggregate>)>;
  // Ranks a collection of `URLVisitAggregate` objects based on a
  // client specified strategy.
  virtual void RankURLVisitAggregates(const Config& config,
                                      std::vector<URLVisitAggregate> visits,
                                      RankVisitAggregatesCallback callback) = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_VISITED_URL_RANKING_SERVICE_H_
