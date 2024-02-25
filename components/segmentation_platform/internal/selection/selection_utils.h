// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SELECTION_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SELECTION_UTILS_H_

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {
using proto::SegmentId;

namespace selection_utils {

// Converts ResultState to PredictionStatus.
PredictionStatus ResultStateToPredictionStatus(
    SegmentResultProvider::ResultState result_state);

}  // namespace selection_utils
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SELECTION_UTILS_H_
