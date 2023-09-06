// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for family user metrics service.
class SupervisedUserMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    supervised_user::SupervisedUserMetricsService::RegisterProfilePrefs(
        pref_service_.registry());
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        supervised_user::SupervisedUserURLFilter::ALLOW);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSupervisedUserSafeSites, true);
    filter_.SetDefaultFilteringBehavior(
        supervised_user::SupervisedUserURLFilter::ALLOW);
    filter_.SetFilterInitialized(true);

    supervised_user_metrics_service_ =
        std::make_unique<supervised_user::SupervisedUserMetricsService>(
            &pref_service_, &filter_);
  }

  void TearDown() override { supervised_user_metrics_service_->Shutdown(); }

 protected:
  int GetDayIdPref() {
    return pref_service_.GetInteger(prefs::kSupervisedUserMetricsDayId);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestingPrefServiceSimple pref_service_;
  supervised_user::SupervisedUserURLFilter filter_ =
      supervised_user::SupervisedUserURLFilter(
          base::BindRepeating([](const GURL& url) { return false; }),
          std::make_unique<supervised_user::FakeURLFilterDelegate>());
  std::unique_ptr<supervised_user::SupervisedUserMetricsService>
      supervised_user_metrics_service_;
};

// Tests OnNewDay() is called after more than one day passes.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMultipleDays) {
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(supervised_user::SupervisedUserMetricsService::GetDayIdForTesting(
                base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAtMidnight) {
  task_environment_.FastForwardBy(base::Hours(3));
  EXPECT_EQ(supervised_user::SupervisedUserMetricsService::GetDayIdForTesting(
                base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMidnight) {
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(supervised_user::SupervisedUserMetricsService::GetDayIdForTesting(
                base::Time::Now()),
            GetDayIdPref());
}
