// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_config.h"

#include "base/test/scoped_feature_list.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/tile_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {

namespace {

void TestImagePrefetchMode(std::map<std::string, std::string> params,
                           ImagePrefetchMode expected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kQueryTiles,
                                                  params);
  EXPECT_EQ(TileConfig::GetImagePrefetchMode(), expected);
}

}  // namespace

// Test to verify Finch parameters for enabled experiment group is read
// correctly.
TEST(TileConfigTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {kExperimentTagKey, "1234"},     {kBaseURLKey, "https://test.com"},
      {kExpireDurationKey, "100"},     {kIsUnmeteredNetworkRequiredKey, "true"},
      {kScheduleIntervalKey, "123"},   {kMaxRandomWindowKey, "234"},
      {kOneoffTaskWindowKey, "345"},   {kBackoffInitDelayInMsKey, "456"},
      {kBackoffMaxDelayInMsKey, "567"}};
  feature_list.InitAndEnableFeatureWithParameters(features::kQueryTiles,
                                                  params);
  EXPECT_EQ(TileConfig::GetQueryTilesServerUrl("", false),
            GURL("https://test.com/v1/querytiles"));
  EXPECT_EQ(TileConfig::GetQueryTilesServerUrl("https://xyz.com", false),
            GURL("https://test.com/v1/querytiles"));
  EXPECT_TRUE(TileConfig::GetIsUnMeteredNetworkRequired());
  EXPECT_EQ(TileConfig::GetExperimentTag("us"), "1234");
  EXPECT_EQ(TileConfig::GetExpireDuration(), base::Seconds(100));
  EXPECT_EQ(TileConfig::GetScheduleIntervalInMs(), 123);
  EXPECT_EQ(TileConfig::GetMaxRandomWindowInMs(), 234);
  EXPECT_EQ(TileConfig::GetOneoffTaskWindowInMs(), 345);
  EXPECT_EQ(TileConfig::GetBackoffPolicyArgsInitDelayInMs(), 456);
  EXPECT_EQ(TileConfig::GetBackoffPolicyArgsMaxDelayInMs(), 567);
}

// Test to verify that if not configured, default parameter values are used.
TEST(TileConfigTest, FinchConfigDefaultParameter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kQueryTiles);
  EXPECT_EQ(TileConfig::GetQueryTilesServerUrl("", true), GURL());
  EXPECT_EQ(TileConfig::GetQueryTilesServerUrl("https://xyz.com", true),
            GURL("https://xyz.com/v1/querytiles"));
  EXPECT_FALSE(TileConfig::GetIsUnMeteredNetworkRequired());
  EXPECT_TRUE(TileConfig::GetExperimentTag("us").empty());
  EXPECT_EQ(TileConfig::GetExpireDuration(), base::Days(2));
  EXPECT_EQ(TileConfig::GetScheduleIntervalInMs(),
            base::Hours(12).InMilliseconds());
  EXPECT_EQ(TileConfig::GetMaxRandomWindowInMs(),
            base::Hours(4).InMilliseconds());
  EXPECT_EQ(TileConfig::GetOneoffTaskWindowInMs(),
            base::Hours(2).InMilliseconds());
  EXPECT_EQ(TileConfig::GetBackoffPolicyArgsInitDelayInMs(),
            base::Seconds(30).InMilliseconds());
  EXPECT_EQ(TileConfig::GetBackoffPolicyArgsMaxDelayInMs(),
            base::Days(1).InMilliseconds());
}

// Test to verify ImagePrefetchMode can be parsed correctly from Finch
// parameters.
TEST(TileConfigTest, GetImagePrefetchMode) {
  TestImagePrefetchMode({{kImagePrefetchModeKey, "none"}},
                        ImagePrefetchMode::kNone);
  TestImagePrefetchMode(std::map<std::string, std::string>(),
                        ImagePrefetchMode::kTopLevel);
  TestImagePrefetchMode({{kImagePrefetchModeKey, ""}},
                        ImagePrefetchMode::kTopLevel);
  TestImagePrefetchMode({{kImagePrefetchModeKey, "top"}},
                        ImagePrefetchMode::kTopLevel);
  TestImagePrefetchMode({{kImagePrefetchModeKey, "all"}},
                        ImagePrefetchMode::kAll);
}

// Test to verify the default params for enabled countries.
TEST(TileConfigTest, ExperimentTagForEnabledCountries) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_EQ(TileConfig::GetExperimentTag("in"),
            "{maxLevels : 1, enableTrending : true, maxTrendingQueries : 8}");
  EXPECT_EQ(TileConfig::GetExperimentTag("in"),
            TileConfig::GetExperimentTag("ng"));
  EXPECT_EQ(TileConfig::GetExperimentTag("jp"),
            "{maxLevels : 1, rankTiles : true, enableTrending : true, "
            "maxTrendingQueries : 8, disableEntityTranslation: true}");

  // Finch params should override default values.
  std::map<std::string, std::string> params = {
      {kExperimentTagKey,
       "{maxLevels : 1, enableTrending : true, rankTiles : true}"}};
  feature_list.InitAndEnableFeatureWithParameters(features::kQueryTiles,
                                                  params);
  EXPECT_EQ(TileConfig::GetExperimentTag("in"),
            "{maxLevels : 1, enableTrending : true, rankTiles : true}");
  EXPECT_EQ(TileConfig::GetExperimentTag("in"),
            TileConfig::GetExperimentTag("ng"));
  EXPECT_EQ(TileConfig::GetExperimentTag("in"),
            TileConfig::GetExperimentTag("JP"));
}

}  // namespace query_tiles
