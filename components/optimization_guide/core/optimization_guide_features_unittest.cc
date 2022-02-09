// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(OptimizationGuideFeaturesTest,
     TestGetOptimizationGuideServiceGetHintsURLHTTPSOnly) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteOptimizationGuideFetching,
      {{"optimization_guide_service_url", "http://NotAnHTTPSServer.com"}});

  EXPECT_EQ(features::GetOptimizationGuideServiceGetHintsURL().spec(),
            kOptimizationGuideServiceGetHintsDefaultURL);
  EXPECT_TRUE(features::GetOptimizationGuideServiceGetHintsURL().SchemeIs(
      url::kHttpsScheme));
}

TEST(OptimizationGuideFeaturesTest,
     TestGetOptimizationGuideServiceGetHintsURLViaFinch) {
  base::test::ScopedFeatureList scoped_feature_list;

  std::string optimization_guide_service_url = "https://finchserver.com/";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteOptimizationGuideFetching,
      {{"optimization_guide_service_url", optimization_guide_service_url}});

  EXPECT_EQ(features::GetOptimizationGuideServiceGetHintsURL().spec(),
            optimization_guide_service_url);
}

TEST(OptimizationGuideFeaturesTest, InvalidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "-1"},
       {"noise_prob_for_rappor_metrics", "-.5"}});
  EXPECT_EQ(1, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(0.0, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(OptimizationGuideFeaturesTest, ValidPageContentRAPPORMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "2"},
       {"noise_prob_for_rappor_metrics", ".2"}});
  EXPECT_EQ(2, features::NumBitsForRAPPORMetrics());
  EXPECT_EQ(.2, features::NoiseProbabilityForRAPPORMetrics());
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageEntitiesModelOnPageContentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndDisableFeature(
      features::kPageEntitiesPageContentAnnotations);

  EXPECT_FALSE(features::ShouldExecutePageEntitiesModelOnPageContent("en-US"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageEntitiesModelOnPageContentEmptyAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(
      features::kPageEntitiesPageContentAnnotations);

  EXPECT_TRUE(features::ShouldExecutePageEntitiesModelOnPageContent("en-US"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageEntitiesModelOnPageContentWithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageEntitiesPageContentAnnotations,
      {{"supported_locales", "en,zh-TW"}});

  EXPECT_TRUE(features::ShouldExecutePageEntitiesModelOnPageContent("en-US"));
  EXPECT_FALSE(features::ShouldExecutePageEntitiesModelOnPageContent(""));
  EXPECT_FALSE(features::ShouldExecutePageEntitiesModelOnPageContent("zh-CN"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndDisableFeature(
      features::kPageVisibilityPageContentAnnotations);

  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentEmptyAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(
      features::kPageVisibilityPageContentAnnotations);

  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
}

TEST(OptimizationGuideFeaturesTest,
     ShouldExecutePageVisibilityModelOnPageContentWithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageVisibilityPageContentAnnotations,
      {{"supported_locales", "en,zh-TW"}});

  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent(""));
  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("zh-CN"));
}

}  // namespace

}  // namespace optimization_guide
