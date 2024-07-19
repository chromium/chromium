// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
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
constexpr base::Time kLastShownTime1 = kSessionTime + base::Minutes(45);
constexpr base::Time kLastShownTime2 = kSessionTime + base::Minutes(75);
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
    user_education::KeyedFeaturePromoData keyed_data1;
    keyed_data1.show_count = 1;
    keyed_data1.last_shown_time = kLastShownTime1;
    user_education::KeyedFeaturePromoData keyed_data2;
    keyed_data2.show_count = 2;
    keyed_data2.last_shown_time = kLastShownTime2;
    data.shown_for_keys.emplace(kAppName1, keyed_data1);
    data.shown_for_keys.emplace(kAppName2, keyed_data2);
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
    EXPECT_THAT(actual->shown_for_keys,
                testing::ContainerEq(expected.shown_for_keys));
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

  void SaveNewBadgeData(const user_education::NewBadgeData& data,
                        const base::Feature& to_save_data_for) {
    service_.SaveNewBadgeData(to_save_data_for, data);
  }

  void CompareNewBadgeData(const user_education::NewBadgeData& expected,
                           const base::Feature& to_read_data_for) {
    const auto actual = service_.ReadNewBadgeData(to_read_data_for);
    EXPECT_EQ(expected.show_count, actual.show_count);
    EXPECT_EQ(expected.used_count, actual.used_count);
    EXPECT_EQ(expected.feature_enabled_time, actual.feature_enabled_time);
  }

  void ResetNewBadgeData(const base::Feature& to_reset_data_for) {
    service_.ResetNewBadge(to_reset_data_for);
  }

  void SaveRecentSessionData(const RecentSessionData& data) {
    service_.SaveRecentSessionData(data);
  }

  void ResetRecentSessionData() { service_.ResetRecentSessionData(); }

  void CompareRecentSessionData(const RecentSessionData& expected) {
    const auto actual = service_.ReadRecentSessionData();
    EXPECT_THAT(actual.recent_session_start_times,
                testing::ContainerEq(expected.recent_session_start_times));
    EXPECT_EQ(expected.enabled_time, actual.enabled_time);
  }

  Profile& profile() { return profile_; }
  BrowserFeaturePromoStorageService& service() { return service_; }

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
  data.shown_for_keys.clear();
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
  data2.shown_for_keys.clear();
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

