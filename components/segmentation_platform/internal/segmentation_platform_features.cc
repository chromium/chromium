// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_features.h"

#include "base/feature_list.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {
namespace features {
namespace {

// Default min delay in seconds between two successful executions of a model.
// Default is 12 hours.
constexpr int kDefaultMinDelayForModelRerunSeconds = 43200;

// Default TTL for segment selection.
constexpr int kDefaultSegmentSelectionTTLDays = 28;

}  // namespace

base::TimeDelta GetMinDelayForModelRerun() {
  int min_delay_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kSegmentationPlatformFeature, "min_delay_for_model_rerun_seconds",
      kDefaultMinDelayForModelRerunSeconds);

  return base::TimeDelta::FromSeconds(min_delay_seconds);
}

base::TimeDelta GetSegmentSelectionTTL() {
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      kSegmentationPlatformFeature, "segment_selection_ttl_days",
      kDefaultSegmentSelectionTTLDays);

  return base::TimeDelta::FromDays(segment_selection_ttl_days);
}

}  // namespace features
}  // namespace segmentation_platform
