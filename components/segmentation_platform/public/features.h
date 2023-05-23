// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/strings/string_piece.h"

namespace segmentation_platform::features {

// Core feature flag for segmentation platform.
BASE_DECLARE_FEATURE(kSegmentationPlatformFeature);

// Feature flag for enabling UKM based engine.
BASE_DECLARE_FEATURE(kSegmentationPlatformUkmEngine);

// Feature flag for enabling adaptive toolbar v2 multi-output model.
BASE_DECLARE_FEATURE(kSegmentationPlatformAdaptiveToolbarV2Feature);

// Feature flag for enabling low engagement segmentation key.
BASE_DECLARE_FEATURE(kSegmentationPlatformLowEngagementFeature);

// Feature flag for enabling Feed user segments feature.
BASE_DECLARE_FEATURE(kSegmentationPlatformFeedSegmentFeature);

// Feature flag for enabling categorization into resume heavy user.
BASE_DECLARE_FEATURE(kResumeHeavyUserSegmentFeature);

// Feature flag for enabling Power user segmentation.
BASE_DECLARE_FEATURE(kSegmentationPlatformPowerUserFeature);

// Feature flag for enabling frequent feature user segment.
BASE_DECLARE_FEATURE(kFrequentFeatureUserSegmentFeature);

// Feature flag for enabling contextual page actions. Only effective when at
// least one action is enabled.
BASE_DECLARE_FEATURE(kContextualPageActions);

// Feature flag for enabling search user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformSearchUser);

// Feature flag for device switcher segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformDeviceSwitcher);

// Feature flag for enabling price tracking action feature.
BASE_DECLARE_FEATURE(kContextualPageActionPriceTracking);

// Feature flag for enabling reader mode action feature.
BASE_DECLARE_FEATURE(kContextualPageActionReaderMode);

// Feature flag for enabling reader mode action feature.
BASE_DECLARE_FEATURE(kContextualPageActionShareModel);

// Feature flag for enabling shopping user segment feature.
BASE_DECLARE_FEATURE(kShoppingUserSegmentFeature);

// Feature flag for enabling default reporting segments.
BASE_DECLARE_FEATURE(kSegmentationDefaultReportingSegments);

// Feature flag for enabling device tier segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformDeviceTier);

// Feature flag for enabling tablet productivity user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformTabletProductivityUser);

// Feature flag for enabling model execution report sampling.
BASE_DECLARE_FEATURE(kSegmentationPlatformModelExecutionSampling);
}  // namespace segmentation_platform::features

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
