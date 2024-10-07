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

// Value must match `segmentation_platform::kURLVisitResumptionClassifierKey`.
const char kTabResumptionRankerKey[] = "url_visit_resumption_ranker";

// Aggregate metrics event related names.
const char kURLVisitSeenEventName[] = "VisitedURLRanking.URLVisit.Seen";
const char kURLVisitActivatedEventName[] =
    "VisitedURLRanking.URLVisit.Activated";
const char kURLVisitDismissedEventName[] =
    "VisitedURLRanking.URLVisit.Dismissed";

// An action performed by the user on a `URLVisit` through a UI surface.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.visited_url_ranking
// LINT.IfChange(ScoredURLUserAction)
enum ScoredURLUserAction {
  kUnknown = 0,
  kSeen = 1,
  kActivated = 2,
  kDismissed = 3,
  kMaxValue = kDismissed,
};
// LINT.ThenChange(/tools/metrics/histograms/visited_url_ranking/enums.xml:ScoredURLUserAction)

// Settings leveraged for ranking `URLVisitAggregate` objects.
struct Config {
  // A value that identifies the type of model to run.
  std::string key;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ResultStatus {
  kError = 0,
  kSuccess = 1,
  kMaxValue = kSuccess,
};

// Holds data about the vector of URLVisitAggregates.
struct URLVisitsMetadata {
  size_t aggregates_count_before_transforms = 0;
  std::optional<base::Time> most_recent_timestamp;
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

  // Computes `URLVisitAggregate` objects based on a series of `options` for
  // one or more data providers and triggers the `callback` with such data.
  using GetURLVisitAggregatesCallback = base::OnceCallback<
      void(ResultStatus, URLVisitsMetadata, std::vector<URLVisitAggregate>)>;
  virtual void FetchURLVisitAggregates(
      const FetchOptions& options,
      GetURLVisitAggregatesCallback callback) = 0;

  using RankURLVisitAggregatesCallback =
      base::OnceCallback<void(ResultStatus, std::vector<URLVisitAggregate>)>;
  // Ranks a collection of `URLVisitAggregate` objects based on a client
  // specified strategy.
  virtual void RankURLVisitAggregates(
      const Config& config,
      std::vector<URLVisitAggregate> visit_aggregates,
      RankURLVisitAggregatesCallback callback) = 0;

  // Adds relevant decorations to a collection of `URLVisitAggregate` objects.
  // Only the visits that are to be displayed on the UI should be passed to
  // this method.
  using DecorateURLVisitAggregatesCallback =
      base::OnceCallback<void(ResultStatus, std::vector<URLVisitAggregate>)>;
  // TODO(crbug/364577990): Remove this function when callers switch to the
  // version that uses metadata.
  virtual void DecorateURLVisitAggregates(
      const Config& config,
      std::vector<URLVisitAggregate> visit_aggregates,
      DecorateURLVisitAggregatesCallback callback) = 0;
  virtual void DecorateURLVisitAggregates(
      const Config& config,
      visited_url_ranking::URLVisitsMetadata url_visits_metadata,
      std::vector<URLVisitAggregate> visit_aggregates,
      DecorateURLVisitAggregatesCallback callback) = 0;

  // Records a user action performed for a `URLVisitAggregate` object returned
  // by `RankURLVisitAggregates`. This is needed to collect feedback on whether
  // the predicted URL visit was a success or failure, to help train the ranking
  // system. The caller must call `RecordAction` for every `URLVisitAggregate`
  // object seen, dismissed, or activated by the user.  It would be preferred to
  // not record actions in situations where the visit was not shown to the user,
  // or the visit was not in a visible part of the screen. `visit_id` and
  // `visit_request_id` are provided in the `URLVisitAggregate` object. Only
  // valid to call within the same Chrome session as the call to
  // `RankURLVisitAggregates`. It is ok to make multiple calls to
  // `RankURLVisitAggregates` before calling `RecordAction` since
  // `visit_request_id` is globally unique.
  // It is better to call RecordAction at a point where user can no longer
  // activate the URL, like NTP is being destroyed. But, it is ok to record seen
  // and activated events when ever they happen.
  virtual void RecordAction(
      ScoredURLUserAction action,
      const std::string& visit_id,
      segmentation_platform::TrainingRequestId visit_request_id) = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_VISITED_URL_RANKING_SERVICE_H_
