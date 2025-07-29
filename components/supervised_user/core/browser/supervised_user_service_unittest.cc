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

class SupervisedUserServiceTest : public ::testing::Test {
 protected:
  // Explicit environment initialization reduces the number of fixtures.
  void Initialize(InitialSupervisionState initial_state) {
    supervised_user_test_environment_ =
        std::make_unique<SupervisedUserTestEnvironment>(initial_state);
  }

  void TearDown() override { supervised_user_test_environment_->Shutdown(); }

  void SetWebFilterType(WebFilterType web_filter_type) {
    supervised_user_test_environment_->SetWebFilterType(web_filter_type);
  }

#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list_{
      kPropagateDeviceContentFiltersToSupervisedUser};
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SupervisedUserTestEnvironment>
      supervised_user_test_environment_;
};

// Tests that web approvals are enabled for supervised users.
TEST_F(SupervisedUserServiceTest, ApprovalRequestsEnabled) {
  Initialize(InitialSupervisionState::kFamilyLinkDefault);
  EXPECT_TRUE(supervised_user_test_environment_->service()
                  ->remote_web_approvals_manager()
                  .AreApprovalRequestsEnabled());
}

// Tests that restricting all site navigation is applied to supervised users.
TEST_F(SupervisedUserServiceTest, UrlIsBlockedForUser) {
  Initialize(InitialSupervisionState::kFamilyLinkCertainSites);
  EXPECT_TRUE(supervised_user_test_environment_->url_filter()
                  ->GetFilteringBehavior(GURL("http://google.com"))
                  .IsBlocked());
}

// Tests that allowing all site navigation is applied to supervised users.
TEST_F(SupervisedUserServiceTest, UrlIsAllowedForUser) {
  Initialize(InitialSupervisionState::kFamilyLinkAllowAllSites);
  EXPECT_TRUE(supervised_user_test_environment_->url_filter()
                  ->GetFilteringBehavior(GURL("http://google.com"))
                  .IsAllowed());
}

// Tests that changes to the allow or blocklist of the parent configuration are
// recorded.
TEST_F(SupervisedUserServiceTest, ManagedSiteListTypeMetricOnPrefsChange) {
  Initialize(InitialSupervisionState::kFamilyLinkDefault);
  // Check post-initialization counts of FamilyUser metrics.
  histogram_tester_.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                      /*sample=*/0, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                      /*sample=*/0, /*expected_count=*/1);

  SetWebFilterType(WebFilterType::kAllowAllSites);
  // Blocks `kExampleUrl0`.
  supervised_user_test_environment_->SetManualFilterForHost(kExampleUrl0,
                                                            false);

  histogram_tester_.ExpectBucketCount(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBlockedListOnly,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                      /*sample=*/0, /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                      /*sample=*/1, /*expected_count=*/1);

  // Unblocks and approves `kExampleUrl0`.
  supervised_user_test_environment_->SetManualFilterForHost(kExampleUrl0, true);
  histogram_tester_.ExpectBucketCount(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kApprovedListOnly,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                      /*sample=*/1, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                      /*sample=*/0, /*expected_count=*/2);

  // Blocks `kExampleURL1`.
  ASSERT_NE(kExampleUrl0, kExampleUrl1);
  supervised_user_test_environment_->SetManualFilterForHost(kExampleUrl1,
                                                            false);

  histogram_tester_.ExpectBucketCount(
      kManagedSiteListHistogramName,
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBoth,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kApprovedSitesCountHistogramName,
                                      /*sample=*/1, /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(kBlockedSitesCountHistogramName,
                                      /*sample=*/1, /*expected_count=*/2);

  // 4 transitions: initial, 2 * kExampleUrl0 and 1 * kExampleUrl1.
  histogram_tester_.ExpectTotalCount(kManagedSiteListHistogramName,
                                     /*expected_count=*/4);
  histogram_tester_.ExpectTotalCount(kApprovedSitesCountHistogramName,
                                     /*expected_count=*/4);
  histogram_tester_.ExpectTotalCount(kBlockedSitesCountHistogramName,
                                     /*expected_count=*/4);
}

class SupervisedUserServiceTestUnsupervised : public SupervisedUserServiceTest {
 public:
  SupervisedUserServiceTestUnsupervised() {
    Initialize(InitialSupervisionState::kUnsupervised);
  }
};

