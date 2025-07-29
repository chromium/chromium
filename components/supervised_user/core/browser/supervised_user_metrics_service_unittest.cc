// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {

using ::testing::_;
using ::testing::Eq;

constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";
constexpr char kWebFilterTypeForFamilyUserHistogramName[] =
    "SupervisedUsers.WebFilterType.FamilyLink";

#if BUILDFLAG(IS_ANDROID)
constexpr char kWebFilterTypeForLocallySupervisedHistogramName[] =
    "SupervisedUsers.WebFilterType.LocallySupervised";
#endif  // BUILDFLAG(IS_ANDROID)
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";
constexpr char kApprovedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Approved";
constexpr char kBlockedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Blocked";

class SupervisedUserMetricsServiceTest : public ::testing::Test {
 protected:
  // Explicit environment initialization reduces the number of fixtures.
  void Initialize(InitialSupervisionState initial_state) {
    supervised_user_test_environment_ =
        std::make_unique<SupervisedUserTestEnvironment>(initial_state);
  }

  void TearDown() override { supervised_user_test_environment_->Shutdown(); }

  int GetDayIdPref() {
    return supervised_user_test_environment_->pref_service()->GetInteger(
        prefs::kSupervisedUserMetricsDayId);
  }

#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list_{
      kPropagateDeviceContentFiltersToSupervisedUser};
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<SupervisedUserTestEnvironment>
      supervised_user_test_environment_;
};

// Tests that the recorded day is updated after more than one day passes.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMultipleDays) {
  Initialize(InitialSupervisionState::kFamilyLinkDefault);
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
  EXPECT_NE(0, GetDayIdPref());
}

