// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_METADATA_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_METADATA_UTILS_H_

#include "base/time/time.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"

namespace segmentation_platform {
namespace metadata_utils {

enum ValidationResult {
  VALIDATION_SUCCESS = 0,
  SEGMENT_ID_NOT_FOUND = 1,
  METADATA_NOT_FOUND = 2,
  TIME_UNIT_INVALID = 3,
};

// Whether the given SegmentInfo and its metadata is valid to be used for the
// current segmentation platform.
ValidationResult ValidateSegmentInfo(const proto::SegmentInfo& segment_info);

// Whether the given metadata is valid to be used for the current segmentation
// platform.
ValidationResult ValidateMetadata(
    const proto::SegmentationModelMetadata& model_metadata);

// Whether a segment has expired results or no result. Called to determine
// whether the model should be rerun.
bool HasExpiredOrUnavailableResult(const proto::SegmentInfo& segment_info);

// Whether the results were computed too recently for a given segment. If
// true, the model execution should be skipped for the time being.
bool HasFreshResults(const proto::SegmentInfo& segment_info);

// Helper method to read the time unit from the proto.
base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata);

}  // namespace metadata_utils
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_METADATA_UTILS_H_
