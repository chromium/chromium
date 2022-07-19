// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/features.h"

#include "build/build_config.h"

namespace segmentation_platform::features {

const base::Feature kSegmentationPlatformFeature{
    "SegmentationPlatform", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSegmentationPlatformDummyFeature{
    "SegmentationPlatformDummyFeature", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSegmentationStructuredMetricsFeature{
    "SegmentationStructuredMetrics", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSegmentationPlatformUkmEngine{
    "SegmentationPlatformUkmEngine", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSegmentationPlatformLowEngagementFeature{
    "SegmentationPlatformLowEngagementFeature",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSegmentationPlatformFeedSegmentFeature{
  "SegmentationPlatformFeedSegmentFeature",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
};
#else
      base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

const base::Feature kContextualPageActions{"ContextualPageActions",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContextualPageActionPriceTracking{
    "ContextualPageActionPriceTracking", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace segmentation_platform::features
