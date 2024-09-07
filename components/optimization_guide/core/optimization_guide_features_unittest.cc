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
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class OptimizationGuideFeaturesTest : public testing::Test {
 public:
  void SetUp() override {
    model_execution::prefs::RegisterProfilePrefs(prefs_.registry());
  }

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(OptimizationGuideFeaturesTest,
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

TEST_F(OptimizationGuideFeaturesTest,
       TestGetOptimizationGuideServiceGetHintsURLViaFinch) {
  base::test::ScopedFeatureList scoped_feature_list;

  std::string optimization_guide_service_url = "https://finchserver.com/";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteOptimizationGuideFetching,
      {{"optimization_guide_service_url", optimization_guide_service_url}});

  EXPECT_EQ(features::GetOptimizationGuideServiceGetHintsURL().spec(),
            optimization_guide_service_url);
}

TEST_F(OptimizationGuideFeaturesTest, ModelQualityLoggingDefault) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(features::kModelQualityLogging);

  EXPECT_TRUE(features::IsModelQualityLoggingEnabled());

  // Compose, wallpaper search and tab organization should be enabled by
  // default whereas product specifications should be disabled by default.
  MqlsFeatureRegistry& registry = MqlsFeatureRegistry::GetInstance();
  EXPECT_TRUE(features::IsModelQualityLoggingEnabledForFeature(
      registry.GetFeature(proto::LogAiDataRequest::FeatureCase::kCompose)));
  EXPECT_TRUE(
      features::IsModelQualityLoggingEnabledForFeature(registry.GetFeature(
          proto::LogAiDataRequest::FeatureCase::kTabOrganization)));
  EXPECT_TRUE(
      features::IsModelQualityLoggingEnabledForFeature(registry.GetFeature(
          proto::LogAiDataRequest::FeatureCase::kWallpaperSearch)));
  EXPECT_FALSE(
      features::IsModelQualityLoggingEnabledForFeature(registry.GetFeature(
          proto::LogAiDataRequest::FeatureCase::kProductSpecifications)));
}

TEST_F(OptimizationGuideFeaturesTest,
       ModelQualityLoggingAlwaysDisabledForTestAndUnspecifiedFeatures) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kModelQualityLogging,
      {{"model_execution_feature_test", "true"},
       {"model_execution_feature_unspecified", "true"}});

  EXPECT_TRUE(features::IsModelQualityLoggingEnabled());
}

TEST_F(OptimizationGuideFeaturesTest, ComposeModelQualityLoggingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  MqlsFeatureRegistry& registry = MqlsFeatureRegistry::GetInstance();
  const MqlsFeatureMetadata* metadata =
      registry.GetFeature(proto::LogAiDataRequest::FeatureCase::kCompose);

  scoped_feature_list.InitAndDisableFeature(*metadata->field_trial_feature());

  EXPECT_TRUE(features::IsModelQualityLoggingEnabled());
  EXPECT_FALSE(features::IsModelQualityLoggingEnabledForFeature(metadata));
  // TabOrganization should still be enabled.
  EXPECT_TRUE(
      features::IsModelQualityLoggingEnabledForFeature(registry.GetFeature(
          proto::LogAiDataRequest::FeatureCase::kTabOrganization)));
}

TEST_F(OptimizationGuideFeaturesTest, ModelQualityLoggingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndDisableFeature(features::kModelQualityLogging);

  // All features logging should be disabled if ModelQualityLogging is disabled.
  EXPECT_FALSE(features::IsModelQualityLoggingEnabled());
  MqlsFeatureRegistry& registry = MqlsFeatureRegistry::GetInstance();
  EXPECT_FALSE(features::IsModelQualityLoggingEnabledForFeature(
      registry.GetFeature(proto::LogAiDataRequest::FeatureCase::kCompose)));
  EXPECT_FALSE(
      features::IsModelQualityLoggingEnabledForFeature(registry.GetFeature(
          proto::LogAiDataRequest::FeatureCase::kTabOrganization)));
  EXPECT_FALSE(
      features::IsModelQualityLoggingEnabledForFeature(registry.GetFeature(
          proto::LogAiDataRequest::FeatureCase::kWallpaperSearch)));
}

TEST_F(OptimizationGuideFeaturesTest,
       OptimizationGuidePersonalizedFetchingDefaultBehaviour) {
  features::RequestContextSet allowedContexts =
      features::GetAllowedContextsForPersonalizedMetadata();

  // Check contexts.
  EXPECT_FALSE(
      allowedContexts.Has(optimization_guide::proto::CONTEXT_UNSPECIFIED));
  EXPECT_TRUE(allowedContexts.Has(
      optimization_guide::proto::CONTEXT_PAGE_INSIGHTS_HUB));
}

