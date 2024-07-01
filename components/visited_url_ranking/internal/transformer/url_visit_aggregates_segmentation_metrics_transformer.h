// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_URL_VISIT_AGGREGATES_SEGMENTATION_METRICS_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_URL_VISIT_AGGREGATES_SEGMENTATION_METRICS_TRANSFORMER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace segmentation_platform {

class SegmentationPlatformService;

}  // namespace segmentation_platform

namespace visited_url_ranking {

// The number of day ranges fo which `URLVisitAggregate` user interaction
// metrics will be computed (e.g., 1 day, 7 days, 30 days).
inline constexpr int kUserInteractionMetricsCollectionNumDayRanges = 3;

// The number of days preceding the current time for which user interaction
// metrics are collected.
inline constexpr std::array<int, kUserInteractionMetricsCollectionNumDayRanges>
    kAggregateMetricDayRanges = {1, 7, 30};

class URLVisitAggregatesSegmentationMetricsTransformer
    : public URLVisitAggregatesTransformer {
 public:
  explicit URLVisitAggregatesSegmentationMetricsTransformer(
      segmentation_platform::SegmentationPlatformService* sps);
  ~URLVisitAggregatesSegmentationMetricsTransformer() override;

  // Disallow copy/assign.
  URLVisitAggregatesSegmentationMetricsTransformer(
      const URLVisitAggregatesSegmentationMetricsTransformer&) = delete;
  URLVisitAggregatesSegmentationMetricsTransformer& operator=(
      const URLVisitAggregatesSegmentationMetricsTransformer&) = delete;

  // URLVisitAggregatesTransformer:

  // Sets segmenttion metric fields for `URLVisitAggregate` objects such as user
  // interaction signals (e.g., `seen`, `used`, `dismissed`).
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

 private:
  class SegmentationInitObserver;

  // Callback invoked with the segmentation framework's metrics query data is
  // ready.
  void OnMetricsQueryDataReady(
      std::vector<URLVisitAggregate> url_visit_aggregates,
      OnTransformCallback callback,
      segmentation_platform::DatabaseClient::ResultStatus status,
      const segmentation_platform::ModelProvider::Request& result);

  const raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;
  std::vector<std::unique_ptr<SegmentationInitObserver>> init_observers_;

  base::WeakPtrFactory<URLVisitAggregatesSegmentationMetricsTransformer>
      weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_URL_VISIT_AGGREGATES_SEGMENTATION_METRICS_TRANSFORMER_H_
