// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "components/optimization_guide/core/model_quality/model_quality_util.h"

namespace optimization_guide {

class GetModelExecutionFeatureTest : public testing::Test {};

TEST_F(GetModelExecutionFeatureTest, GetModelExecutionFeature) {
  EXPECT_EQ(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE,
      GetModelExecutionFeature(proto::LogAiDataRequest::FeatureCase::kCompose));
  EXPECT_EQ(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION,
      GetModelExecutionFeature(
          proto::LogAiDataRequest::FeatureCase::kTabOrganization));
  EXPECT_EQ(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH,
      GetModelExecutionFeature(
          proto::LogAiDataRequest::FeatureCase::kWallpaperSearch));
}

}  // namespace optimization_guide
