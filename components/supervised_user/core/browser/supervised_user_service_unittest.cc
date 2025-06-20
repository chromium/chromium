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
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";
constexpr char kApprovedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Approved";
constexpr char kBlockedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Blocked";

const char kExampleUrl0[] = "http://www.example0.com";
const char kExampleUrl1[] = "http://www.example1.com/123";

class SupervisedUserMetricsServiceExtensionDelegateFake
    : public SupervisedUserMetricsService::
          SupervisedUserMetricsServiceExtensionDelegate {
  bool RecordExtensionsMetrics() override { return false; }
};

class SupervisedUserServiceTestBase : public ::testing::Test {
 public:
  explicit SupervisedUserServiceTestBase(bool is_supervised) {
    if (is_supervised) {
      supervised_user_test_environment_.pref_service()->SetString(
          prefs::kSupervisedUserId, kChildAccountSUID);
    } else {
      supervised_user_test_environment_.pref_service()->ClearPref(
          prefs::kSupervisedUserId);
    }
  }

  void TearDown() override { supervised_user_test_environment_.Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
};

class SupervisedUserServiceTest : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTest()
      : SupervisedUserServiceTestBase(/*is_supervised=*/true) {}
};

// Tests that web approvals are enabled for supervised users.
TEST_F(SupervisedUserServiceTest, ApprovalRequestsEnabled) {
  EXPECT_TRUE(supervised_user_test_environment_.service()
                  ->remote_web_approvals_manager()
                  .AreApprovalRequestsEnabled());
}

// Tests that restricting all site navigation is applied to supervised users.
TEST_F(SupervisedUserServiceTest, UrlIsBlockedForUser) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  EXPECT_TRUE(supervised_user_test_environment_.url_filter()
                  ->GetFilteringBehavior(GURL("http://google.com"))
                  .IsBlocked());
}

// Tests that allowing all site navigation is applied to supervised users.
TEST_F(SupervisedUserServiceTest, UrlIsAllowedForUser) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);
  EXPECT_TRUE(supervised_user_test_environment_.url_filter()
                  ->GetFilteringBehavior(GURL("http://google.com"))
                  .IsAllowed());
}

// Tests that changes in parent configuration for web filter types are recorded.
TEST_F(SupervisedUserServiceTest, WebFilterTypeOnPrefsChange) {
  base::HistogramTester histogram_tester;

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kTryToBlockMatureSites);
  histogram_tester.ExpectUniqueSample(kWebFilterTypeHistogramName,
                                      /*sample=*/
                                      WebFilterType::kTryToBlockMatureSites,
                                      /*expected_bucket_count=*/0);

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);
  histogram_tester.ExpectBucketCount(kWebFilterTypeHistogramName,
                                     /*sample=*/
                                     WebFilterType::kAllowAllSites,
                                     /*expected_count=*/1);

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  histogram_tester.ExpectBucketCount(kWebFilterTypeHistogramName,
                                     /*sample=*/
                                     WebFilterType::kCertainSites,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(kWebFilterTypeHistogramName,
                                    /*expected_count=*/2);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Death tests tend to be flaky on Android or ChromeOS.
TEST_F(SupervisedUserServiceTest, CantEnableFilteringUsingUserControls) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH_IF_SUPPORTED(
      EnableBrowserContentFilters(
          *supervised_user_test_environment_.pref_service()),
      "Users who are subject to Family Link parental controls cannot change "
      "browser content filters");
}
#endif

// Tests that changes to the allow or blocklist of the parent configuration are
// recorded.
TEST_F(SupervisedUserServiceTest, ManagedSiteListTypeMetricOnPrefsChange) {
  base::HistogramTester histogram_tester;

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);
  // Blocks `kExampleUrl0`.
  supervised_user_test_environment_.SetManualFilterForHost(kExampleUrl0, false);

  histogram_tester.ExpectBucketCount(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBlockedListOnly,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                     /*sample=*/0, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                     /*sample=*/1, /*expected_count=*/1);

  // Approves `kExampleUrl0`.
  supervised_user_test_environment_.SetManualFilterForHost(kExampleUrl0, true);

  histogram_tester.ExpectBucketCount(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kApprovedListOnly,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                     /*sample=*/1, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                     /*sample=*/0, /*expected_count=*/1);

  // Blocks `kExampleURL1`.
  supervised_user_test_environment_.SetManualFilterForHost(kExampleUrl1, false);

  histogram_tester.ExpectBucketCount(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBoth,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                     /*sample=*/1, /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                     /*sample=*/1, /*expected_count=*/2);

  histogram_tester.ExpectTotalCount(kManagedSiteListHistogramName,
                                    /*expected_count=*/3);
  histogram_tester.ExpectTotalCount(kApprovedSitesCountHistogramName,
                                    /*expected_count=*/3);
  histogram_tester.ExpectTotalCount(kBlockedSitesCountHistogramName,
                                    /*expected_count=*/3);
}

