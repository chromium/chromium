// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
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

  static BrowserFeaturePromoStorageService::PromoData CreateTestData() {
    BrowserFeaturePromoStorageService::PromoData data;
    data.is_dismissed = true;
    data.last_dismissed_by =
        BrowserFeaturePromoStorageService::CloseReason::kSnooze;
    data.last_show_time = base::Time::FromMillisecondsSinceUnixEpoch(1);
    data.last_snooze_time = base::Time::FromMillisecondsSinceUnixEpoch(2);
    data.snooze_count = 3;
    data.show_count = 4;
    data.last_snooze_duration = base::Days(3);
    data.shown_for_apps.insert(kAppName1);
    data.shown_for_apps.insert(kAppName2);
    return data;
  }

  static void CompareData(
      const BrowserFeaturePromoStorageService::PromoData& expected,
      const BrowserFeaturePromoStorageService::PromoData& actual) {
    EXPECT_EQ(expected.is_dismissed, actual.is_dismissed);
    EXPECT_EQ(expected.last_show_time, actual.last_show_time);
    EXPECT_EQ(expected.last_snooze_time, actual.last_snooze_time);
    EXPECT_EQ(expected.snooze_count, actual.snooze_count);
    EXPECT_EQ(expected.show_count, actual.show_count);
    EXPECT_EQ(expected.last_snooze_duration, actual.last_snooze_duration);
    EXPECT_THAT(actual.shown_for_apps,
                testing::ContainerEq(expected.shown_for_apps));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  BrowserFeaturePromoStorageService service_;
};

TEST_F(BrowserFeaturePromoStorageServiceTest, NoDataByDefault) {
  EXPECT_FALSE(service_.ReadPromoData(kTestIPHFeature).has_value());
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndReadsData) {
  const auto data = CreateTestData();
  service_.SavePromoData(kTestIPHFeature, data);
  const auto result = service_.ReadPromoData(kTestIPHFeature);
  ASSERT_TRUE(result.has_value());
  CompareData(data, *result);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SaveAgain) {
  auto data = CreateTestData();
  service_.SavePromoData(kTestIPHFeature, data);
  data.shown_for_apps.clear();
  data.is_dismissed = false;
  data.show_count++;
  service_.SavePromoData(kTestIPHFeature, data);
  const auto result = service_.ReadPromoData(kTestIPHFeature);
  ASSERT_TRUE(result.has_value());
  CompareData(data, *result);
}

TEST_F(BrowserFeaturePromoStorageServiceTest, ResetClearsData) {
  const auto data = CreateTestData();
  service_.SavePromoData(kTestIPHFeature, data);
  service_.Reset(kTestIPHFeature);
  const auto result = service_.ReadPromoData(kTestIPHFeature);
  ASSERT_FALSE(result.has_value());
}

TEST_F(BrowserFeaturePromoStorageServiceTest, SavesAndReadsMultipleFeatures) {
  const auto data = CreateTestData();
  service_.SavePromoData(kTestIPHFeature, data);
  EXPECT_FALSE(service_.ReadPromoData(kTestIPHFeature2).has_value());
  auto data2 = CreateTestData();
  data2.is_dismissed = false;
  data2.last_dismissed_by =
      BrowserFeaturePromoStorageService::CloseReason::kCancel;
  data2.shown_for_apps.clear();
  data2.show_count = 6;
  service_.SavePromoData(kTestIPHFeature2, data2);
  CompareData(data, *service_.ReadPromoData(kTestIPHFeature));
  CompareData(data2, *service_.ReadPromoData(kTestIPHFeature2));
}
