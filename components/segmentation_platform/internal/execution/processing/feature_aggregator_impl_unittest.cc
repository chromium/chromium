// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_aggregator_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/metrics/metrics_hashes.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::processing {
using Sample = SignalDatabase::Sample;
using proto::Aggregation;
using proto::SignalType;

namespace {
constexpr base::TimeDelta kDefaultBucketDuration = base::Hours(3);
constexpr base::TimeDelta kOneSecond = base::Seconds(1);
constexpr base::TimeDelta kTwoSeconds = base::Seconds(2);
constexpr base::TimeDelta kThreeSeconds = base::Seconds(3);
constexpr base::TimeDelta kFourSeconds = base::Seconds(4);
constexpr uint64_t kDefaultBucketCount = 6;

}  // namespace

class FeatureAggregatorImplTest : public testing::Test {
 public:
  FeatureAggregatorImplTest() = default;
  ~FeatureAggregatorImplTest() override = default;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    feature_aggregator_ = std::make_unique<FeatureAggregatorImpl>();
  }

  // Returns samples which when using kDefaultBucketDuration will end up in the
  // following 6 buckets with their respective values:
  // bucket[0] = {1, 2}                (count=2, sum=3)
  // bucket[1] = {3, 4, 5}             (count=3, sum=12)
  // bucket[2] = {6, 7, 8, 9}          (count=4, sum=30)
  // bucket[3] = {10}                  (count=1, sum=10)
  // bucket[4] = {}                    (count=0, sum=0)
  // bucket[5] = {11, 12, 13, 14, 15}  (count=5, sum=65)
  std::vector<Sample> value_samples() {
    return {
        // First bucket.
        {clock_.Now(), 1},
        {clock_.Now() - kOneSecond, 2},
        // Second bucket.
        {clock_.Now() - kDefaultBucketDuration, 3},
        {clock_.Now() - kDefaultBucketDuration - kOneSecond, 4},
        {clock_.Now() - kDefaultBucketDuration - kTwoSeconds, 5},
        // Third bucket.
        {clock_.Now() - kDefaultBucketDuration * 2, 6},
        {clock_.Now() - kDefaultBucketDuration * 2 - kOneSecond, 7},
        {clock_.Now() - kDefaultBucketDuration * 2 - kTwoSeconds, 8},
        {clock_.Now() - kDefaultBucketDuration * 2 - kThreeSeconds, 9},
        // Fourth bucket.
        {clock_.Now() - kDefaultBucketDuration * 3, 10},
        // Fifth bucket is empty.
        // Sixth bucket.
        {clock_.Now() - kDefaultBucketDuration * 5, 11},
        {clock_.Now() - kDefaultBucketDuration * 5 - kOneSecond, 12},
        {clock_.Now() - kDefaultBucketDuration * 5 - kTwoSeconds, 13},
        {clock_.Now() - kDefaultBucketDuration * 5 - kThreeSeconds, 14},
        {clock_.Now() - kDefaultBucketDuration * 5 - kFourSeconds, 15},
    };
  }

  // Returns samples which when using kDefaultBucketDuration will end up in the
  // following 6 buckets with their respective values:
  // bucket[0] = {0, 0}           (count=2, sum=0)
  // bucket[1] = {0, 0, 0}        (count=3, sum=0)
  // bucket[2] = {0, 0, 0, 0}     (count=4, sum=0)
  // bucket[3] = {0}              (count=1, sum=0)
  // bucket[4] = {}               (count=0, sum=0)
  // bucket[5] = {0, 0, 0, 0, 0}  (count=5, sum=0)
  std::vector<Sample> zero_value_samples() {
    std::vector<Sample> samples = value_samples();
    for (auto& sample : samples)
      sample.second = 0;

    return samples;
  }

  // Verifies the result of a single invocation of Process(...), comparing to
  // the expected output.
  void Verify(SignalType signal_type,
              Aggregation aggregation,
              uint64_t bucket_count,
              base::TimeDelta bucket_duration,
              std::vector<Sample> samples,
              std::optional<std::vector<float>> expected) {
    std::vector<SignalDatabase::DbEntry> entries;
    base::Time start_time = clock_.Now();
    for (const auto& sample : samples) {
      entries.push_back(SignalDatabase::DbEntry{.type = signal_type,
                                                .name_hash = 123,
                                                .time = sample.first,
                                                .value = sample.second});
      if (start_time > sample.first) {
        start_time = sample.first;
      }
    }
    std::optional<std::vector<float>> res = feature_aggregator_->Process(
        signal_type, 123, aggregation, bucket_count, start_time, clock_.Now(),
        bucket_duration, {}, entries);
    EXPECT_EQ(expected, res);
  }

  // Verifies the result of a multiple invocations of Process(...), comparing to
  // the expected output in the cases of using value samples, zero-value
  // samples, no-value samples, and an empty input vector.
  void VerifyAllOptional(SignalType signal_type,
                         Aggregation aggregation,
                         std::optional<std::vector<float>> expected_value,
                         std::optional<std::vector<float>> expected_zero_value,
                         std::optional<std::vector<float>> expected_empty) {
    // Value is always assumed to be 1 for USER_ACTION.
    Verify(signal_type, aggregation, kDefaultBucketCount,
           kDefaultBucketDuration, value_samples(), expected_value);

    // Value is always assumed to be 1 for USER_ACTION.
    Verify(signal_type, aggregation, kDefaultBucketCount,
           kDefaultBucketDuration, zero_value_samples(), expected_zero_value);

    Verify(signal_type, aggregation, kDefaultBucketCount,
           kDefaultBucketDuration, {}, expected_empty);
  }

  // Calls VerifyAllOptional with non optional parameters.
  void VerifyAll(SignalType signal_type,
                 Aggregation aggregation,
                 std::vector<float> expected_value,
                 std::vector<float> expected_zero_value,
                 std::vector<float> expected_empty) {
    VerifyAllOptional(signal_type, aggregation, expected_value,
                      expected_zero_value, expected_empty);
  }

  base::SimpleTestClock clock_;
  std::unique_ptr<FeatureAggregatorImpl> feature_aggregator_;
};

