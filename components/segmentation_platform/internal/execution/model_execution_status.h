// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_STATUS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_STATUS_H_

namespace segmentation_platform {

// Success or failure states resulting from the model execution.
// Keep up to date with SegmentationPlatformModelExecutionStatus in
// //tools/metrics/histograms/enums.xml.
enum class ModelExecutionStatus {
  kSuccess = 0,
  kExecutionError = 1,
  kInvalidMetadata = 2,
  kMaxValue = kInvalidMetadata,
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_STATUS_H_
