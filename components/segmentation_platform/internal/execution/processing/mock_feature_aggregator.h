// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_MOCK_FEATURE_AGGREGATOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_MOCK_FEATURE_AGGREGATOR_H_

#include <vector>

#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform::processing {

// Mock of feature aggregator class. Used for testing.
class MockFeatureAggregator : public FeatureAggregator {
 public:
  MockFeatureAggregator();
  ~MockFeatureAggregator() override;
  MOCK_METHOD(std::optional<std::vector<float>>,
              Process,
              (proto::SignalType signal_type,
               uint64_t name_hash,
               proto::Aggregation aggregation,
               uint64_t bucket_count,
               const base::Time& start_time,
               const base::Time& end_time,
               const base::TimeDelta& bucket_duration,
               const std::vector<int32_t>& accepted_enum_ids,
               const std::vector<SignalDatabase::DbEntry>& all_samples),
              (const override));
  MOCK_METHOD(void,
              FilterEnumSamples,
              (const std::vector<int32_t>& accepted_enum_ids,
               std::vector<SignalDatabase::Sample>& samples),
              (const override));
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_MOCK_FEATURE_AGGREGATOR_H_
