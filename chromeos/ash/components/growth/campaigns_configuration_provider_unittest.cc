// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_configuration_provider.h"

#include "base/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace growth {

namespace {

BASE_FEATURE(kTestFeatureValid,
             "IPH_GrowthFramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeatureInvalid,
             "GrowthGramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeaturePlaceholder,
             "IPH_ScalableIphHelpAppBasedNudge",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class CampaignsConfigurationProviderTest : public testing::Test {
 protected:
  CampaignsConfigurationProvider provider_;
};

TEST_F(CampaignsConfigurationProviderTest, HasPrefixPlaceholderFeature) {
  const auto prefixes =
      provider_.MaybeProvideAllowedEventPrefixes(kTestFeaturePlaceholder);

  EXPECT_EQ(1u, prefixes.size());
  EXPECT_TRUE(prefixes.contains("ChromeOSAshGrowthCampaigns"));
}

TEST_F(CampaignsConfigurationProviderTest, NoPrefixInvalidFeature) {
  const auto prefixes =
      provider_.MaybeProvideAllowedEventPrefixes(kTestFeatureInvalid);

  EXPECT_EQ(0u, prefixes.size());
}

TEST_F(CampaignsConfigurationProviderTest, SetConfigWithInvalidFeature) {
  std::map<std::string, std::string> params;

  feature_engagement::FeatureConfig config;
  provider_.SetConfig(params);
  provider_.MaybeProvideFeatureConfiguration(
      kTestFeatureInvalid, config, /*known_features=*/{}, /*known_groups=*/{});
  EXPECT_FALSE(config.valid);
}

TEST_F(CampaignsConfigurationProviderTest, SetConfigWithValidFeature) {
  std::map<std::string, std::string> params;

  feature_engagement::FeatureConfig config;
  provider_.SetConfig(params);
  provider_.MaybeProvideFeatureConfiguration(
      kTestFeatureValid, config, /*known_features=*/{}, /*known_groups=*/{});

  EXPECT_TRUE(config.valid);
}

}  // namespace growth
