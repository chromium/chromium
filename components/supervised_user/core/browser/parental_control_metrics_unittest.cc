// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/parental_control_metrics.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class ParentalControlMetricsTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        supervised_user::SupervisedUserURLFilter::ALLOW);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kSupervisedUserSafeSites, true);

    // Prepare args for the AsyncURLChecker.
    kids_chrome_management_client_ =
        std::make_unique<KidsChromeManagementClient>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            identity_test_env_.identity_manager());

    filter_.SetFilterInitialized(true);
    parental_control_metrics_ =
        std::make_unique<supervised_user::ParentalControlMetrics>(
            pref_service_.get(), &filter_);
  }

 protected:
  void OnNewDay() { parental_control_metrics_->OnNewDay(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<KidsChromeManagementClient> kids_chrome_management_client_;
  supervised_user::SupervisedUserURLFilter filter_ =
      supervised_user::SupervisedUserURLFilter(
          base::BindRepeating([](const GURL& url) { return false; }),
          std::make_unique<MockServiceDelegate>());

  std::unique_ptr<supervised_user::ParentalControlMetrics>
      parental_control_metrics_;
  base::HistogramTester histogram_tester_;

 private:
  // TODO(b/279164926): update SupervisedUserURLFilter::Delegate to prevent
  // replicating MockServiceDelegate code.
  class MockServiceDelegate
      : public supervised_user::SupervisedUserURLFilter::Delegate {
   public:
    std::string GetCountryCode() override { return std::string(); }
  };
};

TEST_F(ParentalControlMetricsTest, WebFilterTypeMetric) {
  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered policies change or
  // OnNewDay(). Since the default values are the same of override values, the
  // WebFilterType doesn't change and no report here.
  pref_service_->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                            supervised_user::SupervisedUserURLFilter::ALLOW);
  pref_service_->SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // Tests filter "try to block mature sites".
  filter_.InitAsyncURLChecker(kids_chrome_management_client_.get());
  OnNewDay();
  histogram_tester_.ExpectUniqueSample(
      supervised_user::SupervisedUserURLFilter::
          GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::WebFilterType::
          kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);

  // Tests filter "allow all sites".
  pref_service_->SetBoolean(prefs::kSupervisedUserSafeSites, false);
  filter_.ClearAsyncURLChecker();
  OnNewDay();
  histogram_tester_.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::WebFilterType::kAllowAllSites,
      /*expected_count=*/1);

  // Tests filter "only allow certain sites".
  pref_service_->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                            supervised_user::SupervisedUserURLFilter::BLOCK);
  filter_.SetDefaultFilteringBehavior(
      supervised_user::SupervisedUserURLFilter::BLOCK);
  OnNewDay();
  histogram_tester_.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::WebFilterType::kCertainSites,
      /*expected_count=*/1);

  histogram_tester_.ExpectTotalCount(supervised_user::SupervisedUserURLFilter::
                                         GetWebFilterTypeHistogramNameForTest(),
                                     /*expected_count=*/3);
}

TEST_F(ParentalControlMetricsTest, ManagedSiteListTypeMetric) {
  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered by policies change or
  // OnNewDay(). Since the default values are the same of override values, the
  // WebFilterType doesn't change and no report here.
  pref_service_->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                            supervised_user::SupervisedUserURLFilter::ALLOW);
  pref_service_->SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // Tests daily report.
  OnNewDay();
  histogram_tester_.ExpectUniqueSample(
      supervised_user::SupervisedUserURLFilter::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::ManagedSiteList::kEmpty,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      supervised_user::SupervisedUserURLFilter::
          GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      supervised_user::SupervisedUserURLFilter::
          GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_bucket_count=*/1);
}
