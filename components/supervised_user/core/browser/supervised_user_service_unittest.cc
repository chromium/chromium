// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {

const char kExampleUrl0[] = "http://www.example0.com";
const char kExampleUrl1[] = "http://www.example1.com/123";

}  // namespace

class SupervisedUserServiceTestBase : public ::testing::Test {
 public:
  explicit SupervisedUserServiceTestBase(bool is_supervised) {
    settings_service_.Init(syncable_pref_service_.user_prefs_store());
    supervised_user::RegisterProfilePrefs(syncable_pref_service_.registry());
    if (is_supervised) {
      syncable_pref_service_.SetString(prefs::kSupervisedUserId,
                                       kChildAccountSUID);
    } else {
      syncable_pref_service_.ClearPref(prefs::kSupervisedUserId);
    }

    service_ = std::make_unique<SupervisedUserService>(
        identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), syncable_pref_service_,
        settings_service_, &sync_service_,
        std::make_unique<FakeURLFilterDelegate>(),
        std::make_unique<FakePlatformDelegate>(),
        /*can_show_first_time_interstitial_banner=*/true);

    service_->Init();
  }

  void TearDown() override {
    settings_service_.Shutdown();
    service_->Shutdown();
  }

 protected:
  // Dependencies (ordering among the groups matters for object desturction).
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  syncer::MockSyncService sync_service_;
  sync_preferences::TestingPrefServiceSyncable syncable_pref_service_;
  SupervisedUserSettingsService settings_service_;

  std::unique_ptr<SupervisedUserService> service_;
};

class SupervisedUserServiceTest : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTest()
      : SupervisedUserServiceTestBase(/*is_supervised=*/true) {}
};

// Tests that web approvals are enabled for supervised users.
TEST_F(SupervisedUserServiceTest, ApprovalRequestsEnabled) {
  ASSERT_TRUE(
      service_->remote_web_approvals_manager().AreApprovalRequestsEnabled());
}

// Tests that restricting all site navigation is applied to supervised users.
TEST_F(SupervisedUserServiceTest, UrlIsBlockedForUser) {
  // Set "only allow certain sites" filter.
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(FilteringBehavior::kBlock));
  service_->GetURLFilter()->SetDefaultFilteringBehavior(
      FilteringBehavior::kBlock);

  ASSERT_TRUE(service_->IsBlockedURL(GURL("http://google.com")));
}

// Tests that allowing all site navigation is applied to supervised users.
TEST_F(SupervisedUserServiceTest, UrlIsAllowedForUser) {
  // Set "allow all sites" filter.
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(FilteringBehavior::kAllow));
  syncable_pref_service_.SetBoolean(prefs::kSupervisedUserSafeSites, false);

  ASSERT_FALSE(service_->IsBlockedURL(GURL("http://google.com")));
}

// Tests that changes in parent configuration for web filter types are recorded.
TEST_F(SupervisedUserServiceTest, WebFilterTypeOnPrefsChange) {
  base::HistogramTester histogram_tester;

  // Tests filter "try to block mature sites".
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(FilteringBehavior::kAllow));
  syncable_pref_service_.SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // This should not increase since only changes from the default are recorded.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/0);

  // Tests filter "allow all sites".
  syncable_pref_service_.SetBoolean(prefs::kSupervisedUserSafeSites, false);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      WebFilterType::kAllowAllSites,
      /*expected_count=*/1);

  // Tests filter "only allow certain sites".
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(FilteringBehavior::kBlock));
  service_->GetURLFilter()->SetDefaultFilteringBehavior(
      FilteringBehavior::kBlock);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      WebFilterType::kCertainSites,
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*expected_count=*/2);
}