// Tests that the recorded day is updated after metrics service is created.
TEST_F(SupervisedUserMetricsServiceTest,
       NewDayAfterServiceCreationForFamilyLink) {
  Initialize(InitialSupervisionState::kFamilyLinkDefault);
  EXPECT_NE(0, GetDayIdPref());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(SupervisedUserMetricsServiceTest,
       NewDayAfterServiceCreationForLocallySupervised) {
  Initialize(InitialSupervisionState::kSupervisedWithAllContentFilters);
  EXPECT_NE(0, GetDayIdPref());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Unsupervised user doesn't emit metrics and will not have a day id pref.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterSupervisedUserDetected) {
  Initialize(InitialSupervisionState::kUnsupervised);
  EXPECT_EQ(0, GetDayIdPref());

  // Advancing clock won't change the day id pref - user is not supervised.
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(0, GetDayIdPref());

  EnableParentalControls(*supervised_user_test_environment_->pref_service());
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests that metrics are not recorded for unsupervised users.
TEST_F(SupervisedUserMetricsServiceTest,
       MetricsNotRecordedForSignedOutSupervisedUser) {
  Initialize(InitialSupervisionState::kUnsupervised);
  histogram_tester_.ExpectTotalCount(kWebFilterTypeHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kWebFilterTypeForFamilyUserHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kManagedSiteListHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kApprovedSitesCountHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kBlockedSitesCountHistogramName,
                                     /*expected_count=*/0);
}

TEST_F(SupervisedUserMetricsServiceTest,
       MetricsRecordedForFamilyLinkSupervisedUser) {
  Initialize(InitialSupervisionState::kFamilyLinkDefault);

  histogram_tester_.ExpectUniqueSample(kWebFilterTypeHistogramName,
                                       /*sample=*/
                                       WebFilterType::kTryToBlockMatureSites,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kWebFilterTypeForFamilyUserHistogramName,
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(SupervisedUserMetricsServiceTest,
       MetricsRecordedForLocallySupervisedUser) {
  Initialize(InitialSupervisionState::kSupervisedWithAllContentFilters);
  histogram_tester_.ExpectTotalCount(
      kWebFilterTypeForLocallySupervisedHistogramName,
      /*expected_count=*/1);
  // For this type of supervised user, the managed site list is not reported.
  histogram_tester_.ExpectTotalCount(kManagedSiteListHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kApprovedSitesCountHistogramName,
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(kBlockedSitesCountHistogramName,
                                     /*expected_count=*/0);
}

using FieldTrialName = std::string;
// Test suite to check correct association with synthetic field trials
class SupervisedUserMetricsServiceFieldTrialTest
    : public testing::TestWithParam<FieldTrialName> {
 protected:
  ~SupervisedUserMetricsServiceFieldTrialTest() override {
    test_environment_->Shutdown();
  }

  void ToggleContentFilter(bool enabled) {
    CHECK(test_environment_)
        << "Create test environment first with CreateTestEnvironment().";

    if (GetFieldTrialName() == "AndroidDeviceSearchContentFilters") {
      test_environment_->search_content_filters_observer()->SetEnabled(enabled);
      return;
    } else if (GetFieldTrialName() == "AndroidDeviceBrowserContentFilters") {
      test_environment_->browser_content_filters_observer()->SetEnabled(
          enabled);
      return;
    }

    NOTREACHED() << "Unsupported field trial name: " << GetFieldTrialName();
  }

  void CreateTestEnvironment(std::unique_ptr<MetricsServiceAccessorDelegateMock>
                                 metrics_service_accessor_delegate) {
    test_environment_ = std::make_unique<SupervisedUserTestEnvironment>(
        std::move(metrics_service_accessor_delegate));
  }

  static FieldTrialName GetFieldTrialName() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list{
      kPropagateDeviceContentFiltersToSupervisedUser};
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<SupervisedUserTestEnvironment> test_environment_;
};

TEST_P(SupervisedUserMetricsServiceFieldTrialTest,
       SyntheticFieldTrialRegistered) {
  // Register calls before environment's created, because the metrics service
  // calls field trial registration on creation.
  auto mock = std::make_unique<MetricsServiceAccessorDelegateMock>();
  EXPECT_CALL(*mock, RegisterSyntheticFieldTrial(_, "Disabled")).Times(1);
  EXPECT_CALL(*mock, RegisterSyntheticFieldTrial(Eq(GetParam()), "Disabled"))
      .Times(2);
  EXPECT_CALL(*mock, RegisterSyntheticFieldTrial(Eq(GetParam()), "Enabled"))
      .Times(1);

  CreateTestEnvironment(std::move(mock));

  // This cycles all possible combinations of states for each filter:
  // off -> on -> off.
  // There should be a all-disabled initial registration when the metrics
  // service is created.
  ToggleContentFilter(/*enabled=*/true);
  ToggleContentFilter(/*enabled=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserMetricsServiceFieldTrialTest,
    testing::Values("AndroidDeviceSearchContentFilters",
                    "AndroidDeviceBrowserContentFilters"),
    [](const testing::TestParamInfo<FieldTrialName>& info) {
      return info.param;
    });

#endif  // BUILDFLAG(IS_ANDROID)

struct PeriodicalWebFilterTypeTestParams {
  std::string test_name;
  // Which user type to start with.
  InitialSupervisionState initial_supervision_state;
  // In these histograms, verify emissions of
  // WebFilterType::kTryToBlockMatureSites - default filtering settings.
  std::vector<std::string> interesting_histogram_names;
  // Whether we expect any emissions at all.
  bool expect_emissions;
};

class SupervisedUserMetricsServiceWebFilterTypePeriodicalTest
    : public SupervisedUserMetricsServiceTest,
      public ::testing::WithParamInterface<PeriodicalWebFilterTypeTestParams> {
};

TEST_P(SupervisedUserMetricsServiceWebFilterTypePeriodicalTest,
       WebFilterTypeEmittedDaily) {
  Initialize(GetParam().initial_supervision_state);
  // Check post-initialization counts of FamilyUser metrics reset histogram
  // tracker.
  for (const std::string& histogram_name :
       GetParam().interesting_histogram_names) {
    histogram_tester_.ExpectBucketCount(histogram_name,
                                        WebFilterType::kTryToBlockMatureSites,
                                        GetParam().expect_emissions ? 1 : 0);
  }

  int start_day_id =
      base::Time::Now().LocalMidnight().since_origin().InDaysFloored();
  EXPECT_EQ(GetParam().expect_emissions ? start_day_id : 0,
            supervised_user_test_environment_->pref_service()->GetInteger(
                prefs::kSupervisedUserMetricsDayId));

  // Advance clock by some days and note how many calendar days have passed
  // (DST, service's polling resolution and other details can affect the day
  // count).
  task_environment_.FastForwardBy(base::Days(1));
  int end_day_id =
      base::Time::Now().LocalMidnight().since_origin().InDaysFloored();

  // Initial day emission + one for each *new* day or none.
  int expected_count =
      GetParam().expect_emissions ? (1 + (end_day_id - start_day_id)) : 0;

  for (const std::string& histogram_name :
       GetParam().interesting_histogram_names) {
    histogram_tester_.ExpectBucketCount(
        histogram_name, WebFilterType::kTryToBlockMatureSites, expected_count);
  }
}


const PeriodicalWebFilterTypeTestParams kPeriodicalWebFilterTypeTestParams[] = {
    {"Unsupervised",
     InitialSupervisionState::kUnsupervised,
     {kWebFilterTypeHistogramName,
#if BUILDFLAG(IS_ANDROID)
      kWebFilterTypeForLocallySupervisedHistogramName,
#endif  // BUILDFLAG(IS_ANDROID)
      kWebFilterTypeForFamilyUserHistogramName},
     /*expect_emissions=*/false},
    {"SupervisedWithParentalControls",
     InitialSupervisionState::kFamilyLinkDefault,
     {kWebFilterTypeHistogramName, kWebFilterTypeForFamilyUserHistogramName},
     /*expect_emissions=*/true},
#if BUILDFLAG(IS_ANDROID)
    {"SupervisedLocally",
     InitialSupervisionState::kSupervisedWithAllContentFilters,
     {kWebFilterTypeForLocallySupervisedHistogramName},
     /*expect_emissions=*/true},
#endif  // BUILDFLAG(IS_ANDROID)
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserMetricsServiceWebFilterTypePeriodicalTest,
    testing::ValuesIn(kPeriodicalWebFilterTypeTestParams),
    [](const testing::TestParamInfo<
        SupervisedUserMetricsServiceWebFilterTypePeriodicalTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace
}  // namespace supervised_user
