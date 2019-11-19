// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ml_app_rank_provider.h"

#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.pb.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

const char kAppId[] = "app_id";

TEST(MlAppRankProviderTest, MlInferenceTest) {
  base::test::TaskEnvironment task_environment_;

  chromeos::machine_learning::FakeServiceConnectionImpl fake_service_connection;

  const double expected_value = 1.234;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});

  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(&fake_service_connection);

  MlAppRankProvider ml_app_rank_provider;

  base::flat_map<std::string, AppLaunchFeatures> app_features_map;
  AppLaunchFeatures features;
  features.set_app_id(kAppId);
  features.set_app_type(AppLaunchEvent_AppType_CHROME);
  features.set_click_rank(1);
  for (int hour = 0; hour < 24; ++hour) {
    features.add_clicks_each_hour(1);
  }

  app_features_map[kAppId] = features;
  ml_app_rank_provider.CreateRankings(app_features_map, 3, 1, 7);

  EXPECT_EQ(0UL, ml_app_rank_provider.RetrieveRankings().size());

  task_environment_.RunUntilIdle();

  const std::map<std::string, float> ranking_map =
      ml_app_rank_provider.RetrieveRankings();

  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, ranking_map.size());
  const auto it = ranking_map.find(kAppId);
  ASSERT_NE(ranking_map.end(), it);
  EXPECT_NEAR(expected_value, it->second, 0.001);
}

TEST(MlAppRankProviderTest, ExecutionAfterDestructorTest) {
  base::test::TaskEnvironment task_environment_;

  chromeos::machine_learning::FakeServiceConnectionImpl fake_service_connection;

  const double expected_value = 1.234;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});

  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(&fake_service_connection);

  {
    MlAppRankProvider ml_app_rank_provider;

    base::flat_map<std::string, AppLaunchFeatures> app_features_map;
    AppLaunchFeatures features;
    features.set_app_id(kAppId);
    features.set_app_type(AppLaunchEvent_AppType_CHROME);
    app_features_map[kAppId] = features;
    ml_app_rank_provider.CreateRankings(app_features_map, 3, 1, 7);
  }
  // Run the background tasks after ml_app_rank_provider has been destroyed.
  // If this does not crash it is a success.
  task_environment_.RunUntilIdle();
}

TEST(MlAppRankProviderTest, CreateRankerExampleTest) {
  base::test::TaskEnvironment task_environment_;

  MlAppRankProvider ml_app_rank_provider;

  base::flat_map<std::string, AppLaunchFeatures> app_features_map;
  AppLaunchFeatures features;
  features.set_app_id(kAppId);
  features.set_app_type(AppLaunchEvent_AppType_CHROME);
  features.set_click_rank(1);
  features.set_clicks_last_hour(3);
  features.set_clicks_last_24_hours(4);
  features.set_last_launched_from(AppLaunchEvent_LaunchedFrom_GRID);
  features.set_most_recently_used_index(2);
  features.set_total_clicks(100);
  for (int hour = 0; hour < 24; ++hour) {
    features.add_clicks_each_hour(hour + 10);
  }

  app_features_map[kAppId] = features;

  assist_ranker::RankerExample actual =
      CreateRankerExample(features, 120, 4, 3, 19, 7, 17);

  auto* actual_feature_map(actual.mutable_features());

  EXPECT_EQ(3, (*actual_feature_map)["DayOfWeek"].int32_value());
  EXPECT_EQ(19, (*actual_feature_map)["HourOfDay"].int32_value());
  EXPECT_EQ(7, (*actual_feature_map)["AllClicksLastHour"].int32_value());
  EXPECT_EQ(17, (*actual_feature_map)["AllClicksLast24Hours"].int32_value());
  EXPECT_EQ(1, (*actual_feature_map)["AppType"].int32_value());
  EXPECT_EQ(1, (*actual_feature_map)["ClickRank"].int32_value());
  EXPECT_EQ(3, (*actual_feature_map)["ClicksLastHour"].int32_value());
  EXPECT_EQ(4, (*actual_feature_map)["ClicksLast24Hours"].int32_value());
  EXPECT_EQ(1, (*actual_feature_map)["LastLaunchedFrom"].int32_value());
  EXPECT_EQ(true, (*actual_feature_map)["HasClick"].bool_value());
  EXPECT_EQ(2, (*actual_feature_map)["MostRecentlyUsedIndex"].int32_value());
  EXPECT_EQ(120, (*actual_feature_map)["TimeSinceLastClick"].int32_value());
  EXPECT_EQ(100, (*actual_feature_map)["TotalClicks"].int32_value());
  EXPECT_NEAR(20.0, (*actual_feature_map)["TotalClicksPerHour"].float_value(),
              0.1);
  EXPECT_EQ(4, (*actual_feature_map)["TotalHours"].int32_value());
  EXPECT_EQ(std::string("chrome-extension://") + kAppId,
            (*actual_feature_map)["URL"].string_value());

  EXPECT_EQ(10, (*actual_feature_map)["ClicksEachHour00"].int32_value());
  EXPECT_EQ(11, (*actual_feature_map)["ClicksEachHour01"].int32_value());
  EXPECT_EQ(19, (*actual_feature_map)["ClicksEachHour09"].int32_value());
  EXPECT_EQ(20, (*actual_feature_map)["ClicksEachHour10"].int32_value());
  // Bucketizing rounds 21-29 down to 20, 31-39 down to 30.
  EXPECT_EQ(20, (*actual_feature_map)["ClicksEachHour11"].int32_value());
  EXPECT_EQ(20, (*actual_feature_map)["ClicksEachHour19"].int32_value());
  EXPECT_EQ(30, (*actual_feature_map)["ClicksEachHour20"].int32_value());
  EXPECT_EQ(30, (*actual_feature_map)["ClicksEachHour23"].int32_value());

  EXPECT_NEAR(2.0, (*actual_feature_map)["ClicksPerHour00"].float_value(), 0.1);
  EXPECT_NEAR(2.2, (*actual_feature_map)["ClicksPerHour01"].float_value(), 0.1);
  EXPECT_NEAR(6.0, (*actual_feature_map)["ClicksPerHour23"].float_value(), 0.1);

  EXPECT_EQ(10 + 11 + 12 + 13,
            (*actual_feature_map)["FourHourClicks0"].int32_value());
  EXPECT_EQ(30 + 30 + 30 + 30,
            (*actual_feature_map)["FourHourClicks5"].int32_value());

  EXPECT_EQ(10 + 11 + 12 + 13 + 14 + 15,
            (*actual_feature_map)["SixHourClicks0"].int32_value());
  EXPECT_EQ(20 + 20 + 30 + 30 + 30 + 30,
            (*actual_feature_map)["SixHourClicks3"].int32_value());
}

}  // namespace app_list
