// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_aggregator_impl.h"

#include <cstdint>
#include <vector>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform::processing {
using Sample = SignalDatabase::Sample;

namespace {
std::vector<std::vector<Sample>> Bucketize(
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  std::vector<std::vector<Sample>> bucketized_samples(bucket_count);
  for (auto& sample : samples) {
    const base::Time& timestamp = sample.first;
    base::TimeDelta time_since_now = end_time - timestamp;
    int bucket_index = time_since_now / bucket_duration;

    // Ignore out-of-bounds samples.
    if (bucket_index < 0 || base::saturated_cast<uint32_t>(bucket_index) >=
                                bucketized_samples.size()) {
      continue;
    }

    bucketized_samples[bucket_index].emplace_back(sample);
  }

  return bucketized_samples;
}

int64_t SumValues(proto::SignalType signal_type,
                  const std::vector<Sample>& samples) {
  if (signal_type == proto::SignalType::USER_ACTION)
    return base::saturated_cast<int64_t>(samples.size());

  int64_t sum = 0;
  for (auto& sample : samples)
    sum = base::ClampAdd(sum, sample.second);

  return sum;
}

std::vector<float> CountAggregation(const std::vector<Sample>& samples) {
  return {static_cast<float>(samples.size())};
}

std::vector<float> CountBooleanAggregation(const std::vector<Sample>& samples) {
  return {static_cast<float>(samples.size() > 0 ? 1 : 0)};
}

std::vector<float> BucketedCountAggregation(
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  std::vector<float> tensor_data;
  for (auto& bucket : bucketized_samples)
    tensor_data.emplace_back(static_cast<float>(bucket.size()));

  return tensor_data;
}

std::vector<float> BucketedCountBooleanAggregation(
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  std::vector<float> tensor_data;
  for (auto& bucket : bucketized_samples)
    tensor_data.emplace_back(static_cast<float>(bucket.size() > 0 ? 1 : 0));

  return tensor_data;
}

std::vector<float> BucketedCountBooleanTrueCountAggregation(
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  int64_t true_count = 0;
  for (auto& bucket : bucketized_samples) {
    if (bucket.size() > 0)
      true_count = base::ClampAdd(true_count, 1);
  }

  return {static_cast<float>(true_count)};
}

std::vector<float> BucketedCumulativeCountAggregation(
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  int64_t cumulative_count = 0;
  std::vector<float> tensor_data;
  for (auto& bucket : bucketized_samples) {
    cumulative_count = base::ClampAdd(cumulative_count, bucket.size());
    tensor_data.emplace_back(static_cast<float>(cumulative_count));
  }

  return tensor_data;
}

std::vector<float> SumAggregation(proto::SignalType signal_type,
                                  const std::vector<Sample>& samples) {
  return {static_cast<float>(SumValues(signal_type, samples))};
}

std::vector<float> SumBooleanAggregation(proto::SignalType signal_type,
                                         const std::vector<Sample>& samples) {
  return SumValues(signal_type, samples) > 0 ? std::vector<float>{1}
                                             : std::vector<float>{0};
}

std::vector<float> BucketedSumAggregation(
    proto::SignalType signal_type,
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  std::vector<float> tensor_data;
  for (auto& bucket : bucketized_samples) {
    tensor_data.emplace_back(
        static_cast<float>(SumValues(signal_type, bucket)));
  }

  return tensor_data;
}

std::vector<float> BucketedSumBooleanAggregation(
    proto::SignalType signal_type,
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  std::vector<float> tensor_data;
  for (auto& bucket : bucketized_samples) {
    tensor_data.emplace_back(
        static_cast<float>(SumValues(signal_type, bucket) > 0 ? 1 : 0));
  }

  return tensor_data;
}

std::vector<float> BucketedSumBooleanTrueCountAggregation(
    proto::SignalType signal_type,
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  int64_t true_count = 0;
  for (auto& bucket : bucketized_samples) {
    if (SumValues(signal_type, bucket) > 0)
      true_count = base::ClampAdd(true_count, 1);
  }

  return std::vector<float>{static_cast<float>(true_count)};
}

std::vector<float> BucketedCumulativeSumAggregation(
    proto::SignalType signal_type,
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) {
  auto bucketized_samples =
      Bucketize(bucket_count, end_time, bucket_duration, samples);

  int64_t cumulative_sum = 0;
  std::vector<float> tensor_data;
  for (auto& bucket : bucketized_samples) {
    cumulative_sum =
        base::ClampAdd(cumulative_sum, SumValues(signal_type, bucket));
    tensor_data.emplace_back(static_cast<float>(cumulative_sum));
  }

  return tensor_data;
}

}  // namespace

FeatureAggregatorImpl::FeatureAggregatorImpl() = default;

FeatureAggregatorImpl::~FeatureAggregatorImpl() = default;

absl::optional<std::vector<float>> FeatureAggregatorImpl::Process(
    proto::SignalType signal_type,
    proto::Aggregation aggregation,
    uint64_t bucket_count,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<Sample>& samples) const {
  switch (aggregation) {
    case proto::Aggregation::UNKNOWN:
      NOTREACHED();
      return std::vector<float>();
    case proto::Aggregation::COUNT:
      return CountAggregation(samples);
    case proto::Aggregation::COUNT_BOOLEAN:
      return CountBooleanAggregation(samples);
    case proto::Aggregation::BUCKETED_COUNT:
      return BucketedCountAggregation(bucket_count, end_time, bucket_duration,
                                      samples);
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN:
      return BucketedCountBooleanAggregation(bucket_count, end_time,
                                             bucket_duration, samples);
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT:
      return BucketedCountBooleanTrueCountAggregation(bucket_count, end_time,
                                                      bucket_duration, samples);
    case proto::Aggregation::BUCKETED_CUMULATIVE_COUNT:
      return BucketedCumulativeCountAggregation(bucket_count, end_time,
                                                bucket_duration, samples);
    case proto::Aggregation::SUM:
      return SumAggregation(signal_type, samples);
    case proto::Aggregation::SUM_BOOLEAN:
      return SumBooleanAggregation(signal_type, samples);
    case proto::Aggregation::BUCKETED_SUM:
      return BucketedSumAggregation(signal_type, bucket_count, end_time,
                                    bucket_duration, samples);
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN:
      return BucketedSumBooleanAggregation(signal_type, bucket_count, end_time,
                                           bucket_duration, samples);
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT:
      return BucketedSumBooleanTrueCountAggregation(
          signal_type, bucket_count, end_time, bucket_duration, samples);
    case proto::Aggregation::BUCKETED_CUMULATIVE_SUM:
      return BucketedCumulativeSumAggregation(
          signal_type, bucket_count, end_time, bucket_duration, samples);
    case proto::Aggregation::LATEST_OR_DEFAULT:
      if (samples.empty()) {
        // If empty, then latest data cannot be found.
        return absl::nullopt;
      }
      return std::vector<float>(
          {static_cast<float>(samples[samples.size() - 1].second)});
  }
}

void FeatureAggregatorImpl::FilterEnumSamples(
    const std::vector<int32_t>& accepted_enum_ids,
    std::vector<Sample>& samples) const {
  if (accepted_enum_ids.size() == 0)
    return;

  auto new_end = std::remove_if(
      samples.begin(), samples.end(), [&accepted_enum_ids](Sample sample) {
        return !base::Contains(accepted_enum_ids, sample.second);
      });
  samples.erase(new_end, samples.end());
}

}  // namespace segmentation_platform::processing
