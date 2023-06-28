// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_MIGRATION_TEST_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_MIGRATION_TEST_UTILS_H_

#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform::migration_test_utils {

std::unique_ptr<Config> GetTestConfigForBinaryClassifier(
    const std::string& segmentation_key,
    const std::string& segmentation_uma_name,
    proto::SegmentId segment_id);

std::unique_ptr<Config> GetTestConfigForAdaptiveToolbar();

proto::OutputConfig GetTestOutputConfigForBinaryClassifier(
    proto::SegmentId segment_id);

proto::OutputConfig GetTestOutputConfigForAdaptiveToolbar();

}  // namespace segmentation_platform::migration_test_utils

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_MIGRATION_TEST_UTILS_H_
