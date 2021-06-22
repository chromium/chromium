// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {
namespace features {

// Core feature flag for segmentation platform.
const base::Feature kSegmentationPlatformFeature{
    "SegmentationPlatform", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features

}  // namespace segmentation_platform
