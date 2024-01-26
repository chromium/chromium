// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_AGGREGATOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_AGGREGATOR_H_

#include <cstdint>
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
  // time frame. Returns std::nullopt if the aggregation cannot be applied on
  // the provided samples.
  virtual std::optional<std::vector<float>> Process(
      proto::SignalType signal_type,
      uint64_t name_hash,
      proto::Aggregation aggregation,
      uint64_t bucket_count,
      const base::Time& start_time,
      const base::Time& end_time,
      const base::TimeDelta& bucket_duration,
      const std::vector<int32_t>& accepted_enum_ids,
      const std::vector<SignalDatabase::DbEntry>& all_samples) const = 0;
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_AGGREGATOR_H_
