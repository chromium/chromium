// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "base/notreached.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
using Sample = SignalDatabase::Sample;

namespace {
std::vector<float> SumCountAggregation(const std::vector<Sample>& samples) {
  return std::vector<float>{samples.size()};
}

std::vector<float> SumValuesAggregation(proto::SignalType signal_type,
                                        const std::vector<Sample>& samples) {
  if (signal_type == proto::SignalType::USER_ACTION)
    return SumCountAggregation(samples);

  float sum = 0;
  for (auto& sample : samples)
    sum += sample.second.value();

  return std::vector<float>{sum};
}
}  // namespace

FeatureAggregatorImpl::FeatureAggregatorImpl() = default;

FeatureAggregatorImpl::~FeatureAggregatorImpl() = default;

std::vector<float> FeatureAggregatorImpl::Process(
    proto::SignalType signal_type,
    proto::Aggregation aggregation,
    uint64_t length,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<SignalDatabase::Sample>& samples) const {
  switch (aggregation) {
    case proto::Aggregation::SUM_COUNT:
      return SumCountAggregation(samples);
    case proto::Aggregation::SUM_VALUES:
      return SumValuesAggregation(signal_type, samples);
    case proto::Aggregation::BUCKETED_COUNT:
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN:
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT:
    case proto::Aggregation::BUCKETED_CUMULATIVE_COUNT:
    case proto::Aggregation::SUM_VALUES_BOOLEAN:
    case proto::Aggregation::BUCKETED_CUMULATIVE_SUM_VALUES:
    case proto::Aggregation::BUCKETED_SUM_VALUES:
    case proto::Aggregation::UNKNOWN:
      // TODO(nyquist): Implement the rest of the aggregations.
      NOTREACHED();
      return std::vector<float>();
  }
}

void FeatureAggregatorImpl::FilterEnumSamples(
    const std::vector<uint32_t>& accepted_enum_values,
    std::vector<SignalDatabase::Sample>& samples) const {
  if (accepted_enum_values.size() == 0)
    return;

  auto new_end = std::remove_if(
      samples.begin(), samples.end(),
      [&accepted_enum_values](SignalDatabase::Sample sample) {
        DCHECK(sample.second.has_value());
        auto found =
            std::find(accepted_enum_values.begin(), accepted_enum_values.end(),
                      sample.second.value()) != accepted_enum_values.end();
        return !found;
      });
  samples.erase(new_end, samples.end());
}

}  // namespace segmentation_platform
