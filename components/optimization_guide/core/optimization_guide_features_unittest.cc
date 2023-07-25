// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include <limits>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  // These are the default enabled values.
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-AU"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-CA"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-GB"));
  EXPECT_TRUE(features::ShouldExecutePageVisibilityModelOnPageContent("en-US"));

  EXPECT_FALSE(
      features::ShouldExecutePageVisibilityModelOnPageContent("zh-CN"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent("fr"));
  EXPECT_FALSE(features::ShouldExecutePageVisibilityModelOnPageContent(""));
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

TEST(OptimizationGuideFeaturesTest, OptimizationGuidePersonalizedFetching) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuidePersonalizedFetching,
      {
          {"allowed_contexts", "CONTEXT_PAGE_NAVIGATION,CONTEXT_BOOKMARKS"},
          {"oauth_scopes", "scope,scope2"},
      });

  // Check scopes.
  EXPECT_THAT(features::OAuthScopesForPersonalizedMetadata(),
              ::testing::UnorderedElementsAre("scope", "scope2"));

  // Check contexts.
  EXPECT_FALSE(features::EnabledPersonalizedMetadata(
      optimization_guide::proto::CONTEXT_UNSPECIFIED));
  EXPECT_TRUE(features::EnabledPersonalizedMetadata(
      optimization_guide::proto::CONTEXT_PAGE_NAVIGATION));
  EXPECT_TRUE(features::EnabledPersonalizedMetadata(
      optimization_guide::proto::CONTEXT_BOOKMARKS));
}

TEST(OptimizationGuideFeaturesTest, TestOverrideNumThreadsForOptTarget) {
  struct TestCase {
    std::string label;
    bool enabled;
    std::map<std::string, std::string> params;
    std::vector<std::pair<proto::OptimizationTarget, absl::optional<int>>> want;
  };

  struct TestCase tests[] = {
      {
          .label = "feature disabled",
          .enabled = false,
          .params = {},
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, absl::nullopt},
              },
      },
      {
          .label = "feature enabled, but no params",
          .enabled = true,
          .params = {},
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, absl::nullopt},
              },
      },
      {
          .label = "one target overriden",
          .enabled = true,
          .params =
              {
                  {"OPTIMIZATION_TARGET_PAGE_VISIBILITY", "1"},
              },
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, 1},
              },
      },
      {
          .label = "zero is nullopt",
          .enabled = true,
          .params =
              {
                  {"OPTIMIZATION_TARGET_PAGE_VISIBILITY", "0"},
              },
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, absl::nullopt},
              },
      },
      {
          .label = "less than -1",
          .enabled = true,
          .params =
              {
                  {"OPTIMIZATION_TARGET_PAGE_VISIBILITY", "-2"},
              },
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, absl::nullopt},
              },
      },
      {
          .label = "-1 is valid",
          .enabled = true,
          .params =
              {
                  {"OPTIMIZATION_TARGET_PAGE_VISIBILITY", "-1"},
              },
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, -1},
              },
      },
      {
          .label = "two targets overriden",
          .enabled = true,
          .params =
              {
                  {"OPTIMIZATION_TARGET_PAGE_VISIBILITY", "1"},
                  {"OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD", "-1"},
              },
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, 1},
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, -1},
              },
      },
      {
          .label = "capped at num cpu",
          .enabled = true,
          .params =
              {
                  {"OPTIMIZATION_TARGET_PAGE_VISIBILITY",
                   base::NumberToString(std::numeric_limits<int>::max())},
              },
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
                   base::SysInfo::NumberOfProcessors()},
              },
      },
  };

  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.label);

    base::test::ScopedFeatureList scoped_feature_list;
    if (test.enabled) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kOverrideNumThreadsForModelExecution, test.params);
    } else {
      scoped_feature_list.InitAndDisableFeature(
          features::kOverrideNumThreadsForModelExecution);
    }

    for (const auto& expectation : test.want) {
      proto::OptimizationTarget opt_target = expectation.first;
      absl::optional<int> num_threads = expectation.second;

      EXPECT_EQ(num_threads,
                features::OverrideNumThreadsForOptTarget(opt_target))
          << GetStringNameForOptimizationTarget(opt_target);
    }
  }
}

}  // namespace

}  // namespace optimization_guide
