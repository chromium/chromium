// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_features.h"

#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class SegmentationPlatformFeaturesTest : public testing::Test {
 public:
  SegmentationPlatformFeaturesTest() = default;
  ~SegmentationPlatformFeaturesTest() override = default;
};

TEST_F(SegmentationPlatformFeaturesTest, DefaultValues) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSegmentationPlatformFeature);
  EXPECT_EQ(base::TimeDelta::FromSeconds(43200),
            features::GetMinDelayForModelRerun());
  EXPECT_EQ(base::TimeDelta::FromDays(28), features::GetSegmentSelectionTTL());
}

}  // namespace segmentation_platform
