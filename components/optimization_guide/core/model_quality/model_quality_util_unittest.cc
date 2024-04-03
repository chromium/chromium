// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_util.h"

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class GetModelExecutionFeatureTest : public testing::Test {};

TEST_F(GetModelExecutionFeatureTest, GetModelExecutionFeature) {
  EXPECT_EQ(
      UserVisibleFeatureKey::kCompose,
      GetModelExecutionFeature(proto::LogAiDataRequest::FeatureCase::kCompose));
  EXPECT_EQ(UserVisibleFeatureKey::kTabOrganization,
            GetModelExecutionFeature(
                proto::LogAiDataRequest::FeatureCase::kTabOrganization));
  EXPECT_EQ(UserVisibleFeatureKey::kWallpaperSearch,
            GetModelExecutionFeature(
                proto::LogAiDataRequest::FeatureCase::kWallpaperSearch));
}

}  // namespace optimization_guide