TEST_F(BrowserFeaturePromoStorageServiceTest, NoNewBadgeDataByDefault) {
  CompareNewBadgeData(user_education::NewBadgeData(), kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndReadsNewBadgeData) {
  user_education::NewBadgeData data;
  data.show_count = 2;
  data.used_count = 3;
  data.feature_enabled_time = base::Time::Now();
  SaveNewBadgeData(data, kTestIPHFeature);
  CompareNewBadgeData(data, kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndClearNewBadgeData) {
  user_education::NewBadgeData data;
  data.show_count = 2;
  data.used_count = 3;
  data.feature_enabled_time = base::Time::Now();
  SaveNewBadgeData(data, kTestIPHFeature);
  ResetNewBadgeData(kTestIPHFeature);
  CompareNewBadgeData(user_education::NewBadgeData(), kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SaveNewBadgeDataAgain) {
  user_education::NewBadgeData data;
  SaveNewBadgeData(data, kTestIPHFeature);
  data.show_count = 2;
  data.used_count = 3;
  data.feature_enabled_time = base::Time::Now();
  SaveNewBadgeData(data, kTestIPHFeature);
  CompareNewBadgeData(data, kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SaveMultipleNewBadgeData) {
  user_education::NewBadgeData data;
  data.show_count = 2;
  data.used_count = 3;
  data.feature_enabled_time = base::Time::Now();
  user_education::NewBadgeData data2;
  data2.show_count = 4;
  data2.used_count = 1;
  data2.feature_enabled_time = base::Time::Now();
  SaveNewBadgeData(data, kTestIPHFeature);
  CompareNewBadgeData(user_education::NewBadgeData(), kTestIPHFeature2);
  SaveNewBadgeData(data2, kTestIPHFeature2);
  CompareNewBadgeData(data, kTestIPHFeature);
  CompareNewBadgeData(data2, kTestIPHFeature2);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SaveAndRestoreRecentSessionData) {
  CompareRecentSessionData(RecentSessionData());
  RecentSessionData data;
  data.enabled_time = base::Time::FromSecondsSinceUnixEpoch(10);
  data.recent_session_start_times = {
      base::Time::FromSecondsSinceUnixEpoch(100000),
      base::Time::FromSecondsSinceUnixEpoch(10000),
      base::Time::FromSecondsSinceUnixEpoch(1000),
      base::Time::FromSecondsSinceUnixEpoch(100),
  };
  SaveRecentSessionData(data);
  CompareRecentSessionData(data);
}

TEST_F(BrowserFeaturePromoStorageServiceTest,
       SaveRecentSessionDataMultipleTimes) {
  CompareRecentSessionData(RecentSessionData());
  RecentSessionData data;
  data.enabled_time = base::Time::FromSecondsSinceUnixEpoch(10);
  data.recent_session_start_times = {
      base::Time::FromSecondsSinceUnixEpoch(100000),
      base::Time::FromSecondsSinceUnixEpoch(10000),
      base::Time::FromSecondsSinceUnixEpoch(1000),
      base::Time::FromSecondsSinceUnixEpoch(100),
  };
  SaveRecentSessionData(data);

  // Add a new entry to the front of the list, save, and verify.
  // This ensures that an existing list can be safely added to.
  data.recent_session_start_times.insert(
      data.recent_session_start_times.begin(),
      base::Time::FromSecondsSinceUnixEpoch(110000));
  SaveRecentSessionData(data);
  CompareRecentSessionData(data);

  // Replace the entire list with a single, later entry, then save and verify.
  // This ensures that we do not trigger a previous bug where the old data was
  // not removed from prefs.
  data.recent_session_start_times.clear();
  data.recent_session_start_times.emplace_back(
      base::Time::FromSecondsSinceUnixEpoch(120000));
  SaveRecentSessionData(data);
  CompareRecentSessionData(data);
}

TEST_F(BrowserFeaturePromoStorageServiceTest,
       SaveAndRestoreRecentSessionData_NoEnabledTime) {
  CompareRecentSessionData(RecentSessionData());
  RecentSessionData data;
  data.recent_session_start_times = {
      base::Time::FromSecondsSinceUnixEpoch(1000),
      base::Time::FromSecondsSinceUnixEpoch(100),
  };
  SaveRecentSessionData(data);
  CompareRecentSessionData(data);
}

TEST_F(BrowserFeaturePromoStorageServiceTest,
       SaveAndRestoreRecentSessionData_ElidesOutOfOrderEntries) {
  RecentSessionData data;
  data.enabled_time = base::Time::FromSecondsSinceUnixEpoch(10);
  data.recent_session_start_times = {
      base::Time::FromSecondsSinceUnixEpoch(10000),
      base::Time::FromSecondsSinceUnixEpoch(100000),
      base::Time::FromSecondsSinceUnixEpoch(1000),
      base::Time::FromSecondsSinceUnixEpoch(100),
  };
  SaveRecentSessionData(data);

  // Expected entries will elide the out-of-order entry.
  data.recent_session_start_times = {
      base::Time::FromSecondsSinceUnixEpoch(10000),
      base::Time::FromSecondsSinceUnixEpoch(1000),
      base::Time::FromSecondsSinceUnixEpoch(100),
  };
  CompareRecentSessionData(data);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, ResetRecentSessionData) {
  RecentSessionData data;
  data.enabled_time = base::Time::FromSecondsSinceUnixEpoch(10);
  data.recent_session_start_times = {
      base::Time::FromSecondsSinceUnixEpoch(100000),
      base::Time::FromSecondsSinceUnixEpoch(10000),
      base::Time::FromSecondsSinceUnixEpoch(1000),
      base::Time::FromSecondsSinceUnixEpoch(100),
  };
  SaveRecentSessionData(data);
  ResetRecentSessionData();
  CompareRecentSessionData(RecentSessionData());
}

TEST_F(BrowserFeaturePromoStorageServiceTest, LegacyDataTest) {
  static constexpr base::Time kLastSnoozeTime = kSessionTime - base::Days(30);

  {
    static constexpr char kPromoDataPath[] = "in_product_help.snoozed_feature";
    ScopedDictPrefUpdate update(profile().GetPrefs(), kPromoDataPath);
    auto& pref_data = update.Get();

    pref_data.SetByDottedPath("TestIPHFeature.is_dismissed", false);
    pref_data.SetByDottedPath("TestIPHFeature.last_snooze_time",
                              base::TimeToValue(kLastSnoozeTime));
    pref_data.SetByDottedPath("TestIPHFeature.snooze_count", 1);

    base::Value::List shown_for;
    shown_for.Append(kAppName1);
    pref_data.SetByDottedPath("TestIPHFeature.shown_for_apps",
                              std::move(shown_for));
  }

  user_education::FeaturePromoData data;
  base::SimpleTestClock test_clock;
  service().set_clock_for_testing(&test_clock);
  const base::Time kCurrentTime = kSessionTime + base::Hours(1);
  test_clock.SetNow(kCurrentTime);

  // These are the values that were explicitly written.
  data.is_dismissed = false;
  data.last_snooze_time = kLastSnoozeTime;
  data.snooze_count = 1;

  // These values are auto-generated when missing as follows:
  data.last_show_time = data.last_snooze_time - base::Seconds(1);
  data.show_count = data.snooze_count;
  data.first_show_time = data.last_show_time;
  data.last_dismissed_by = user_education::FeaturePromoClosedReason::kCancel;

  // This demonstrates default values for old-style keyed promos.
  user_education::KeyedFeaturePromoData key_data;
  key_data.show_count = 1;
  key_data.last_shown_time = kCurrentTime;
  data.shown_for_keys.emplace(kAppName1, key_data);

  // See if this data matches the expectation.
  CompareData(data, kTestIPHFeature);

  // Reset the clock so there is no dangling pointer.
  service().set_clock_for_testing(base::DefaultClock::GetInstance());
}