// Tests that changes to the allow or blocklist of the parent configuration are
// recorded.
TEST_F(SupervisedUserServiceTest, ManagedSiteListTypeMetricOnPrefsChange) {
  base::HistogramTester histogram_tester;

  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered by policies change. Since
  // the default values are the same of override values, the WebFilterType
  // doesn't change and no report here.
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(FilteringBehavior::kAllow));
  syncable_pref_service_.SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // Blocks `kExampleUrl0`.
  {
    ScopedDictPrefUpdate hosts_update(&syncable_pref_service_,
                                      prefs::kSupervisedUserManualHosts);
    base::Value::Dict& hosts = hosts_update.Get();
    hosts.Set(kExampleUrl0, false);
  }

  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBlockedListOnly,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/1);

  // Approves `kExampleUrl0`.
  {
    ScopedDictPrefUpdate hosts_update(&syncable_pref_service_,
                                      prefs::kSupervisedUserManualHosts);
    base::Value::Dict& hosts = hosts_update.Get();
    hosts.Set(kExampleUrl0, true);
  }

  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kApprovedListOnly,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);

  // Blocks `kExampleURL1`.
  {
    ScopedDictPrefUpdate urls_update(&syncable_pref_service_,
                                     prefs::kSupervisedUserManualURLs);
    base::Value::Dict& urls = urls_update.Get();
    urls.Set(kExampleUrl1, false);
  }

  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBoth,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/2);

  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*expected_count=*/3);
  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*expected_count=*/3);
  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*expected_count=*/3);
}

TEST_F(SupervisedUserServiceTest, InterstitialBannerState) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_IOS)
  EXPECT_TRUE(service_->GetUpdatedBannerState(
                  FirstTimeInterstitialBannerState::kUnknown) ==
              FirstTimeInterstitialBannerState::kNeedToShow);
  EXPECT_TRUE(service_->GetUpdatedBannerState(
                  FirstTimeInterstitialBannerState::kNeedToShow) ==
              FirstTimeInterstitialBannerState::kNeedToShow);
  EXPECT_TRUE(service_->GetUpdatedBannerState(
                  FirstTimeInterstitialBannerState::kSetupComplete) ==
              FirstTimeInterstitialBannerState::kSetupComplete);
#else
  {
    // On other platforms, the state is marked complete.
    EXPECT_TRUE(service_->GetUpdatedBannerState(
                    FirstTimeInterstitialBannerState::kUnknown) ==
                FirstTimeInterstitialBannerState::kSetupComplete);
    EXPECT_TRUE(service_->GetUpdatedBannerState(
                    FirstTimeInterstitialBannerState::kNeedToShow) ==
                FirstTimeInterstitialBannerState::kSetupComplete);
    EXPECT_TRUE(service_->GetUpdatedBannerState(
                    FirstTimeInterstitialBannerState::kSetupComplete) ==
                FirstTimeInterstitialBannerState::kSetupComplete);
  }
#endif
}

class SupervisedUserServiceTestUnsupervised
    : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTestUnsupervised()
      : SupervisedUserServiceTestBase(/*is_supervised=*/false) {}
};

// Tests that web approvals are not enabled for unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised, ApprovalRequestsDisabled) {
  ASSERT_FALSE(
      service_->remote_web_approvals_manager().AreApprovalRequestsEnabled());
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised, UrlIsAllowedForUser) {
  // Set "only allow certain sites" filter.
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(FilteringBehavior::kBlock));
  service_->GetURLFilter()->SetDefaultFilteringBehavior(
      FilteringBehavior::kBlock);

  ASSERT_FALSE(service_->IsBlockedURL(GURL("http://google.com")));
}

// TODO(crbug.com/1364589): Failing consistently on linux-chromeos-dbg
// due to failed timezone conversion assertion.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DeprecatedFilterPolicy DISABLED_DeprecatedFilterPolicy
#else
#define MAYBE_DeprecatedFilterPolicy DeprecatedFilterPolicy
#endif
TEST_F(SupervisedUserServiceTest, MAYBE_DeprecatedFilterPolicy) {
  ASSERT_EQ(syncable_pref_service_.GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior),
            static_cast<int>(FilteringBehavior::kAllow));
  EXPECT_DCHECK_DEATH(syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      /* SupervisedUserURLFilter::WARN */ 1));
}

}  // namespace supervised_user