class SupervisedUserServiceTestUnsupervised
    : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTestUnsupervised()
      : SupervisedUserServiceTestBase(/*is_supervised=*/false) {}
};

// Tests that web approvals are not enabled for unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised, ApprovalRequestsDisabled) {
  EXPECT_FALSE(supervised_user_test_environment_.service()
                   ->remote_web_approvals_manager()
                   .AreApprovalRequestsEnabled());
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised,
       CantRequestUrlClassificationBlocking) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  EXPECT_EQ(static_cast<int>(FilteringBehavior::kAllow),
            supervised_user_test_environment_.pref_service()->GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior))
      << "Check why supervised user service received "
         "WebFilterType::kCertainSites change (FilteringBehavior::kBlock)";

  EXPECT_FALSE(supervised_user_test_environment_.service()->IsBlockedURL(
      GURL("http://google.com")));
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised,
       CantRequestTryToFilterClassificationViaFamilyLink) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kTryToBlockMatureSites);
  EXPECT_EQ(static_cast<int>(FilteringBehavior::kAllow),
            supervised_user_test_environment_.pref_service()->GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior));
  EXPECT_FALSE(supervised_user_test_environment_.pref_service()->GetBoolean(
      prefs::kSupervisedUserSafeSites));

  EXPECT_FALSE(supervised_user_test_environment_.service()->IsBlockedURL(
      GURL("http://google.com")));
}

// This checks verifies whether single profile can cycle through the all types
// of supervision.
TEST_F(SupervisedUserServiceTestUnsupervised, CyclesThroughFilteringSettings) {
  ASSERT_EQ(WebFilterType::kDisabled,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  // Browser content filtering is functionally equivalent to
  // WebFilterType::kTryToBlockMatureSites with empty manual allow and
  // blocklists.
  EnableBrowserContentFilters(
      *supervised_user_test_environment_.pref_service());
  EXPECT_EQ(WebFilterType::kTryToBlockMatureSites,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  DisableBrowserContentFilters(
      *supervised_user_test_environment_.pref_service());
  EXPECT_EQ(WebFilterType::kDisabled,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  // "Try to block mature sites" is the default setting for child accounts
  // (profiles supervised by the Family Link).
  EnableParentalControls(*supervised_user_test_environment_.pref_service());
  EXPECT_EQ(WebFilterType::kTryToBlockMatureSites,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  // Once Family Link parental controls are enabled, more settings are
  // available:
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);
  EXPECT_EQ(WebFilterType::kAllowAllSites,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  EXPECT_EQ(WebFilterType::kCertainSites,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kTryToBlockMatureSites);
  EXPECT_EQ(WebFilterType::kTryToBlockMatureSites,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());

  // Finally, turn all controls and bring back to defaults.
  DisableParentalControls(*supervised_user_test_environment_.pref_service());
  EXPECT_EQ(WebFilterType::kDisabled,
            supervised_user_test_environment_.url_filter()->GetWebFilterType());
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised, UrlIsAllowedForUser) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  EXPECT_FALSE(supervised_user_test_environment_.service()->IsBlockedURL(
      GURL("http://google.com")));
}

// TODO(crbug.com/1364589): Failing consistently on linux-chromeos-dbg
// due to failed timezone conversion assertion.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DeprecatedFilterPolicy DISABLED_DeprecatedFilterPolicy
#else
#define MAYBE_DeprecatedFilterPolicy DeprecatedFilterPolicy
#endif
TEST_F(SupervisedUserServiceTest, MAYBE_DeprecatedFilterPolicy) {
  ASSERT_EQ(supervised_user_test_environment_.pref_service()->GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior),
            static_cast<int>(FilteringBehavior::kAllow));
  EXPECT_DCHECK_DEATH(
      supervised_user_test_environment_.pref_service_syncable()
          ->SetSupervisedUserPref(
              prefs::kDefaultSupervisedUserFilteringBehavior,
              /* SupervisedUserURLFilter::WARN */ base::Value(1)));
}
}  // namespace
}  // namespace supervised_user
