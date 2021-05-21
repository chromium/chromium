// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"

namespace segmentation_platform {

SelectedSegment::SelectedSegment(OptimizationTarget segment_id)
    : segment_id(segment_id), in_use(false) {}

}  // namespace segmentation_platform
