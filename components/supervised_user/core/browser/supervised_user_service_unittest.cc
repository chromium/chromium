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
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {

const char kExampleUrl0[] = "http://www.example0.com";
const char kExampleUrl1[] = "http://www.example1.com/123";

}  // namespace

class FilterDelegateImpl : public SupervisedUserURLFilter::Delegate {
 public:
  std::string GetCountryCode() override { return std::string(); }
};

class SupervisedUserServiceTestBase : public ::testing::Test {
 public:
  explicit SupervisedUserServiceTestBase(bool is_supervised)
      : kids_chrome_management_client_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            identity_test_env_.identity_manager()) {
    settings_service_.Init(syncable_pref_service_.user_prefs_store());
    SupervisedUserService::RegisterProfilePrefs(
        syncable_pref_service_.registry());
    if (is_supervised) {
      syncable_pref_service_.SetString(prefs::kSupervisedUserId,
                                       kChildAccountSUID);
    } else {
      syncable_pref_service_.ClearPref(prefs::kSupervisedUserId);
    }

    service_ = std::make_unique<SupervisedUserService>(
        identity_test_env_.identity_manager(), &kids_chrome_management_client_,
        syncable_pref_service_, settings_service_, sync_service_,
        /*check_webstore_url_callback=*/
        base::BindRepeating([](const GURL& url) { return false; }),
        std::make_unique<FilterDelegateImpl>(),
        /*can_show_first_time_interstitial_banner=*/false);

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
  KidsChromeManagementClient kids_chrome_management_client_;

  std::unique_ptr<SupervisedUserService> service_;
};

class SupervisedUserServiceTest : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTest()
      : SupervisedUserServiceTestBase(/*is_supervised=*/true) {}
};

TEST_F(SupervisedUserServiceTest, IsURLFilteringEnabled) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(service_->IsURLFilteringEnabled());
#else
  ASSERT_FALSE(service_->IsURLFilteringEnabled());
#endif

  // Enable filtering flag across platforms.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);

  EXPECT_TRUE(service_->IsURLFilteringEnabled());
}

TEST_F(SupervisedUserServiceTest,
       AreExtensionsPermissionsEnabledWithExtensionsPermissionsFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(service_->AreExtensionsPermissionsEnabled());
#else
  EXPECT_FALSE(service_->AreExtensionsPermissionsEnabled());
#endif
}

TEST_F(SupervisedUserServiceTest,
       AreExtensionsPermissionsEnabledWithExtensionsPermissionsFlagEnabled) {
  base::test::ScopedFeatureList feature_list(
      kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(service_->AreExtensionsPermissionsEnabled());
#else
  EXPECT_FALSE(service_->AreExtensionsPermissionsEnabled());
#endif
}

TEST_F(SupervisedUserServiceTest, ManagedSiteListTypeMetricOnPrefsChange) {
  base::HistogramTester histogram_tester;

  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered by policies change. Since
  // the default values are the same of override values, the WebFilterType
  // doesn't change and no report here.
  syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      SupervisedUserURLFilter::ALLOW);
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
TEST_F(SupervisedUserServiceTest,
       CookieDeletionDisabledForYoutubeDomainsWhenClearingCookiesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kClearingCookiesKeepsSupervisedUsersSignedIn);

  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_TRUE(service_->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_TRUE(service_->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

TEST_F(SupervisedUserServiceTest,
       CookieDeletionAllowedForYoutubeDomainsWhenClearingCookiesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kClearingCookiesKeepsSupervisedUsersSignedIn);

  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

class SupervisedUserServiceTestUnsupervised
    : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTestUnsupervised()
      : SupervisedUserServiceTestBase(/*is_supervised=*/false) {}
};

TEST_F(SupervisedUserServiceTestUnsupervised, IsURLFilteringEnabled) {
  ASSERT_FALSE(service_->IsURLFilteringEnabled());

  // Enable filtering flag across platforms.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      kFilterWebsitesForSupervisedUsersOnDesktopAndIOS));

  EXPECT_FALSE(service_->IsURLFilteringEnabled());
}

TEST_F(SupervisedUserServiceTestUnsupervised, AreExtensionsPermissionsEnabled) {
  EXPECT_FALSE(service_->AreExtensionsPermissionsEnabled());
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
            SupervisedUserURLFilter::ALLOW);
  EXPECT_DCHECK_DEATH(syncable_pref_service_.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      /* SupervisedUserURLFilter::WARN */ 1));
}

TEST_F(SupervisedUserServiceTestUnsupervised,
       CookieDeletionAllowedForYoutubeDomainsWhenClearingCookiesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kClearingCookiesKeepsSupervisedUsersSignedIn);

  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

TEST_F(SupervisedUserServiceTestUnsupervised,
       CookieDeletionAllowedForYoutubeDomainsWhenClearingCookiesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kClearingCookiesKeepsSupervisedUsersSignedIn);

  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://google.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://example.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("http://youtube.com")));
  EXPECT_FALSE(service_->IsCookieDeletionDisabled(GURL("https://youtube.com")));
}

}  // namespace supervised_user
