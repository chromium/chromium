// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {
constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";
constexpr char kApprovedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Approved";
constexpr char kBlockedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Blocked";

// Tests for family user metrics service.
class SupervisedUserMetricsServiceTest : public testing::Test {
 protected:
  ~SupervisedUserMetricsServiceTest() override {
    supervised_user_test_environment_.Shutdown();
  }

  int GetDayIdPref() {
    return supervised_user_test_environment_.pref_service()->GetInteger(
        prefs::kSupervisedUserMetricsDayId);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
};

// Tests that the recorded day is updated after more than one day passes.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMultipleDays) {
  EnableParentalControls(*supervised_user_test_environment_.pref_service());

  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
  EXPECT_NE(0, GetDayIdPref());
}

// Tests that the recorded day is updated after metrics service is created.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterServiceCreation) {
  EnableParentalControls(*supervised_user_test_environment_.pref_service());

  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
  EXPECT_NE(0, GetDayIdPref());
}

// Tests that the recorded day is updated only after a supervised user is
// detected.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterSupervisedUserDetected) {
  DisableParentalControls(*supervised_user_test_environment_.pref_service());

  task_environment_.FastForwardBy(base::Hours(1));
  // Day ID should not change if the filter is not initialized.
  EXPECT_EQ(0, GetDayIdPref());

  EnableParentalControls(*supervised_user_test_environment_.pref_service());
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests that metrics are not recorded for unsupervised users.
TEST_F(SupervisedUserMetricsServiceTest,
       MetricsNotRecordedForSignedOutSupervisedUser) {
  DisableParentalControls(*supervised_user_test_environment_.pref_service());
  histogram_tester_.ExpectTotalCount(kWebFilterTypeHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kManagedSiteListHistogramName,
                                     /*expected_count=*/0);
}

// Tests that default metrics are recorded for supervised users whose parent has
// not changed the initial configuration.
TEST_F(SupervisedUserMetricsServiceTest, RecordDefaultMetrics) {
  // If the parent has not changed their configuration the supervised user
  // should be subject to default mature sites blocking.
  EnableParentalControls(*supervised_user_test_environment_.pref_service());
  histogram_tester_.ExpectUniqueSample(kWebFilterTypeHistogramName,
                                       /*sample=*/
                                       WebFilterType::kTryToBlockMatureSites,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kEmpty,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kApprovedSitesCountHistogramName,
                                       /*sample=*/0,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kBlockedSitesCountHistogramName,
                                       /*sample=*/0,
                                       /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace supervised_user
