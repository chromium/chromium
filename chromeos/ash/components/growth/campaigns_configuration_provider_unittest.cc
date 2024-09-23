// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_configuration_provider.h"

#include "base/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace growth {

namespace {

using feature_engagement::ANY;
using feature_engagement::Comparator;
using feature_engagement::EQUAL;
using feature_engagement::EventConfig;
using feature_engagement::FeatureConfig;
using feature_engagement::LESS_THAN;

BASE_FEATURE(kTestFeatureValid,
             "IPH_GrowthFramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeatureInvalid,
             "GrowthGramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kEventUsedKey[] = "event_used";
constexpr char kEventTriggerKey[] = "event_trigger";
constexpr char kEvent1Key[] = "event_1";
constexpr char kEventImpressionKey[] = "event_impression";
constexpr char kEventDismissalKey[] = "event_dismissal";

constexpr char kEventUsedParam[] =
    "name:ChromeOSAshGrowthCampaigns_EventUsed;comparator:any;window:1;storage:"
    "1";

constexpr char kEventTriggerParam[] =
    "name:ChromeOSAshGrowthCampaigns_EventTrigger;comparator:any;window:1;"
    "storage:1";

constexpr char kEventImpressionParam[] =
    "name:ChromeOSAshGrowthCampaigns_Impression_CampaignId_100;comparator:<3;"
    "window:365;storage:365";

constexpr char kEventDismissalParam[] =
    "name:ChromeOSAshGrowthCampaigns_Dismissed_CampaignId_100;comparator:<1;"
    "window:365;storage:365";

constexpr char kEvent1Param[] =
    "name:ChromeOSAshGrowthCampaigns_Impression_CampaignId_100;comparator:==0;"
    "window:365;storage:365";

}  // namespace

class CampaignsConfigurationProviderTest : public testing::Test {
 protected:
  CampaignsConfigurationProvider provider_;
};

TEST_F(CampaignsConfigurationProviderTest, HasPrefixValidFeature) {
  const auto prefixes =
      provider_.MaybeProvideAllowedEventPrefixes(kTestFeatureValid);

  EXPECT_EQ(1u, prefixes.size());
  EXPECT_TRUE(prefixes.contains("ChromeOSAshGrowthCampaigns_"));
}

TEST_F(CampaignsConfigurationProviderTest, NoPrefixInvalidFeature) {
  const auto prefixes =
      provider_.MaybeProvideAllowedEventPrefixes(kTestFeatureInvalid);

  EXPECT_EQ(0u, prefixes.size());
}

TEST_F(CampaignsConfigurationProviderTest, SetConfigWithInvalidFeature) {
  std::map<std::string, std::string> params;

  FeatureConfig config;
  provider_.SetConfig(params);
  provider_.MaybeProvideFeatureConfiguration(
      kTestFeatureInvalid, config, /*known_features=*/{}, /*known_groups=*/{});
  EXPECT_FALSE(config.valid);
}

TEST_F(CampaignsConfigurationProviderTest, SetConfigWithValidFeature) {
  std::map<std::string, std::string> params;
  params[kEventUsedKey] = kEventUsedParam;
  params[kEventTriggerKey] = kEventTriggerParam;
  params[kEventImpressionKey] = kEventImpressionParam;
  params[kEventDismissalKey] = kEventDismissalParam;
  params[kEvent1Key] = kEvent1Param;

  FeatureConfig expected;
  expected.valid = true;
  expected.used = EventConfig("ChromeOSAshGrowthCampaigns_EventUsed",
                              Comparator(ANY, 0), 1, 1);
  expected.trigger = EventConfig("ChromeOSAshGrowthCampaigns_EventTrigger",
                                 Comparator(ANY, 0), 1, 1);
  expected.event_configs.insert(
      EventConfig("ChromeOSAshGrowthCampaigns_Impression_CampaignId_100",
                  Comparator(LESS_THAN, 3), 365, 365));
  expected.event_configs.insert(
      EventConfig("ChromeOSAshGrowthCampaigns_Dismissed_CampaignId_100",
                  Comparator(LESS_THAN, 1), 365, 365));
  expected.event_configs.insert(
      EventConfig("ChromeOSAshGrowthCampaigns_Impression_CampaignId_100",
                  Comparator(EQUAL, 0), 365, 365));

  FeatureConfig config;
  provider_.SetConfig(params);
  provider_.MaybeProvideFeatureConfiguration(
      kTestFeatureValid, config, /*known_features=*/{}, /*known_groups=*/{});

  EXPECT_EQ(expected, config);
}

}  // namespace growth