// Tests that web approvals are not enabled for unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised, ApprovalRequestsDisabled) {
  EXPECT_FALSE(supervised_user_test_environment_->service()
                   ->remote_web_approvals_manager()
                   .AreApprovalRequestsEnabled());
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised,
       CantRequestUrlClassificationBlocking) {
  SetWebFilterType(WebFilterType::kCertainSites);

  EXPECT_TRUE(
      supervised_user_test_environment_->pref_service()
          ->FindPreference(prefs::kDefaultSupervisedUserFilteringBehavior)
          ->IsDefaultValue())
      << "With supervision disabled, the pref should be"
         " reset to default.";
  EXPECT_EQ(static_cast<int>(FilteringBehavior::kAllow),
            supervised_user_test_environment_->pref_service()->GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior));

  EXPECT_FALSE(supervised_user_test_environment_->service()->IsBlockedURL(
      GURL("http://google.com")));
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised,
       CantRequestTryToFilterClassificationViaFamilyLink) {
  SetWebFilterType(WebFilterType::kTryToBlockMatureSites);

  EXPECT_TRUE(supervised_user_test_environment_->pref_service()
                  ->FindPreference(prefs::kSupervisedUserSafeSites)
                  ->IsDefaultValue())
      << "With supervision disabled, the pref should be "
         "reset to default.";
  EXPECT_FALSE(supervised_user_test_environment_->pref_service()->GetBoolean(
      prefs::kSupervisedUserSafeSites));

  EXPECT_FALSE(supervised_user_test_environment_->service()->IsBlockedURL(
      GURL("http://google.com")));
}

// Tests that supervision restrictions do not apply to unsupervised users.
TEST_F(SupervisedUserServiceTestUnsupervised, UrlIsAllowedForUser) {
  SetWebFilterType(WebFilterType::kCertainSites);
  EXPECT_FALSE(supervised_user_test_environment_->service()->IsBlockedURL(
      GURL("http://google.com")));
}

// This test suite verifies how web filter type changes are propagated from this
// service to the metrics service.
class SupervisedUserServiceWebFilterTypeTransitionsTest
    : public SupervisedUserServiceTest {
 protected:
  WebFilterType GetWebFilterType() {
    return supervised_user_test_environment_->service()
        ->GetURLFilter()
        ->GetWebFilterType();
  }
};

// Transitions between Family Link supervision states. There is 1 unsupervised
// state and 3 states of web filter type for Family Link. Each state can
// transition to any other state. "FamilyUser.WebFilterType" is a legacy
// histogram but is still asserted.
class SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest
    : public SupervisedUserServiceWebFilterTypeTransitionsTest {
 protected:
  void EnableParentalControls() {
    ::supervised_user::EnableParentalControls(
        *supervised_user_test_environment_->pref_service_syncable());
  }

  void DisableParentalControls() {
    ::supervised_user::DisableParentalControls(
        *supervised_user_test_environment_->pref_service_syncable());
  }
};

TEST_F(SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest,
       FromUnsupervisedToSupervisedWithAllowAllSites) {
  // Set initial state to unsupervised. Web filter type is dormant.
  Initialize(InitialSupervisionState::kUnsupervised);
  SetWebFilterType(WebFilterType::kAllowAllSites);

  // Filter is disabled, but unsupervised users don't report the metric.
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     0);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 0);

  EnableParentalControls();
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kAllowAllSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kAllowAllSites,
      1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kAllowAllSites, 1);

  // Disable parental controls. No more metrics are emitted.
  DisableParentalControls();
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     1);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 1);
}

TEST_F(SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest,
       FromUnsupervisedToSupervisedWithCertainSites) {
  // Set initial state to unsupervised. Web filter type is dormant.
  Initialize(InitialSupervisionState::kUnsupervised);
  SetWebFilterType(WebFilterType::kCertainSites);

  // Filter is disabled, but unsupervised users don't report the metric.
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     0);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 0);

  EnableParentalControls();
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kCertainSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kCertainSites,
      1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kCertainSites, 1);

  // Disable parental controls. No more metrics are emitted.
  DisableParentalControls();
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     1);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 1);
}

TEST_F(SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest,
       FromUnsupervisedToSupervised) {
  // Set initial state to unsupervised.
  Initialize(InitialSupervisionState::kUnsupervised);

  // Filter is disabled, but unsupervised users don't report the metric.
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     0);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 0);

  EnableParentalControls();
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kTryToBlockMatureSites, 1);

  // Disable parental controls. No more metrics are emitted.
  DisableParentalControls();
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     1);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 1);
}

TEST_F(SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest,
       FromTryToBlockMatureSitesToAllowAllSites) {
  // Set initial state to supervised with try to block mature sites.
  Initialize(InitialSupervisionState::kFamilyLinkTryToBlockMatureSites);

  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kTryToBlockMatureSites, 1);

  SetWebFilterType(WebFilterType::kAllowAllSites);
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kAllowAllSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kAllowAllSites,
      1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kAllowAllSites, 1);

  SetWebFilterType(WebFilterType::kTryToBlockMatureSites);
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 2);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kTryToBlockMatureSites, 2);

  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     3);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 3);
}

TEST_F(SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest,
       FromAllowAllSitesToCertainSites) {
  // Set initial state to supervised with try to block mature sites.
  Initialize(InitialSupervisionState::kFamilyLinkAllowAllSites);

  EXPECT_EQ(GetWebFilterType(), WebFilterType::kAllowAllSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kAllowAllSites,
      1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kAllowAllSites, 1);

  SetWebFilterType(WebFilterType::kCertainSites);
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kCertainSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kCertainSites,
      1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kCertainSites, 1);

  SetWebFilterType(WebFilterType::kAllowAllSites);
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kAllowAllSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kAllowAllSites,
      2);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kAllowAllSites, 2);

  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     3);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 3);
}

