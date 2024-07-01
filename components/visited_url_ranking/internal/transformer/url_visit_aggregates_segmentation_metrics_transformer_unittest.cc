// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/url_visit_aggregates_segmentation_metrics_transformer.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_database_client.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace visited_url_ranking {

class URLVisitAggregatesSegmentationMetricsTransformerTest
    : public URLVisitAggregatesTransformerTest {
 public:
  URLVisitAggregatesSegmentationMetricsTransformerTest() = default;
  ~URLVisitAggregatesSegmentationMetricsTransformerTest() override = default;

  // Disallow copy/assign.
  URLVisitAggregatesSegmentationMetricsTransformerTest(
      const URLVisitAggregatesSegmentationMetricsTransformerTest&) = delete;
  URLVisitAggregatesSegmentationMetricsTransformerTest& operator=(
      const URLVisitAggregatesSegmentationMetricsTransformerTest&) = delete;

  void SetUp() override {
    database_client_ =
        std::make_unique<segmentation_platform::MockDatabaseClient>();
    segmentation_platform_service_ = std::make_unique<
        segmentation_platform::MockSegmentationPlatformService>();

    EXPECT_CALL(*segmentation_platform_service_, GetDatabaseClient())
        .WillRepeatedly(testing::Return(database_client_.get()));

    transformer_ =
        std::make_unique<URLVisitAggregatesSegmentationMetricsTransformer>(
            segmentation_platform_service_.get());
  }

  void TearDown() override {
    transformer_ = nullptr;
    segmentation_platform_service_ = nullptr;
    database_client_ = nullptr;
  }

 protected:
  std::unique_ptr<segmentation_platform::MockDatabaseClient> database_client_;

 private:
  std::unique_ptr<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
};

TEST_F(URLVisitAggregatesSegmentationMetricsTransformerTest, Transform) {
  // A collection of seen, activated, and dismissed counts across the
  // day ranges specified in `kAggregateMetricDayRanges`.
  constexpr size_t kNumUserInteractionMetrics = 3;
  constexpr size_t kSampleAggregatesCount = 2;
  std::vector<float> counts = {
      // Last day
      2.0, 3.0,  // Seen
      1.0, 2.0,  // Activated
      0, 1.0,    // Dismissed
      // Last 7 days
      3.0, 4.0,  // Seen
      2.0, 3.0,  // Activated
      1.0, 2.0,  // Dismissed
      // Last 30 days
      4.0, 5.0,  // Seen
      3.0, 4.0,  // Activated
      2.0, 3.0,  // Dismissed
  };
  ASSERT_EQ(counts.size(), kAggregateMetricDayRanges.size() *
                               kNumUserInteractionMetrics *
                               kSampleAggregatesCount);
  EXPECT_CALL(*database_client_, ProcessFeatures(_, _, _))
      .WillOnce(testing::Invoke(
          [&counts](
              const segmentation_platform::proto::SegmentationModelMetadata&
                  metadata,
              base::Time end_time,
              segmentation_platform::DatabaseClient::FeaturesCallback
                  callback) {
            std::move(callback).Run(
                segmentation_platform::DatabaseClient::ResultStatus::kSuccess,
                counts);
          }));

  std::vector<URLVisitAggregate> input_sample_aggregates = {};
  for (size_t i = 0; i < kSampleAggregatesCount; i++) {
    input_sample_aggregates.push_back(CreateSampleURLVisitAggregate(
        GURL(base::StrCat({kSampleSearchUrl, base::NumberToString(i)}))));
  }
  URLVisitAggregatesSegmentationMetricsTransformerTest::Result result =
      TransformAndGetResult(std::move(input_sample_aggregates));
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  EXPECT_EQ(result.second.size(), kSampleAggregatesCount);
  EXPECT_EQ(result.second.front().metrics_signals.size(),
            kNumUserInteractionMetrics * kAggregateMetricDayRanges.size());
  EXPECT_EQ(result.second.back().metrics_signals.size(),
            kNumUserInteractionMetrics * kAggregateMetricDayRanges.size());

  const auto& first_aggregate_metrics = result.second.front().metrics_signals;
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("seen_count_last_day"), 2.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("activated_count_last_day"), 1.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("dismissed_count_last_day"), 0);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("seen_count_last_7_days"), 3.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("activated_count_last_7_days"),
                  2.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("dismissed_count_last_7_days"),
                  1.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("seen_count_last_30_days"), 4.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("activated_count_last_30_days"),
                  3.0f);
  EXPECT_FLOAT_EQ(first_aggregate_metrics.at("dismissed_count_last_30_days"),
                  2.0f);

  const auto& last_aggregate_metrics = result.second.back().metrics_signals;
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("seen_count_last_day"), 3.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("activated_count_last_day"), 2.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("dismissed_count_last_day"), 1.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("seen_count_last_7_days"), 4.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("activated_count_last_7_days"),
                  3.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("dismissed_count_last_7_days"),
                  2.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("seen_count_last_30_days"), 5.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("activated_count_last_30_days"),
                  4.0f);
  EXPECT_FLOAT_EQ(last_aggregate_metrics.at("dismissed_count_last_30_days"),
                  3.0f);
}

}  // namespace visited_url_ranking
