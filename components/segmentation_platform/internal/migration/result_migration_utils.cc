// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/result_migration_utils.h"

#include <string>

#include "components/segmentation_platform/internal/migration/adaptive_toolbar_migration.h"
#include "components/segmentation_platform/internal/migration/binary_classifier_migration.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"

namespace segmentation_platform::pref_migration_utils {

proto::ClientResult CreateClientResultFromOldResult(
    Config* config,
    const SelectedSegment& old_result) {
  if (GetClassifierTypeForMigration(config->segmentation_key) ==
      proto::Predictor::kBinaryClassifier) {
    return pref_migration_utils::CreateClientResultForBinaryClassifier(
        config, old_result);
  } else if (config->segmentation_key == kAdaptiveToolbarSegmentationKey) {
    return pref_migration_utils::CreateClientResultForAdaptiveToolbar(
        config, old_result);
  } else {
    NOTREACHED_IN_MIGRATION();
    return proto::ClientResult();
  }
}

proto::Predictor::PredictorTypeCase GetClassifierTypeForMigration(
    const std::string& segmentation_key) {
  if (segmentation_key == kAdaptiveToolbarSegmentationKey ||
      segmentation_key == kContextualPageActionsKey) {
    return proto::Predictor::kMultiClassClassifier;
  } else if (segmentation_key == kChromeLowUserEngagementSegmentationKey ||
             segmentation_key == kCrossDeviceUserKey ||
             segmentation_key == kDeviceSwitcherKey ||
             segmentation_key == kFrequentFeatureUserKey ||
             segmentation_key == kIntentionalUserKey ||
             segmentation_key == kResumeHeavyUserKey ||
             segmentation_key == kShoppingUserSegmentationKey) {
    return proto::Predictor::kBinaryClassifier;
  } else if (segmentation_key == kFeedUserSegmentationKey ||
             segmentation_key == kPowerUserKey ||
             segmentation_key == kSearchUserKey ||
             segmentation_key == kDeviceTierKey ||
             segmentation_key == kTabletProductivityUserKey) {
    return proto::Predictor::kBinnedClassifier;
  }
  // This case is reached for non-legacy models, and it is ok to return
  // regressor because migration is not required for these cases.
  return proto::Predictor::kRegressor;
}

}  // namespace segmentation_platform::pref_migration_utils
