// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
using Samples = std::vector<SignalDatabase::Sample>;

class FeatureAggregatorImplTest : public testing::Test {
 public:
  FeatureAggregatorImplTest() = default;
  ~FeatureAggregatorImplTest() override = default;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    feature_aggregator_ = std::make_unique<FeatureAggregatorImpl>();
  }
  base::SimpleTestClock clock_;
  std::unique_ptr<FeatureAggregatorImpl> feature_aggregator_;
};

TEST_F(FeatureAggregatorImplTest, SumCountAggregation) {
  Samples samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };

  std::vector<float> res = feature_aggregator_->Process(
      proto::SignalType::HISTOGRAM_VALUE, proto::Aggregation::SUM_COUNT, 1u,
      clock_.Now(), base::TimeDelta::FromSeconds(10), samples);
  // SUM_COUNT always produces a single value.
  EXPECT_EQ(1u, res.size());
  // We should have counted 3 samples.
  EXPECT_EQ(3, res[0]);
}

TEST_F(FeatureAggregatorImplTest, SumValuesAggregationHistogram) {
  Samples samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };

  std::vector<float> res = feature_aggregator_->Process(
      proto::SignalType::HISTOGRAM_VALUE, proto::Aggregation::SUM_VALUES, 1u,
      clock_.Now(), base::TimeDelta::FromSeconds(10), samples);
  // SUM_VALUES always produces a single value.
  EXPECT_EQ(1u, res.size());
  // We should have summed up to 1+2+3=6.
  EXPECT_EQ(6, res[0]);
}

TEST_F(FeatureAggregatorImplTest, SumValuesAggregationUserAction) {
  Samples samples{
      {clock_.Now(), absl::nullopt},
      {clock_.Now(), absl::nullopt},
      {clock_.Now(), absl::nullopt},
  };

  std::vector<float> res = feature_aggregator_->Process(
      proto::SignalType::USER_ACTION, proto::Aggregation::SUM_VALUES, 1u,
      clock_.Now(), base::TimeDelta::FromSeconds(10), samples);
  // SUM_VALUES always produces a single value.
  EXPECT_EQ(1u, res.size());
  // We should have summed up to 1+1+1=3.
  EXPECT_EQ(3, res[0]);
}

TEST_F(FeatureAggregatorImplTest, SumValuesAggregationUserActionIgnoresValue) {
  Samples samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };

  std::vector<float> res = feature_aggregator_->Process(
      proto::SignalType::USER_ACTION, proto::Aggregation::SUM_VALUES, 1u,
      clock_.Now(), base::TimeDelta::FromSeconds(10), samples);
  // SUM_VALUES always produces a single value.
  EXPECT_EQ(1u, res.size());
  // We should have summed up to 1+1+1=3.
  EXPECT_EQ(3, res[0]);
}

TEST_F(FeatureAggregatorImplTest, FilterEnumSamples) {
  Samples samples{
      {clock_.Now(), 1}, {clock_.Now(), 2}, {clock_.Now(), 3},
      {clock_.Now(), 4}, {clock_.Now(), 5},
  };

  // Empty accept list should keep all samples.
  feature_aggregator_->FilterEnumSamples(std::vector<int32_t>(), samples);
  EXPECT_EQ(5u, samples.size());

  // Only accept 1 and 3 as enum values.
  feature_aggregator_->FilterEnumSamples(std::vector<int32_t>{2, 4}, samples);
  EXPECT_EQ(2u, samples.size());
  EXPECT_EQ(2, samples[0].second.value());
  EXPECT_EQ(4, samples[1].second.value());
}

}  // namespace segmentation_platform
