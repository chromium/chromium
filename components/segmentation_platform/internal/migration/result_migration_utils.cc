// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/result_migration_utils.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/migration/adaptive_toolbar_migration.h"
#include "components/segmentation_platform/internal/migration/binary_classifier_migration.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform::pref_migration_utils {

proto::ClientResult CreateClientResultFromOldResult(
    Config* config,
    const SelectedSegment& old_result) {
  if (GetClassifierType(config->segmentation_key) ==
      proto::Predictor::kBinaryClassifier) {
    return pref_migration_utils::CreateClientResultForBinaryClassifier(
        config, old_result);
  } else if (config->segmentation_key == kAdaptiveToolbarSegmentationKey) {
    return pref_migration_utils::CreateClientResultForAdaptiveToolbar(
        config, old_result);
  } else {
    NOTREACHED();
    return proto::ClientResult();
  }
}

}  // namespace segmentation_platform::pref_migration_utils
