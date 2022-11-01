// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/dummy_training_data_collector.h"

namespace segmentation_platform {

DummyTrainingDataCollector::DummyTrainingDataCollector() = default;

DummyTrainingDataCollector::~DummyTrainingDataCollector() = default;

void DummyTrainingDataCollector::OnModelMetadataUpdated() {}

void DummyTrainingDataCollector::OnServiceInitialized() {}

void DummyTrainingDataCollector::ReportCollectedContinuousTrainingData() {}

void DummyTrainingDataCollector::OnDecisionTime(
    proto::SegmentId id,
    scoped_refptr<InputContext> input_context,
    DecisionType type) {}

void DummyTrainingDataCollector::OnObservationTrigger(
    TrainingDataCache::RequestId request_id,
    const proto::SegmentInfo& segment_info) {}

}  // namespace segmentation_platform