TEST_F(OptimizationGuideFeaturesTest,
       OptimizationGuidePersonalizedFetchingPopulatedParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuidePersonalizedFetching,
      {
          {"allowed_contexts", "CONTEXT_PAGE_NAVIGATION,CONTEXT_BOOKMARKS"},
      });

  features::RequestContextSet allowedContexts =
      features::GetAllowedContextsForPersonalizedMetadata();

  // Check contexts.
  EXPECT_FALSE(
      allowedContexts.Has(optimization_guide::proto::CONTEXT_UNSPECIFIED));
  EXPECT_FALSE(allowedContexts.Has(
      optimization_guide::proto::CONTEXT_PAGE_INSIGHTS_HUB));
  EXPECT_TRUE(
      allowedContexts.Has(optimization_guide::proto::CONTEXT_PAGE_NAVIGATION));
  EXPECT_TRUE(
      allowedContexts.Has(optimization_guide::proto::CONTEXT_BOOKMARKS));
}

TEST_F(OptimizationGuideFeaturesTest,
       OptimizationGuidePersonalizedFetchingEmptyParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuidePersonalizedFetching,
      {
          {"allowed_contexts", ""},
      });

  features::RequestContextSet allowedContexts =
      features::GetAllowedContextsForPersonalizedMetadata();

  // Check contexts.
  EXPECT_FALSE(
      allowedContexts.Has(optimization_guide::proto::CONTEXT_UNSPECIFIED));
  EXPECT_FALSE(
      allowedContexts.Has(optimization_guide::proto::CONTEXT_PAGE_NAVIGATION));
  EXPECT_FALSE(allowedContexts.Has(
      optimization_guide::proto::CONTEXT_PAGE_INSIGHTS_HUB));
}

TEST_F(OptimizationGuideFeaturesTest, TestOverrideNumThreadsForOptTarget) {
  struct TestCase {
    std::string label;
    bool enabled;
    std::map<std::string, std::string> params;
    std::vector<std::pair<proto::OptimizationTarget, std::optional<int>>> want;
  };

  struct TestCase tests[] = {
      {
          .label = "feature disabled",
          .enabled = false,
          .params = {},
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, std::nullopt},
              },
      },
      {
          .label = "feature enabled, but no params",
          .enabled = true,
          .params = {},
          .want =
              {
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, std::nullopt},
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
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt},
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
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, std::nullopt},
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
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt},
                  {proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, std::nullopt},
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
                  {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt},
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
      std::optional<int> num_threads = expectation.second;

      EXPECT_EQ(num_threads,
                features::OverrideNumThreadsForOptTarget(opt_target))
          << GetStringNameForOptimizationTarget(opt_target);
    }
  }
}

TEST_F(OptimizationGuideFeaturesTest, PredictionModelVersionInKillSwitch) {
  EXPECT_TRUE(features::GetPredictionModelVersionsInKillSwitch().empty());
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kOptimizationGuidePredictionModelKillswitch,
        {{"OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD", "1,3"},
         {"OPTIMIZATION_TARGET_MODEL_VALIDATION", "5"}});

    EXPECT_THAT(features::GetPredictionModelVersionsInKillSwitch(),
                testing::ElementsAre(
                    testing::Pair(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
                                  testing::ElementsAre(1, 3)),
                    testing::Pair(proto::OPTIMIZATION_TARGET_MODEL_VALIDATION,
                                  testing::ElementsAre(5))));
  }
}

TEST_F(OptimizationGuideFeaturesTest,
       IsPerformanceClassCompatibleWithOnDeviceModel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"compatible_on_device_performance_classes", "4,6"}});

  EXPECT_FALSE(features::IsPerformanceClassCompatibleWithOnDeviceModel(
      OnDeviceModelPerformanceClass::kError));
  EXPECT_TRUE(features::IsPerformanceClassCompatibleWithOnDeviceModel(
      OnDeviceModelPerformanceClass::kMedium));
  EXPECT_FALSE(features::IsPerformanceClassCompatibleWithOnDeviceModel(
      OnDeviceModelPerformanceClass::kHigh));
  EXPECT_TRUE(features::IsPerformanceClassCompatibleWithOnDeviceModel(
      OnDeviceModelPerformanceClass::kVeryHigh));
}

TEST_F(OptimizationGuideFeaturesTest, AllowedAdaptationRanks) {
  // Default value
  EXPECT_THAT(features::GetOnDeviceModelAllowedAdaptationRanks(),
              testing::ElementsAre(32));
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kOptimizationGuideOnDeviceModel,
        {{"allowed_adaptation_ranks", "16,32"}});
    EXPECT_THAT(features::GetOnDeviceModelAllowedAdaptationRanks(),
                testing::ElementsAre(16, 32));
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kOptimizationGuideOnDeviceModel,
        {{"allowed_adaptation_ranks", "16,invalid,64"}});
    EXPECT_THAT(features::GetOnDeviceModelAllowedAdaptationRanks(),
                testing::ElementsAre(16, 64));
  }
}

}  // namespace

}  // namespace optimization_guide
