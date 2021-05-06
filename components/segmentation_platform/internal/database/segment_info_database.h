// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_

#include <vector>

#include "base/callback.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

// Represents a DB layer that stores model metadata and prediction results to
// the disk.
class SegmentInfoDatabase {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using AllSegmentInfoCallback = base::OnceCallback<void(
      std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>)>;

  virtual ~SegmentInfoDatabase() = default;

  // TODO(shaktisahu): Initialize DB before instantiating dependent classes.

  // Convenient method to return combined info for all the segments in the
  // database.
  virtual void GetAllSegmentInfo(AllSegmentInfoCallback callback) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_
