// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/features.h"

namespace page_image_service {

// Enabled by default because we are only using this as a killswitch.
BASE_FEATURE(kImageService, base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled the capability by default, can be used as a killswitch.
BASE_FEATURE(kImageServiceSuggestPoweredImages,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled the capability by default, can be used as a killswitch.
BASE_FEATURE(kImageServiceOptimizationGuideSalientImages,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace page_image_service
