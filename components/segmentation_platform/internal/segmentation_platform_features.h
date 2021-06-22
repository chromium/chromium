// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_FEATURES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_FEATURES_H_

#include "base/time/time.h"

namespace segmentation_platform {
namespace features {

// Used to determine if the model was executed too recently to run again.
base::TimeDelta GetMinDelayForModelRerun();

// Time to live for a segment selection. Segment selection can't be changed
// before this duration.
base::TimeDelta GetSegmentSelectionTTL();

}  // namespace features
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_FEATURES_H_