TEST_F(SupervisedUserServiceFamilyLinkWebFilterTypeTransitionsTest,
       FromTryToBlockMatureSitesToCertainSites) {
  // Set initial state to supervised with try to block mature sites.
  Initialize(InitialSupervisionState::kFamilyLinkTryToBlockMatureSites);

  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kTryToBlockMatureSites, 1);

  SetWebFilterType(WebFilterType::kCertainSites);
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kCertainSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink", WebFilterType::kCertainSites,
      1);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kCertainSites, 1);

  SetWebFilterType(WebFilterType::kTryToBlockMatureSites);
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 2);
  histogram_tester_.ExpectBucketCount("FamilyUser.WebFilterType",
                                      WebFilterType::kTryToBlockMatureSites, 2);

  histogram_tester_.ExpectTotalCount("SupervisedUsers.WebFilterType.FamilyLink",
                                     3);
  histogram_tester_.ExpectTotalCount("FamilyUser.WebFilterType", 3);
}

#if BUILDFLAG(IS_ANDROID)
// Transitions between local supervision states. There are two boolean filters,
// so there are 4 states in total. Transitions that change only one filter at a
// time are possible, both ways.
class SupervisedUserServiceLocallySupervisedWebFilterTypeTransitionsTest
    : public SupervisedUserServiceWebFilterTypeTransitionsTest {
 protected:
  void SetBrowserFilterEnabled(bool enabled) {
    supervised_user_test_environment_->browser_content_filters_observer()
        ->SetEnabled(enabled);
  }
  void SetSearchFilterEnabled(bool enabled) {
    supervised_user_test_environment_->search_content_filters_observer()
        ->SetEnabled(enabled);
  }
  bool IsSupervisedLocally() const {
    return supervised_user_test_environment_->service()->IsSupervisedLocally();
  }
};

// All enabled -> only browser filter enabled -> all disabled -> only search
// filter enabled.
TEST_F(SupervisedUserServiceLocallySupervisedWebFilterTypeTransitionsTest,
       AllToBrowserToNoneToSearchToAll) {
  Initialize(InitialSupervisionState::kSupervisedWithAllContentFilters);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.LocallySupervised",
      WebFilterType::kTryToBlockMatureSites, 1);

  // Leaves only browser filter enabled - no change in web filter type.
  SetSearchFilterEnabled(false);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 1);

  // All filters disabled. Disabling supervision won't yield WebFilterType
  // metric.
  SetBrowserFilterEnabled(false);
  EXPECT_FALSE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 1);

  // Supervision is back on, but the browser filter is still disabled. This time
  // WebFilterType metric is emitted to indicate disabled filter setting.
  SetSearchFilterEnabled(true);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.LocallySupervised",
      WebFilterType::kDisabled, 1);

  // Back to where we started: both filters enabled.
  SetBrowserFilterEnabled(true);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.LocallySupervised",
      WebFilterType::kTryToBlockMatureSites, 2);

  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 3);
}

TEST_F(SupervisedUserServiceLocallySupervisedWebFilterTypeTransitionsTest,
       NoneToBrowserToAllToSearchToNone) {
  Initialize(InitialSupervisionState::kUnsupervised);
  EXPECT_FALSE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 0);

  SetBrowserFilterEnabled(true);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 1);

  // All filters enabled
  SetSearchFilterEnabled(true);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kTryToBlockMatureSites);
  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 1);

  // Leaves only the search filter enabled - disables browser filter.
  SetBrowserFilterEnabled(false);
  EXPECT_TRUE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectBucketCount(
      "SupervisedUsers.WebFilterType.LocallySupervised",
      WebFilterType::kDisabled, 1);

  // Back to where we started: unsupervised. Disabling supervision won't yield
  // WebFilterType metric.
  SetSearchFilterEnabled(false);
  EXPECT_FALSE(IsSupervisedLocally());
  EXPECT_EQ(GetWebFilterType(), WebFilterType::kDisabled);
  histogram_tester_.ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 2);
}

#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/1364589): Failing consistently on linux-chromeos-dbg
// due to failed timezone conversion assertion.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DeprecatedFilterPolicy DISABLED_DeprecatedFilterPolicy
#else
#define MAYBE_DeprecatedFilterPolicy DeprecatedFilterPolicy
#endif
TEST_F(SupervisedUserServiceTest, MAYBE_DeprecatedFilterPolicy) {
  Initialize(InitialSupervisionState::kFamilyLinkDefault);
  ASSERT_EQ(supervised_user_test_environment_->pref_service()->GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior),
            static_cast<int>(FilteringBehavior::kAllow));
  EXPECT_DCHECK_DEATH(
      supervised_user_test_environment_->pref_service_syncable()
          ->SetSupervisedUserPref(
              prefs::kDefaultSupervisedUserFilteringBehavior,
              /* SupervisedUserURLFilter::WARN */ base::Value(1)));
}
}  // namespace
}  // namespace supervised_user
