// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

class MockSupervisedUserMetricsServiceExtensionDelegateImpl
    : public SupervisedUserMetricsService::
          SupervisedUserMetricsServiceExtensionDelegate {
 private:
  // SupervisedUserMetricsServiceExtensionDelegate implementation:
  bool RecordExtensionsMetrics() override { return false; }
};

// Tests for family user metrics service.
class SupervisedUserMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
    SupervisedUserMetricsService::RegisterProfilePrefs(
        pref_service_.registry());

    settings_service_.Init(pref_service_.user_prefs_store());

    EnableParentalControls(pref_service_);
    supervised_user_service_ = std::make_unique<SupervisedUserService>(
        identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), pref_service_,
        settings_service_, &sync_service_,
        std::make_unique<FakeURLFilterDelegate>(),
        std::make_unique<FakePlatformDelegate>(),
        /*can_show_first_time_interstitial_banner=*/false);
    supervised_user_service_->Init();
  }

  void TearDown() override {
    settings_service_.Shutdown();
    supervised_user_metrics_service_->Shutdown();
    supervised_user_service_->Shutdown();
  }

 protected:
  int GetDayIdPref() {
    return pref_service_.GetInteger(prefs::kSupervisedUserMetricsDayId);
  }

  // Creates the metrics service under test.
  void CreateMetricsService() {
    supervised_user_metrics_service_ =
        std::make_unique<SupervisedUserMetricsService>(
            &pref_service_, GetURLFilter(),
            std::make_unique<
                MockSupervisedUserMetricsServiceExtensionDelegateImpl>());
  }

  SupervisedUserURLFilter* GetURLFilter() {
    return supervised_user_service_->GetURLFilter();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

  sync_preferences::TestingPrefServiceSyncable pref_service_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<SupervisedUserService> supervised_user_service_;

  SupervisedUserSettingsService settings_service_;
  syncer::MockSyncService sync_service_;

  std::unique_ptr<SupervisedUserMetricsService>
      supervised_user_metrics_service_;
};

// Tests that the recorded day is updated after more than one day passes.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMultipleDays) {
  CreateMetricsService();

  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
  EXPECT_NE(0, GetDayIdPref());
}

// Tests that the recorded day is updated after metrics service is created.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterServiceCreation) {
  CreateMetricsService();

  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
  EXPECT_NE(0, GetDayIdPref());
}

// Tests that the recorded day is updated only after a supervised user is
// detected.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterSupervisedUserDetected) {
  DisableParentalControls(pref_service_);
  CreateMetricsService();

  task_environment_.FastForwardBy(base::Hours(1));
  // Day ID should not change if the filter is not initialized.
  EXPECT_EQ(0, GetDayIdPref());

  EnableParentalControls(pref_service_);
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests that metrics are not recorded for unsupervised users.
TEST_F(SupervisedUserMetricsServiceTest,
       MetricsNotRecordedForSignedOutSupervisedUser) {
  DisableParentalControls(pref_service_);
  CreateMetricsService();
  histogram_tester_.ExpectTotalCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*expected_count=*/0);
}

// Tests that default metrics are recorded for supervised users whose parent has
// not changed the initial configuration.
TEST_F(SupervisedUserMetricsServiceTest, RecordDefaultMetrics) {
  // If the parent has not changed their configuration the supervised user
  // should be subject to default mature sites blocking.
  CreateMetricsService();
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kEmpty,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_bucket_count=*/1);
}

}  // namespace supervised_user
