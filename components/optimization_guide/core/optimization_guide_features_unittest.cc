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

TEST(OptimizationGuideFeaturesTest, GetPageContentModelsToExecute) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"models_to_execute_v2",
        "OPTIMIZATION_TARGET_PAGE_TOPICS,OPTIMIZATION_TARGET_PAGE_ENTITIES"}});

  auto models = features::GetPageContentModelsToExecute("en-US");
  ASSERT_EQ(2U, models.size());
  ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_TOPICS, models[0]);
  ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, models[1]);
}

TEST(OptimizationGuideFeaturesTest,
     GetPageContentModelsToExecuteOldParameterName) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"models_to_execute",
        "OPTIMIZATION_TARGET_PAGE_TOPICS,OPTIMIZATION_TARGET_PAGE_ENTITIES"}});

  auto models = features::GetPageContentModelsToExecute("en-US");
  ASSERT_EQ(2U, models.size());
  ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_TOPICS, models[0]);
  ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, models[1]);
}

TEST(OptimizationGuideFeaturesTest, GetPageContentModelsToExecuteLocales) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{
          "models_to_execute_v2",
          // This string is meant to test language filtering, locale filtering,
          // and tolerance of whitespaces, as well as extra delimiters.
          "OPTIMIZATION_TARGET_PAGE_TOPICS:en:es-ES , OPTIMIZATION_TARGET_PAGE_"
          "ENTITIES,,OPTIMIZATION_TARGET_PAGE_VISIBILITY:zh-TW:",
      }});

  {
    auto models = features::GetPageContentModelsToExecute("en-US");
    ASSERT_EQ(2U, models.size());
    ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_TOPICS, models[0]);
    ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, models[1]);
  }

  {
    auto models = features::GetPageContentModelsToExecute("");
    ASSERT_EQ(1U, models.size());
    ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, models[0]);
  }

  {
    auto models = features::GetPageContentModelsToExecute("zh-CN");
    ASSERT_EQ(1U, models.size());
    ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, models[0]);
  }

  {
    auto models = features::GetPageContentModelsToExecute("zh-TW");
    ASSERT_EQ(2U, models.size());
    ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, models[0]);
    ASSERT_EQ(proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, models[1]);
  }
}

}  // namespace

}  // namespace optimization_guide