TEST_F(FeatureAggregatorImplTest, CountAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::COUNT, {15}, {15}, {0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::COUNT, {15}, {15}, {0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::COUNT, {15}, {15}, {0});
}

TEST_F(FeatureAggregatorImplTest, CountBooleanAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::COUNT_BOOLEAN, {1}, {1}, {0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::COUNT_BOOLEAN, {1}, {1},
            {0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::COUNT_BOOLEAN, {1}, {1},
            {0});
}

TEST_F(FeatureAggregatorImplTest, BucketedCountAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::BUCKETED_COUNT,
            {2, 3, 4, 1, 0, 5}, {2, 3, 4, 1, 0, 5}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_COUNT,
            {2, 3, 4, 1, 0, 5}, {2, 3, 4, 1, 0, 5}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_COUNT,
            {2, 3, 4, 1, 0, 5}, {2, 3, 4, 1, 0, 5}, {0, 0, 0, 0, 0, 0});
}

TEST_F(FeatureAggregatorImplTest, BucketedCountBooleanAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::BUCKETED_COUNT_BOOLEAN,
            {1, 1, 1, 1, 0, 1}, {1, 1, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_COUNT_BOOLEAN,
            {1, 1, 1, 1, 0, 1}, {1, 1, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_COUNT_BOOLEAN,
            {1, 1, 1, 1, 0, 1}, {1, 1, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0});
}

TEST_F(FeatureAggregatorImplTest, BucketedCountBooleanTrueCountAggregation) {
  VerifyAll(SignalType::USER_ACTION,
            Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT, {5}, {5}, {0});

  VerifyAll(SignalType::HISTOGRAM_ENUM,
            Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT, {5}, {5}, {0});

  VerifyAll(SignalType::HISTOGRAM_VALUE,
            Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT, {5}, {5}, {0});
}

TEST_F(FeatureAggregatorImplTest, BucketedCumulativeCountAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::BUCKETED_CUMULATIVE_COUNT,
            {2, 5, 9, 10, 10, 15}, {2, 5, 9, 10, 10, 15}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_CUMULATIVE_COUNT,
            {2, 5, 9, 10, 10, 15}, {2, 5, 9, 10, 10, 15}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_CUMULATIVE_COUNT,
            {2, 5, 9, 10, 10, 15}, {2, 5, 9, 10, 10, 15}, {0, 0, 0, 0, 0, 0});
}

TEST_F(FeatureAggregatorImplTest, SumAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::SUM, {15}, {15}, {0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::SUM, {120}, {0}, {0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::SUM, {120}, {0}, {0});
}

TEST_F(FeatureAggregatorImplTest, SumBooleanAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::SUM_BOOLEAN, {1}, {1}, {0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::SUM_BOOLEAN, {1}, {0},
            {0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::SUM_BOOLEAN, {1}, {0},
            {0});
}

TEST_F(FeatureAggregatorImplTest, BucketedSumAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::BUCKETED_SUM,
            {2, 3, 4, 1, 0, 5}, {2, 3, 4, 1, 0, 5}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_SUM,
            {3, 12, 30, 10, 0, 65}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_SUM,
            {3, 12, 30, 10, 0, 65}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});
}

TEST_F(FeatureAggregatorImplTest, BucketedSumBooleanAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::BUCKETED_SUM_BOOLEAN,
            {1, 1, 1, 1, 0, 1}, {1, 1, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_SUM_BOOLEAN,
            {1, 1, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_SUM_BOOLEAN,
            {1, 1, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});
}

TEST_F(FeatureAggregatorImplTest, BucketedSumBooleanTrueCountAggregation) {
  VerifyAll(SignalType::USER_ACTION,
            Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT, {5}, {5}, {0});

  VerifyAll(SignalType::HISTOGRAM_ENUM,
            Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT, {5}, {0}, {0});

  VerifyAll(SignalType::HISTOGRAM_VALUE,
            Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT, {5}, {0}, {0});
}

TEST_F(FeatureAggregatorImplTest, BucketedCumulativeSumAggregation) {
  VerifyAll(SignalType::USER_ACTION, Aggregation::BUCKETED_CUMULATIVE_SUM,
            {2, 5, 9, 10, 10, 15}, {2, 5, 9, 10, 10, 15}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_CUMULATIVE_SUM,
            {3, 15, 45, 55, 55, 120}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});

  VerifyAll(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_CUMULATIVE_SUM,
            {3, 15, 45, 55, 55, 120}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});
}

