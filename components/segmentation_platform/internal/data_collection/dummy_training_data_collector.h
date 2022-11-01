// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_DUMMY_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_DUMMY_TRAINING_DATA_COLLECTOR_H_

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

namespace segmentation_platform {

// Dummy TrainingDataCollector implementation that does nothing, used when
// kSegmentationStructuredMetricsFeature is disabled.
class DummyTrainingDataCollector : public TrainingDataCollector {
 public:
  DummyTrainingDataCollector();
  ~DummyTrainingDataCollector() override;

  // TrainingDataCollector implementation.
  void OnModelMetadataUpdated() override;
  void OnServiceInitialized() override;
  void ReportCollectedContinuousTrainingData() override;
  void OnDecisionTime(proto::SegmentId id,
                      scoped_refptr<InputContext> input_context,
                      DecisionType type) override;
  void OnObservationTrigger(TrainingDataCache::RequestId request_id,
                            const proto::SegmentInfo& segment_info) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_DUMMY_TRAINING_DATA_COLLECTOR_H_
