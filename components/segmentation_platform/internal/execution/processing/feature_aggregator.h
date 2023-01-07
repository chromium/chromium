// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_AGGREGATOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_AGGREGATOR_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform::processing {

// The FeatureAggregator is able to convert metadata and a vector of samples
// into a vector of resulting floats that the ML model accepts.
class FeatureAggregator {
 public:
  virtual ~FeatureAggregator() = default;

  // Calculate the aggregated result for the given feature metadata and samples.
  // Assumes that the all the provided samples are valid within the required
  // time frame. Returns absl::nullopt if the aggregation cannot be applied on
  // the provided samples.
  virtual absl::optional<std::vector<float>> Process(
      proto::SignalType signal_type,
      proto::Aggregation aggregation,
      uint64_t length,
      const base::Time& end_time,
      const base::TimeDelta& bucket_duration,
      const std::vector<SignalDatabase::Sample>& samples) const = 0;

  // Removes all enum samples that are not accepted. If |accepted_enum_values|
  // is empty, all values are accepted. Note: This modifies the input |samples|.
  virtual void FilterEnumSamples(
      const std::vector<int32_t>& accepted_enum_ids,
      std::vector<SignalDatabase::Sample>& samples) const = 0;
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_AGGREGATOR_H_
