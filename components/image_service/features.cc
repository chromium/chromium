// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_service/features.h"

namespace image_service {

// Enabled by default because we are only using this as a killswitch.
BASE_FEATURE(kImageService, "ImageService", base::FEATURE_ENABLED_BY_DEFAULT);

// Disabled by default because the usage of this is still not approved.
BASE_FEATURE(kImageServiceSuggestPoweredImages,
             "ImageServiceSuggestPoweredImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disabled by default, usage is approved but we still want to control rollout.
BASE_FEATURE(kImageServiceOptimizationGuideSalientImages,
             "ImageServiceOptimizationGuideSalientImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace image_service