TEST_F(FeatureAggregatorImplTest, LatestOrDefaultAggregation) {
  VerifyAllOptional(SignalType::USER_ACTION, Aggregation::LATEST_OR_DEFAULT,
                    std::optional<std::vector<float>>{{15}},
                    std::optional<std::vector<float>>{{0}}, std::nullopt);

  VerifyAllOptional(SignalType::HISTOGRAM_ENUM, Aggregation::LATEST_OR_DEFAULT,
                    std::optional<std::vector<float>>{{15}},
                    std::optional<std::vector<float>>{{0}}, std::nullopt);

  VerifyAllOptional(SignalType::HISTOGRAM_VALUE, Aggregation::LATEST_OR_DEFAULT,
                    std::optional<std::vector<float>>{{15}},
                    std::optional<std::vector<float>>{{0}}, std::nullopt);
}

TEST_F(FeatureAggregatorImplTest, BucketizationThresholds) {
  std::vector<Sample> samples{
      // First sample is exactly 1 day ago, part of second bucket.
      {clock_.Now() - base::Days(1), 1},
      // Second sample is just over 1 day ago, part of second bucket.
      {clock_.Now() - base::Days(1) - base::Seconds(1), 2},
      // Second sample is just under 1 day ago, part of first bucket.
      {clock_.Now() - base::Days(1) + base::Seconds(1), 3},
  };

  Verify(SignalType::USER_ACTION, Aggregation::BUCKETED_COUNT, 2, base::Days(1),
         samples, std::optional<std::vector<float>>({1, 2}));
  Verify(SignalType::USER_ACTION, Aggregation::BUCKETED_SUM, 2, base::Days(1),
         samples, std::optional<std::vector<float>>({1, 2}));
  Verify(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_COUNT, 2,
         base::Days(1), samples, std::optional<std::vector<float>>({1, 2}));
  Verify(SignalType::HISTOGRAM_ENUM, Aggregation::BUCKETED_SUM, 2,
         base::Days(1), samples, std::optional<std::vector<float>>({3, 3}));
  Verify(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_COUNT, 2,
         base::Days(1), samples, std::optional<std::vector<float>>({1, 2}));
  Verify(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_SUM, 2,
         base::Days(1), samples, std::optional<std::vector<float>>({3, 3}));
}

TEST_F(FeatureAggregatorImplTest, BucketsOutOfBounds) {
  std::vector<Sample> samples{
      {clock_.Now() + base::Days(1), 1},  // In the future.
      {clock_.Now(), 2},
      {clock_.Now() - base::Days(1), 3},
      {clock_.Now() - base::Days(2), 4},
      {clock_.Now() - base::Days(3), 5},  // Too old.
  };

  // Using bucket count of 3, means the first sample is out of bounds for being
  // in the future, and the last sample is out of bounds for being too old.
  Verify(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_COUNT, 3,
         base::Days(1), samples, std::optional<std::vector<float>>({1, 1, 1}));
  Verify(SignalType::HISTOGRAM_VALUE, Aggregation::BUCKETED_SUM, 3,
         base::Days(1), samples, std::optional<std::vector<float>>({2, 3, 4}));
}

}  // namespace segmentation_platform::processing
