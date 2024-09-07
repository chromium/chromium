// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features.h"

#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using features::internal::GetAllowedFeaturesForUnsignedUser;
using testing::UnorderedElementsAre;

TEST(ModelExecutionFeature, GetAllowedFeaturesForUnsignedUser) {
  EXPECT_THAT(GetAllowedFeaturesForUnsignedUser(), UnorderedElementsAre());
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{features::internal::kComposeSettingsVisibility,
          {{"allow_unsigned_user", "true"}}},
         {features::internal::kTabOrganizationSettingsVisibility,
          {{"allow_unsigned_user", "true"}}}},
        {});
    EXPECT_THAT(GetAllowedFeaturesForUnsignedUser(),
                UnorderedElementsAre(UserVisibleFeatureKey::kCompose,
                                     UserVisibleFeatureKey::kTabOrganization));
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{features::internal::kComposeSettingsVisibility,
          {{"allow_unsigned_user", "false"}}},
         {features::internal::kTabOrganizationSettingsVisibility,
          {{"allow_unsigned_user", "true"}}}},
        {});
    EXPECT_THAT(GetAllowedFeaturesForUnsignedUser(),
                UnorderedElementsAre(UserVisibleFeatureKey::kTabOrganization));
  }
}

TEST(ModelExecutionFeature, GetOptimizationTargetForModelAdaptation) {
  // This tests the generic logic that we expect for all features going forward.
  EXPECT_THAT(features::internal::GetOptimizationTargetForModelAdaptation(
                  ModelBasedCapabilityKey::kHistorySearch),
              proto::OptimizationTarget::
                  OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_HISTORY_SEARCH);
  EXPECT_THAT(features::internal::GetOptimizationTargetForModelAdaptation(
                  ModelBasedCapabilityKey::kPromptApi),
              proto::OptimizationTarget::
                  OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROMPT_API);
  EXPECT_THAT(features::internal::GetOptimizationTargetForModelAdaptation(
                  ModelBasedCapabilityKey::kSummarize),
              proto::OptimizationTarget::
                  OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SUMMARIZE);

  // Special cases go here.
  EXPECT_THAT(features::internal::GetOptimizationTargetForModelAdaptation(
                  ModelBasedCapabilityKey::kTest),
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION);
  EXPECT_THAT(features::internal::GetOptimizationTargetForModelAdaptation(
                  ModelBasedCapabilityKey::kCompose),
              proto::OptimizationTarget::OPTIMIZATION_TARGET_COMPOSE);
}

}  // namespace
}  // namespace optimization_guide
