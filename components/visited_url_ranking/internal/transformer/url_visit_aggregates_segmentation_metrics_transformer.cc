// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/url_visit_aggregates_segmentation_metrics_transformer.h"

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/service_proxy.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace {

// A metric name substring used for capturing the total number of times visit
// suggestions based on a given aggregate were seen for a given day range.
const char kSeenCount[] = "seen_count";

// A metric name substring used for capturing the total number of times visit
// suggestions based on a given aggregate were activated for a given day range.
const char kActivatedCount[] = "activated_count";

// A metric name substring used for capturing the total number of times visit
// suggestions based on a given aggregate were dismissed for a given day range.
const char kDismissedCount[] = "dismissed_count";

constexpr int kNumUserInteractionMetrics = 3;
constexpr std::array<const char*, kNumUserInteractionMetrics>
    kUserInteractionMetrics{kSeenCount, kActivatedCount, kDismissedCount};

// Helper function to populate time period specific user interaction signal for
// input context creation.
void AddUserInteractionSignal(std::map<std::string, float>& metrics,
                              int day_range,
                              const char* name,
                              int count) {
  metrics.emplace(day_range == 1
                      ? base::StringPrintf("%s_last_day", name)
                      : base::StringPrintf("%s_last_%d_days", name, day_range),
                  count);
}

}  // namespace

namespace visited_url_ranking {

class URLVisitAggregatesSegmentationMetricsTransformer::SegmentationInitObserver
    : public segmentation_platform::ServiceProxy::Observer {
 public:
  SegmentationInitObserver(
      base::OnceClosure callback,
      segmentation_platform::SegmentationPlatformService* service)
      : callback_(std::move(callback)), service_(service) {
    obs_.Observe(service->GetServiceProxy());
  }
  ~SegmentationInitObserver() override = default;

  // segmentation_platform::ServiceProxy::Observer impl:
  void OnServiceStatusChanged(bool is_initialized, int status_flag) override {
    if (is_initialized && !callback_.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback_));
    }
    obs_.Reset();
  }

 private:
  base::OnceClosure callback_;
  const raw_ptr<segmentation_platform::SegmentationPlatformService> service_;
  base::ScopedObservation<segmentation_platform::ServiceProxy,
                          segmentation_platform::ServiceProxy::Observer>
      obs_{this};
};

URLVisitAggregatesSegmentationMetricsTransformer::
    URLVisitAggregatesSegmentationMetricsTransformer(
        segmentation_platform::SegmentationPlatformService* sps)
    : segmentation_platform_service_(sps) {
  CHECK(segmentation_platform_service_);
}

URLVisitAggregatesSegmentationMetricsTransformer::
    ~URLVisitAggregatesSegmentationMetricsTransformer() = default;

void URLVisitAggregatesSegmentationMetricsTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  segmentation_platform::DatabaseClient* client =
      segmentation_platform_service_->GetDatabaseClient();
  if (!client) {
    if (!segmentation_platform_service_->IsPlatformInitialized()) {
      base::OnceClosure run_after_init = base::BindOnce(
          &URLVisitAggregatesSegmentationMetricsTransformer::Transform,
          weak_ptr_factory_.GetWeakPtr(), std::move(aggregates), options,
          std::move(callback));
      init_observers_.push_back(std::make_unique<SegmentationInitObserver>(
          std::move(run_after_init), segmentation_platform_service_));
    } else {
      std::move(callback).Run(Status::kError, std::move(aggregates));
    }
    return;
  }

  segmentation_platform::proto::SegmentationModelMetadata metadata;
  segmentation_platform::MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig();
  std::set<std::string> metric_names = {};
  for (const auto& url_visit_aggregate : aggregates) {
    metric_names.insert(url_visit_aggregate.url_key);
  }

  for (int query_day_count : kAggregateMetricDayRanges) {
    segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
        writer, kURLVisitSeenEventName, metric_names, query_day_count);
    segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
        writer, kURLVisitActivatedEventName, metric_names, query_day_count);
    segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
        writer, kURLVisitDismissedEventName, metric_names, query_day_count);
  }

  client->ProcessFeatures(
      metadata, base::Time::Now(),
      base::BindOnce(&URLVisitAggregatesSegmentationMetricsTransformer::
                         OnMetricsQueryDataReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(aggregates),
                     std::move(callback)));
}

void URLVisitAggregatesSegmentationMetricsTransformer::OnMetricsQueryDataReady(
    std::vector<URLVisitAggregate> url_visit_aggregates,
    OnTransformCallback callback,
    segmentation_platform::DatabaseClient::ResultStatus status,
    const segmentation_platform::ModelProvider::Request& result) {
  // The resulting vector of floats produced by the queries specified in the
  // `metadata` above in the `Transform` function must meet a given expected
  // size as checked below.
  if (result.size() !=
      url_visit_aggregates.size() * 3 * kAggregateMetricDayRanges.size()) {
    std::move(callback).Run(Status::kError, std::move(url_visit_aggregates));
    return;
  }

  for (size_t i = 0; i < kAggregateMetricDayRanges.size(); i++) {
    for (size_t u = 0; u < kUserInteractionMetrics.size(); u++) {
      int count_index =
          (i * kUserInteractionMetrics.size() * url_visit_aggregates.size()) +
          u * url_visit_aggregates.size();
      for (size_t agg_index = 0; agg_index < url_visit_aggregates.size();
           agg_index++) {
        AddUserInteractionSignal(
            url_visit_aggregates[agg_index].metrics_signals,
            kAggregateMetricDayRanges[i], kUserInteractionMetrics[u],
            result[count_index + agg_index]);
      }
    }
  }

  std::move(callback).Run(Status::kSuccess, std::move(url_visit_aggregates));
}

}  // namespace visited_url_ranking
