// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_configuration_provider.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kExtendedParamName[] = "x_param";
constexpr char kExtendedParamValue[] = "x_param_value";
}  // namespace

class UserEducationConfigurationProviderBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  UserEducationConfigurationProviderBrowsertest() = default;
  ~UserEducationConfigurationProviderBrowsertest() override = default;

  void SetUp() override {
    if (UseV2()) {
      feature_list_.InitAndEnableFeaturesWithParameters(
          // Enable features:
          {base::test::FeatureRefAndParams(
               feature_engagement::kIPHWebUiHelpBubbleTestFeature,
               {{kExtendedParamName, kExtendedParamValue}}),
           base::test::FeatureRefAndParams(
               user_education::features::kUserEducationExperienceVersion2,
               {})});
    } else {
      feature_list_.InitAndEnableFeaturesWithParameters(
          // Enable features:
          {base::test::FeatureRefAndParams(
              feature_engagement::kIPHWebUiHelpBubbleTestFeature,
              {{kExtendedParamName, kExtendedParamValue}})},
          // Disable features:
          {user_education::features::kUserEducationExperienceVersion2});
    }
    InProcessBrowserTest::SetUp();
  }

 protected:
  bool UseV2() const { return GetParam(); }

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         UserEducationConfigurationProviderBrowsertest,
                         testing::Bool());

// Ensure that a feature which has field trial params that are not part of the
// FE config still gets proper automatic configuration.
IN_PROC_BROWSER_TEST_P(UserEducationConfigurationProviderBrowsertest,
                       AutoConfigureWithExtendedParam) {
  auto* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser()->profile());
  auto* const config = tracker->GetConfigurationForTesting();
  const feature_engagement::FeatureConfig& feature_config =
      config->GetFeatureConfig(
          feature_engagement::kIPHWebUiHelpBubbleTestFeature);
  ASSERT_TRUE(feature_config.valid);
  EXPECT_FALSE(feature_config.trigger.name.empty());
  if (UseV2()) {
    EXPECT_EQ(feature_engagement::ANY, feature_config.trigger.comparator.type);
    EXPECT_EQ(0U, feature_config.trigger.comparator.value);
  } else {
    EXPECT_EQ(feature_engagement::LESS_THAN,
              feature_config.trigger.comparator.type);
    EXPECT_GT(feature_config.trigger.comparator.value, 0U);
  }
  EXPECT_FALSE(feature_config.used.name.empty());
  EXPECT_EQ(feature_engagement::EQUAL, feature_config.used.comparator.type);
  EXPECT_EQ(0U, feature_config.used.comparator.value);
  EXPECT_TRUE(feature_config.event_configs.empty());
  EXPECT_EQ(kExtendedParamValue,
            base::GetFieldTrialParamValueByFeature(
                feature_engagement::kIPHWebUiHelpBubbleTestFeature,
                kExtendedParamName));
}
