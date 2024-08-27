// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"

#include <string>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform {

DummySegmentationPlatformService::DummySegmentationPlatformService() = default;

DummySegmentationPlatformService::~DummySegmentationPlatformService() = default;

void DummySegmentationPlatformService::GetSelectedSegment(
    const std::string& segmentation_key,
    SegmentSelectionCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SegmentSelectionResult()));
}

void DummySegmentationPlatformService::GetClassificationResult(
    const std::string& segmentation_key,
    const PredictionOptions& prediction_options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     ClassificationResult(PredictionStatus::kFailed)));
}

void DummySegmentationPlatformService::GetAnnotatedNumericResult(
    const std::string& segmentation_key,
    const PredictionOptions& prediction_options,
    scoped_refptr<InputContext> input_context,
    AnnotatedNumericResultCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     AnnotatedNumericResult(PredictionStatus::kFailed)));
}

SegmentSelectionResult DummySegmentationPlatformService::GetCachedSegmentResult(
    const std::string& segmentation_key) {
  return SegmentSelectionResult();
}

void DummySegmentationPlatformService::CollectTrainingData(
    proto::SegmentId segment_id,
    TrainingRequestId request_id,
    const TrainingLabels& param,
    SuccessCallback callback) {}

void DummySegmentationPlatformService::CollectTrainingData(
    proto::SegmentId segment_id,
    TrainingRequestId request_id,
    ukm::SourceId ukm_source_id,
    const TrainingLabels& param,
    SuccessCallback callback) {}

void DummySegmentationPlatformService::EnableMetrics(
    bool signal_collection_allowed) {}

bool DummySegmentationPlatformService::IsPlatformInitialized() {
  return false;
}

}  // namespace segmentation_platform
