// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_education/common/feature_promo_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
BASE_FEATURE(kTestIPHFeature,
             "TestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestIPHFeature2,
             "TestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr char kAppName1[] = "App1";
constexpr char kAppName2[] = "App2";
constexpr base::Time kSessionTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1000));
constexpr base::Time kLastActiveTime = kSessionTime + base::Hours(2);
constexpr base::Time kNewSessionTime = kSessionTime + base::Days(3);
constexpr base::Time kNewActiveTime = kNewSessionTime + base::Minutes(17);
}  // namespace

// Repeats some of the tests in FeaturePromoStorageServiceTest except that a
// live test profile is used to back the service instead of a dummy data map.
class BrowserFeaturePromoStorageServiceTest : public testing::Test {
 public:
  BrowserFeaturePromoStorageServiceTest()
      : task_environment_{base::test::SingleThreadTaskEnvironment::TimeSource::
                              MOCK_TIME},
        service_{&profile_} {
    service_.Reset(kTestIPHFeature);
    service_.Reset(kTestIPHFeature2);
  }

  static user_education::FeaturePromoData CreateTestData() {
    user_education::FeaturePromoData data;
    data.is_dismissed = true;
    data.last_dismissed_by = user_education::FeaturePromoClosedReason::kSnooze;
    data.first_show_time = base::Time::FromMillisecondsSinceUnixEpoch(1);
    data.last_show_time = base::Time::FromMillisecondsSinceUnixEpoch(100);
    data.last_snooze_time = base::Time::FromMillisecondsSinceUnixEpoch(200);
    data.snooze_count = 3;
    data.show_count = 4;
    data.shown_for_apps.insert(kAppName1);
    data.shown_for_apps.insert(kAppName2);
    return data;
  }

  void ResetData(const base::Feature& to_reset_data_for) {
    service_.Reset(to_reset_data_for);
  }

  void CompareData(const user_education::FeaturePromoData& expected,
                   const base::Feature& to_read_data_for) {
    const auto actual = service_.ReadPromoData(to_read_data_for);
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(expected.is_dismissed, actual->is_dismissed);
    EXPECT_EQ(expected.first_show_time, actual->first_show_time);
    EXPECT_EQ(expected.last_show_time, actual->last_show_time);
    EXPECT_EQ(expected.last_snooze_time, actual->last_snooze_time);
    EXPECT_EQ(expected.snooze_count, actual->snooze_count);
    EXPECT_EQ(expected.show_count, actual->show_count);
    EXPECT_THAT(actual->shown_for_apps,
                testing::ContainerEq(expected.shown_for_apps));
  }

  void SaveData(const base::Feature& to_save_data_for,
                const user_education::FeaturePromoData& data) {
    service_.SavePromoData(to_save_data_for, data);
  }

  auto ReadData(const base::Feature& to_read_data_for) {
    return service_.ReadPromoData(to_read_data_for);
  }

  void ResetSessionData() { service_.ResetSession(); }

  user_education::FeaturePromoSessionData ReadSessionData() {
    return service_.ReadSessionData();
  }

  void SaveSessionData(
      const user_education::FeaturePromoSessionData& session_data) {
    service_.SaveSessionData(session_data);
  }

  void CompareSessionData(
      const user_education::FeaturePromoSessionData& expected) {
    const auto actual = ReadSessionData();
    EXPECT_EQ(expected.start_time, actual.start_time);
    EXPECT_EQ(expected.most_recent_active_time, actual.most_recent_active_time);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  BrowserFeaturePromoStorageService service_;
};

TEST_F(BrowserFeaturePromoStorageServiceTest, NoDataByDefault) {
  EXPECT_FALSE(ReadData(kTestIPHFeature).has_value());
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndReadsData) {
  const auto data = CreateTestData();
  SaveData(kTestIPHFeature, data);
  CompareData(data, kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SaveAgain) {
  auto data = CreateTestData();
  SaveData(kTestIPHFeature, data);
  data.shown_for_apps.clear();
  data.is_dismissed = false;
  data.show_count++;
  SaveData(kTestIPHFeature, data);
  CompareData(data, kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, ResetClearsData) {
  const auto data = CreateTestData();
  SaveData(kTestIPHFeature, data);
  ResetData(kTestIPHFeature);
  ASSERT_FALSE(ReadData(kTestIPHFeature).has_value());
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndReadsMultipleFeatures) {
  const auto data = CreateTestData();
  SaveData(kTestIPHFeature, data);
  EXPECT_FALSE(ReadData(kTestIPHFeature2).has_value());
  auto data2 = CreateTestData();
  data2.is_dismissed = false;
  data2.last_dismissed_by = user_education::FeaturePromoClosedReason::kCancel;
  data2.shown_for_apps.clear();
  data2.show_count = 6;
  SaveData(kTestIPHFeature2, data2);
  CompareData(data, kTestIPHFeature);
  CompareData(data2, kTestIPHFeature2);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, NoSessionDataByDefault) {
  CompareSessionData(user_education::FeaturePromoSessionData());
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndReadsSessionData) {
  user_education::FeaturePromoSessionData data;
  data.start_time = kSessionTime;
  data.most_recent_active_time = kLastActiveTime;
  SaveSessionData(data);
  CompareSessionData(data);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SaveSessionAgain) {
  user_education::FeaturePromoSessionData data;
  data.start_time = kSessionTime;
  data.most_recent_active_time = kLastActiveTime;
  SaveSessionData(data);
  data.start_time = kNewSessionTime;
  data.most_recent_active_time = kNewActiveTime;
  SaveSessionData(data);
  CompareSessionData(data);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, ResetSessionClearsData) {
  user_education::FeaturePromoSessionData data;
  data.start_time = kSessionTime;
  data.most_recent_active_time = kLastActiveTime;
  SaveSessionData(data);
  ResetSessionData();
  CompareSessionData(user_education::FeaturePromoSessionData());
}
