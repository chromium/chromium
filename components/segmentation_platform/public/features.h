// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/strings/string_piece.h"

namespace segmentation_platform::features {

// Core feature flag for segmentation platform.
extern const base::Feature kSegmentationPlatformFeature;

// Feature flag for segmentation platform dummy model that is used for
// experimental models and data collection.
extern const base::Feature kSegmentationPlatformDummyFeature;

// Feature flag for allowing structured metrics to be collected.
extern const base::Feature kSegmentationStructuredMetricsFeature;

// Feature flag for enabling UKM based engine.
extern const base::Feature kSegmentationPlatformUkmEngine;

// Feature flag for enabling low engagement segmentation key.
extern const base::Feature kSegmentationPlatformLowEngagementFeature;

// Feature flag for enabling Feed user segments feature.
extern const base::Feature kSegmentationPlatformFeedSegmentFeature;

// Feature flag for enabling contextual page actions. Only effective when at
// least one action is enabled.
extern const base::Feature kContextualPageActions;

// Feature flag for enabling price tracking action feature.
extern const base::Feature kContextualPageActionPriceTracking;

// Feature flag for enabling shopping user segment feature.
extern const base::Feature kShoppingUserSegmentFeature;

// Feature flag for enabling `SegmentInfoCache` for `SegmentInfoDatabase`.
extern const base::Feature kSegmentationPlatformSegmentInfoCache;

}  // namespace segmentation_platform::features

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
