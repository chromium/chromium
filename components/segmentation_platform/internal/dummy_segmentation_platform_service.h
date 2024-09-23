// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_SEGMENTATION_PLATFORM_SERVICE_H_

#include <string>

#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

// A dummy implementation of the SegmentationPlatformService that can be
// returned whenever the feature is not enabled.
class DummySegmentationPlatformService : public SegmentationPlatformService {
 public:
  DummySegmentationPlatformService();
  ~DummySegmentationPlatformService() override;

  // Disallow copy/assign.
  DummySegmentationPlatformService(const DummySegmentationPlatformService&) =
      delete;
  DummySegmentationPlatformService& operator=(
      const DummySegmentationPlatformService&) = delete;

  // SegmentationPlatformService overrides.
  void GetSelectedSegment(const std::string& segmentation_key,
                          SegmentSelectionCallback callback) override;
  void GetClassificationResult(const std::string& segmentation_key,
                               const PredictionOptions& prediction_options,
                               scoped_refptr<InputContext> input_context,
                               ClassificationResultCallback callback) override;
  void GetAnnotatedNumericResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      AnnotatedNumericResultCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) override;
  void CollectTrainingData(proto::SegmentId segment_id,
                           TrainingRequestId request_id,
                           const TrainingLabels& param,
                           SuccessCallback callback) override;
  void CollectTrainingData(proto::SegmentId segment_id,
                           TrainingRequestId request_id,
                           ukm::SourceId ukm_source_id,
                           const TrainingLabels& param,
                           SuccessCallback callback) override;
  void EnableMetrics(bool signal_collection_allowed) override;
  bool IsPlatformInitialized() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_SEGMENTATION_PLATFORM_SERVICE_H_
