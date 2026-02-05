// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_split.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

// Base unit for tests. The features are implemented in SupervisedUserUrlFilter,
// but since it's about to become one of the delegates of
// SupervisedUserUrlFilteringService, we want to test the integration of all
// those features together before landing the migration (test-driven approach).
class FamilyLinkUrlFilterManualBehaviorTestBase : public ::testing::Test {
 protected:
  ~FamilyLinkUrlFilterManualBehaviorTestBase() override {
    supervised_user_test_environment_.Shutdown();
  }

  FamilyLinkUrlFilter* under_test() {
    return supervised_user_test_environment_.family_link_url_filter();
  }

  SupervisedUserTestEnvironment& test_env() {
    return supervised_user_test_environment_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
  base::HistogramTester histogram_tester_;
};

// Test cases only parametrized by kSupervisedUserUseUrlFilteringService
// feature.
class FamilyLinkUrlFilterManualBehaviorTest
    : public base::test::WithFeatureOverride,
      public FamilyLinkUrlFilterManualBehaviorTestBase {
 protected:
  FamilyLinkUrlFilterManualBehaviorTest()
      : base::test::WithFeatureOverride(kSupervisedUserUseUrlFilteringService),
        FamilyLinkUrlFilterManualBehaviorTestBase() {}
};

TEST_P(FamilyLinkUrlFilterManualBehaviorTest,
       DisabledParentalControlsDontBlockUrls) {
  // All sites blocked by default.
  EnableParentalControls(*test_env().pref_service());
  test_env().SetWebFilterType(WebFilterType::kCertainSites);
  EXPECT_TRUE(IsSubjectToParentalControls(*test_env().pref_service()));
  EXPECT_TRUE(under_test()
                  ->GetFilteringBehavior(GURL("http://example.com"))
                  .IsBlocked());

  DisableParentalControls(*test_env().pref_service());
  EXPECT_FALSE(IsSubjectToParentalControls(*test_env().pref_service()));
  EXPECT_FALSE(under_test()
                   ->GetFilteringBehavior(GURL("http://example.com"))
                   .IsBlocked());
}

// Tests that allowing all site navigation is applied to supervised users.
TEST_P(FamilyLinkUrlFilterManualBehaviorTest, AllowAllSitesDoesntBlockUrls) {
  EnableParentalControls(*test_env().pref_service());
  test_env().SetWebFilterType(WebFilterType::kAllowAllSites);
  EXPECT_TRUE(under_test()
                  ->GetFilteringBehavior(GURL("http://example.com"))
                  .IsAllowed());
}

TEST_P(FamilyLinkUrlFilterManualBehaviorTest, UnrelatedHostExceptionIsIgnored) {
  EnableParentalControls(*test_env().pref_service());
  test_env().SetManualFilterForHost("google.com", /*allow=*/false);
  test_env().SetWebFilterType(WebFilterType::kAllowAllSites);
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("https://www.example.com"))
                .behavior,
            FilteringBehavior::kAllow);
}

TEST_P(FamilyLinkUrlFilterManualBehaviorTest, Canonicalization) {
  EnableParentalControls(*test_env().pref_service());
  // We assume that the hosts and URLs are already canonicalized.
  test_env().SetManualFilterForHost("www.moose.org", true);
  test_env().SetManualFilterForHost("www.xn--n3h.net", true);
  test_env().SetManualFilterForUrl("http://www.example.com/foo/", true);
  test_env().SetManualFilterForUrl(
      "http://www.example.com/%C3%85t%C3%B8mstr%C3%B6m", true);
  test_env().SetWebFilterType(WebFilterType::kCertainSites);

  // Base cases.
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("http://www.example.com/foo/"))
                .behavior,
            FilteringBehavior::kAllow);
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(
                    GURL("http://www.example.com/%C3%85t%C3%B8mstr%C3%B6m"))
                .behavior,
            FilteringBehavior::kAllow);

  // Verify that non-URI characters are escaped.
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL(
                    "http://www.example.com/\xc3\x85t\xc3\xb8mstr\xc3\xb6m"))
                .behavior,
            FilteringBehavior::kAllow);

  // Verify that unnecessary URI escapes remain escaped.
  EXPECT_EQ(
      under_test()
          ->GetFilteringBehavior(GURL("http://www.example.com/%66%6F%6F/"))
          .behavior,
      FilteringBehavior::kBlock);

  // Verify that the default port are removed.
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("http://www.example.com:80/foo/"))
                .behavior,
            FilteringBehavior::kAllow);

  // Verify that scheme and hostname are lower cased.
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("htTp://wWw.eXamPle.com/foo/"))
                .behavior,
            FilteringBehavior::kAllow);
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("HttP://WwW.mOOsE.orG/blurp/"))
                .behavior,
            FilteringBehavior::kAllow);

  // Verify that UTF-8 in hostnames are converted to punycode.
  EXPECT_EQ(
      under_test()
          ->GetFilteringBehavior(GURL("http://www.\xe2\x98\x83\x0a.net/bla/"))
          .behavior,
      FilteringBehavior::kAllow);

  // Verify that query and ref are stripped.
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(
                    GURL("http://www.example.com/foo/?bar=baz#ref"))
                .behavior,
            FilteringBehavior::kAllow);
}

// Tests that conflict tracking histogram records a result for no conflicts
// even for paths that determine a result and exit early.
TEST_P(FamilyLinkUrlFilterManualBehaviorTest,
       PatternWithoutConflictOnEarlyExit) {
  // The host map is empty but the url map contains an exact match.
  EnableParentalControls(*test_env().pref_service());
  test_env().SetManualFilterForUrl("https://www.google.com", true);
  test_env().SetWebFilterType(WebFilterType::kCertainSites);

  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("https://www.google.com"))
                .behavior,
            FilteringBehavior::kAllow);

  // When there is no conflict, no entries as recorded in the conflict type
  // histogram. A non-conflict entry is recorded on the conflict tracking
  // histogram.
  histogram_tester().ExpectTotalCount(
      FamilyLinkUrlFilter::GetManagedSiteListConflictTypeHistogramNameForTest(),
      /*expected_count=*/0);
  histogram_tester().ExpectBucketCount(
      FamilyLinkUrlFilter::GetManagedSiteListConflictHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(FamilyLinkUrlFilterManualBehaviorTest);

struct CertainSitesTestCase {
  std::string test_name;
  // Tested url.
  std::string under_test_url;
  std::optional<std::string> allowed_host_exception;
  FilteringBehavior expected_behavior;
};

// Test cases where manual behavior only allows listed hosts.
class FamilyLinkUrlFilterManualBehaviorCertainSitesTest
    : public WithFeatureOverrideAndParamInterface<CertainSitesTestCase>,
      public FamilyLinkUrlFilterManualBehaviorTestBase {
 protected:
  FamilyLinkUrlFilterManualBehaviorCertainSitesTest()
      : WithFeatureOverrideAndParamInterface(
            kSupervisedUserUseUrlFilteringService),
        FamilyLinkUrlFilterManualBehaviorTestBase() {}
};

TEST_P(FamilyLinkUrlFilterManualBehaviorCertainSitesTest, FilteringBehavior) {
  EnableParentalControls(*test_env().pref_service());
  test_env().SetWebFilterType(WebFilterType::kCertainSites);

  if (GetTestCase().allowed_host_exception.has_value()) {
    test_env().SetManualFilterForHost(
        GetTestCase().allowed_host_exception.value(), true);
  }

  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL(GetTestCase().under_test_url))
                .behavior,
            GetTestCase().expected_behavior);
}

// Note: test cases must be unique:
const CertainSitesTestCase kCertainSitesTestCases[] = {
    // The domain matches directly the exception.
    {"MatchingDomain_1", "http://google.com", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingDomain_2", "http://google.com/", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingDomain_3", "http://google.com/whatever", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingDomain_4", "https://google.com/", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingDomain_5", "https://google.com", "*.google.com",
     FilteringBehavior::kAllow},

    // The subdomain matches the exception.
    {"MatchingSubdomain_1", "http://mail.google.com", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingSubdomain_2", "http://x.mail.google.com", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingSubdomain_3", "https://x.mail.google.com/", "*.google.com",
     FilteringBehavior::kAllow},
    {"MatchingSubdomain_4", "http://x.y.google.com/a/b", "*.google.com",
     FilteringBehavior::kAllow},

    // Non-standard schemes are allowed.
    {"NonStandardScheme_1", "bogus://youtube.com/", "*.google.com",
     FilteringBehavior::kAllow},
    {"NonStandardScheme_2", "chrome://youtube.com/", "*.google.com",
     FilteringBehavior::kAllow},
    {"NonStandardScheme_3", "chrome://extensions/", "*.google.com",
     FilteringBehavior::kAllow},
    {"NonStandardScheme_4", "chrome-extension://foo/main.html", "*.google.com",
     FilteringBehavior::kAllow},
    {"NonStandardScheme_5",
     "file:///home/chronos/user/MyFiles/Downloads/img.jpg", "*.google.com",
     FilteringBehavior::kAllow},
    {"NonStandardScheme_6", "file://example.com", std::nullopt,
     FilteringBehavior::kAllow},
    {"NonStandardScheme_7", "filesystem://80cols.com", std::nullopt,
     FilteringBehavior::kAllow},
    {"NonStandardScheme_8", "chrome://example.com", std::nullopt,
     FilteringBehavior::kAllow},
    {"NonStandardScheme_9", "wtf://example.com", std::nullopt,
     FilteringBehavior::kAllow},
    {"NonStandardScheme_10", "gopher://example.com", std::nullopt,
     FilteringBehavior::kAllow},

    // Standard schemes without exception are blocked.
    {"StandardSchemeWithoutException_1", "http://example.com", std::nullopt,
     FilteringBehavior::kBlock},
    {"StandardSchemeWithoutException_2", "https://example.com", std::nullopt,
     FilteringBehavior::kBlock},
    {"StandardSchemeWithoutException_3", "ftp://example.com", std::nullopt,
     FilteringBehavior::kBlock},
    {"StandardSchemeWithoutException_4", "ws://example.com", std::nullopt,
     FilteringBehavior::kBlock},
    {"StandardSchemeWithoutException_5", "wss://example.com", std::nullopt,
     FilteringBehavior::kBlock},

    // Unlisted hosts are blocked.
    {"UnlistedHost_1", "http://youtube.com/", "*.google.com",
     FilteringBehavior::kBlock},
    {"UnlistedHost_2", "http://notgoogle.com/", "*.google.com",
     FilteringBehavior::kBlock},

    // Family link and Google account URLs are always allowed.
    {"FamilyLinkAndGoogleAccountUrls_1", "https://families.google.com/",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_2", "https://families.google.com",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_3", "http://families.google.com/",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_4",
     "https://families.google.com/something", std::nullopt,
     FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_6", "https://myaccount.google.com/",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_7", "https://accounts.google.com/",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_8", "https://familylink.google.com/",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_9", "https://policies.google.com/",
     std::nullopt, FilteringBehavior::kAllow},
    {"FamilyLinkAndGoogleAccountUrls_10", "https://support.google.com/",
     std::nullopt, FilteringBehavior::kAllow},

    // Subdomains of family link and Google account URLs are not allowed.
    {"FamilyLinkAndGoogleAccountUrlsSubdomains_1",
     "https://subdomain.families.google.com/", std::nullopt,
     FilteringBehavior::kBlock},
    {"FamilyLinkAndGoogleAccountUrlsSubdomains_2",
     "https://subdomain.policies.google.com/", std::nullopt,
     FilteringBehavior::kBlock},
    {"FamilyLinkAndGoogleAccountUrlsSubdomains_3",
     "https://subdomain.support.google.com/", std::nullopt,
     FilteringBehavior::kBlock},
    {"FamilyLinkAndGoogleAccountUrlsSubdomains_4",
     "https://subdomain.accounts.google.com/", std::nullopt,
     FilteringBehavior::kBlock},

    // Sync dashboard URLs are always allowed.
    {"SyncDashboardUrls_1", "https://www.google.com/settings/chrome/sync",
     std::nullopt, FilteringBehavior::kAllow},
    {"SyncDashboardUrls_2", "https://www.google.com/settings/chrome/data",
     std::nullopt, FilteringBehavior::kAllow},
    {"SyncDashboardUrls_3",
     "https://www.google.com/settings/chrome/sync?hl=en-US", std::nullopt,
     FilteringBehavior::kAllow},
    {"SyncDashboardUrls_4", "https://chrome.google.com/sync?hl=en-US",
     std::nullopt, FilteringBehavior::kAllow},
    {"SyncDashboardUrls_5", "https://chrome.google.com/data?hl=en-US",
     std::nullopt, FilteringBehavior::kAllow},

    // Play ToS URLs are always allowed.
    {"PlayToSUrls_1", "https://play.google/play-terms", std::nullopt,
     FilteringBehavior::kAllow},
    {"PlayToSUrls_2", "https://play.google/play-terms/", std::nullopt,
     FilteringBehavior::kAllow},
    {"PlayToSUrls_3", "https://play.google/intl/pt-BR_pt/play-terms/",
     std::nullopt, FilteringBehavior::kAllow},
    {"PlayToSUrls_4", "https://play.google/play-terms/index.html", std::nullopt,
     FilteringBehavior::kAllow},

    // Other Urls to Play ToS are not allowed.
    {"BlockedPlayToSUrls_1", "https://play.google.com/about/play-terms/",
     std::nullopt, FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_2", "https://play.google.com/about/play-terms",
     std::nullopt, FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_3",
     "https://play.google.com/intl/pt-BR_pt/about/play-terms/", std::nullopt,
     FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_4",
     "https://play.google.com/about/play-terms/index.html", std::nullopt,
     FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_5", "https://play.google.com/play-terms/",
     std::nullopt, FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_6", "http://play.google.com/about/play-terms/",
     std::nullopt, FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_7", "https://subdomain.play.google/play-terms/",
     std::nullopt, FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_8",
     "https://subdomain.play.google.com/about/play-terms/", std::nullopt,
     FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_9", "https://play.google/", std::nullopt,
     FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_10", "https://play.google.com/", std::nullopt,
     FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_11", "https://play.google/about", std::nullopt,
     FilteringBehavior::kBlock},
    {"BlockedPlayToSUrls_12", "https://play.google.com/about", std::nullopt,
     FilteringBehavior::kBlock},

    // Urls that embed other urls that should be allowed (effectively the
    // embedded url is evaluated).
    {"EmbeddedUrls_1", "http://example.com", "example.com",
     FilteringBehavior::kAllow},
    {"EmbeddedUrls_2", "https://example.com", "example.com",
     FilteringBehavior::kAllow},
    {"EmbeddedUrls_3", "https://cdn.ampproject.org/c/example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_4", "https://cdn.ampproject.org/c/www.example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_5", "https://cdn.ampproject.org/c/example.com/path",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_6", "https://cdn.ampproject.org/c/s/example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_7", "https://sub.cdn.ampproject.org/c/example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_8", "https://sub.cdn.ampproject.org/c/www.example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_9", "https://sub.cdn.ampproject.org/c/example.com/path",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_10", "https://sub.cdn.ampproject.org/c/s/example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_11", "https://www.google.com/amp/example.com", "example.com",
     FilteringBehavior::kAllow},
    {"EmbeddedUrls_12", "https://www.google.com/amp/www.example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_13", "https://www.google.com/amp/s/example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_14", "https://www.google.com/amp/s/example.com/path",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_15",
     "https://webcache.googleusercontent.com/search?q=cache:example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_16",
     "https://webcache.googleusercontent.com/"
     "search?q=cache:example.com+search_query",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_17", "https://translate.google.com/translate?u=example.com",
     "example.com", FilteringBehavior::kAllow},
    {"EmbeddedUrls_18",
     "https://translate.google.com/translate?u=www.example.com", "example.com",
     FilteringBehavior::kAllow},
    {"EmbeddedUrls_19",
     "https://translate.google.com/translate?u=https://example.com",
     "example.com", FilteringBehavior::kAllow},

    // Urls that embed other urls that should be blocked (effectively the
    // embedded url is evaluated) - because the manual exception does not match
    // the embedded url the right way.
    {"EmbeddedUrlsBlocked_1", "https://cdn.ampproject.org/c/other.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_2", "https://cdn.ampproject.org", "example.com",
     FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_3", "https://sub.cdn.ampproject.org", "example.com",
     FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_4", "https://sub.cdn.ampproject.org/c/other.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_5", "https://www.google.com", "example.com",
     FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_6", "https://www.google.com/amp/", "example.com",
     FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_7", "https://www.google.com/amp/other.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_8", "https://webcache.googleusercontent.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_9", "https://webcache.googleusercontent.com/search",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_10",
     "https://webcache.googleusercontent.com/search?q=example.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_11",
     "https://webcache.googleusercontent.com/search?q=cache:other.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_12",
     "https://webcache.googleusercontent.com/"
     "search?q=cache:other.com+example.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_13",
     "https://webcache.googleusercontent.com/"
     "search?q=cache:123456789-01:other.com+example.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_14", "https://translate.google.com", "example.com",
     FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_15", "https://translate.googleusercontent.com",
     "example.com", FilteringBehavior::kBlock},
    {"EmbeddedUrlsBlocked_16",
     "https://translate.google.com/translate?u=other.com", "example.com",
     FilteringBehavior::kBlock},

    // IP addresses
    {"IPAddress_1", "http://123.123.123.123/", "123.123.123.123",
     FilteringBehavior::kAllow},
    {"IPAddress_2", "http://123.123.123.123", std::nullopt,
     FilteringBehavior::kBlock},
    {"IPAddress_3", "http://123.123.123.124/", "123.123.123.123",
     FilteringBehavior::kBlock},
};  // namespace

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyLinkUrlFilterManualBehaviorCertainSitesTest,
    testing::Combine(
        /*kSupervisedUserUseUrlFilteringService=*/testing::Bool(),
        testing::ValuesIn(kCertainSitesTestCases)),
    [](const auto& info) {
      bool is_feature_enabled = std::get<0>(info.param);
      return std::get<1>(info.param).test_name + "_With" +
             kSupervisedUserUseUrlFilteringService.name +
             (is_feature_enabled ? "Enabled" : "Disabled");
    });

struct HostConflictsTestCase {
  std::string test_name;
  // Tested url.
  std::string under_test_url;
  // Define both allowed and blocked hosts to see how conflicts are handled.
  std::vector<std::string> allowed_hosts;
  std::vector<std::string> blocked_hosts;

  // Expected behaviors for the given url under different web filter types,
  // assuming manual lists as above.
  FilteringBehavior certain_sites_behavior;
  FilteringBehavior allow_all_sites_behavior;

  // If set, checks if the conflict tracking histogram records the expected
  // value. If nullopt, the test will not check the histogram.
  std::optional<bool> has_conflict = std::nullopt;
};

// Test cases where manual behavior only allows listed hosts.
class FamilyLinkUrlFilterManualBehaviorHostConflictsTest
    : public WithFeatureOverrideAndParamInterface<HostConflictsTestCase>,
      public FamilyLinkUrlFilterManualBehaviorTestBase {
 protected:
  FamilyLinkUrlFilterManualBehaviorHostConflictsTest()
      : WithFeatureOverrideAndParamInterface<HostConflictsTestCase>(
            kSupervisedUserUseUrlFilteringService),
        FamilyLinkUrlFilterManualBehaviorTestBase() {}

  void SetUp() override {
    EnableParentalControls(*test_env().pref_service());
    for (const auto& host : GetTestCase().allowed_hosts) {
      test_env().SetManualFilterForHost(host, true);
    }
    for (const auto& host : GetTestCase().blocked_hosts) {
      test_env().SetManualFilterForHost(host, false);
    }
  }
};

TEST_P(FamilyLinkUrlFilterManualBehaviorHostConflictsTest, CertainSites) {
  test_env().SetWebFilterType(WebFilterType::kCertainSites);
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL(GetTestCase().under_test_url))
                .behavior,
            GetTestCase().certain_sites_behavior);

  if (GetTestCase().has_conflict.has_value()) {
    histogram_tester().ExpectBucketCount(
        "FamilyUser.ManagedSiteList.Conflict",
        static_cast<int>(*GetTestCase().has_conflict), 1);
  } else {
    histogram_tester().ExpectTotalCount("FamilyUser.ManagedSiteList.Conflict",
                                        0);
  }

  if (!GetTestCase().has_conflict.value_or(false)) {
    histogram_tester().ExpectTotalCount(
        "FamilyUser.ManagedSiteList.SubdomainConflictType", 0);
  } else {
    histogram_tester().ExpectBucketCount(
        "FamilyUser.ManagedSiteList.SubdomainConflictType",
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::kOtherConflictOnly,
        1);
  }
}

TEST_P(FamilyLinkUrlFilterManualBehaviorHostConflictsTest, AllowAllSites) {
  test_env().SetWebFilterType(WebFilterType::kAllowAllSites);
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL(GetTestCase().under_test_url))
                .behavior,
            GetTestCase().allow_all_sites_behavior);
  if (GetTestCase().has_conflict.has_value()) {
    histogram_tester().ExpectBucketCount(
        "FamilyUser.ManagedSiteList.Conflict",
        static_cast<int>(*GetTestCase().has_conflict), 1);
  } else {
    histogram_tester().ExpectTotalCount("FamilyUser.ManagedSiteList.Conflict",
                                        0);
  }

  if (!GetTestCase().has_conflict.value_or(false)) {
    histogram_tester().ExpectTotalCount(
        "FamilyUser.ManagedSiteList.SubdomainConflictType", 0);
  } else {
    histogram_tester().ExpectBucketCount(
        "FamilyUser.ManagedSiteList.SubdomainConflictType",
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::kOtherConflictOnly,
        1);
  }
}

// Note: test cases must be unique. They assume no trivial subdomain conflicts.
const HostConflictsTestCase kHostConflictsTestCases[] = {
    // Matches only allow list, takes precedence over default behavior.
    {.test_name = "ManualAllowListTakesPrecedence_1",
     .under_test_url = "http://www.google.com/foo/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com"},
     .certain_sites_behavior = FilteringBehavior::kAllow,
     .allow_all_sites_behavior = FilteringBehavior::kAllow,
     .has_conflict = false},

    {.test_name = "ManualAllowListTakesPrecedence_2",
     .under_test_url = "http://mail.google.com/moose/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com"},
     .certain_sites_behavior = FilteringBehavior::kAllow,
     .allow_all_sites_behavior = FilteringBehavior::kAllow,
     .has_conflict = false},

    // No list matched, default behavior is used.
    {.test_name = "UnmatchedListsLeaveDefaultBehavior_1",
     .under_test_url = "http://www.google.co.uk/blurp/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com"},
     .certain_sites_behavior = FilteringBehavior::kBlock,
     .allow_all_sites_behavior = FilteringBehavior::kAllow},

    // "*.google.com" conflicts with "calendar.google.com"
    // "*.google.com" conflicts with "www.google.*"
    // In case of conflicts, block takes precedence.
    {.test_name = "Conflicts_1",
     .under_test_url = "http://www.google.com/foo/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com", "www.google.*"},
     .certain_sites_behavior = FilteringBehavior::kBlock,
     .allow_all_sites_behavior = FilteringBehavior::kBlock,
     .has_conflict = true},
    {.test_name = "Conflicts_2",
     .under_test_url = "http://calendar.google.com/bar/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com", "www.google.*"},
     .certain_sites_behavior = FilteringBehavior::kBlock,
     .allow_all_sites_behavior = FilteringBehavior::kBlock,
     .has_conflict = true},

    // No conflicts: the url is blocked or allowed because of single matching
    // rule.
    {.test_name = "NoConflicts_1",
     .under_test_url = "http://mail.google.com/moose/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com", "www.google.*"},
     .certain_sites_behavior = FilteringBehavior::kAllow,
     .allow_all_sites_behavior = FilteringBehavior::kAllow,
     .has_conflict = false},
    {.test_name = "NoConflicts_2",
     .under_test_url = "http://www.google.co.uk/blurp/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com", "www.google.*"},
     .certain_sites_behavior = FilteringBehavior::kBlock,
     .allow_all_sites_behavior = FilteringBehavior::kBlock,
     .has_conflict = false},

    // More obvious conflict where block takes precedence.
    {.test_name = "ManualBlockListTakesPrecedence",
     .under_test_url = "http://calendar.google.com/bar/",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com", "www.google.*"},
     .certain_sites_behavior = FilteringBehavior::kBlock,
     .allow_all_sites_behavior = FilteringBehavior::kBlock,
     .has_conflict = true},

    {.test_name = "UnmatchedListsLeaveDefaultBehavior_2",
     .under_test_url = "http://youtube.com",
     .allowed_hosts = {"*.google.com", "mail.google.com"},
     .blocked_hosts = {"calendar.google.com", "www.google.*"},
     .certain_sites_behavior = FilteringBehavior::kBlock,
     .allow_all_sites_behavior = FilteringBehavior::kAllow},

};  // namespace

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyLinkUrlFilterManualBehaviorHostConflictsTest,
    testing::Combine(
        /*kSupervisedUserUseUrlFilteringService=*/testing::Bool(),
        testing::ValuesIn(kHostConflictsTestCases)),
    [](const auto& info) {
      bool is_feature_enabled = std::get<0>(info.param);
      return std::get<1>(info.param).test_name + "_With" +
             kSupervisedUserUseUrlFilteringService.name +
             (is_feature_enabled ? "Enabled" : "Disabled");
    });

struct HostConflictTypeTestCase {
  std::string test_name;
  std::map<std::string, bool> host_exceptions;
  std::optional<FamilyLinkUrlFilter::FilteringSubdomainConflictType>
      conflict_type;
};

// Test cases where manual behavior only allows listed hosts.
class FamilyLinkUrlFilterManualBehaviorHostConflictTypesTest
    : public WithFeatureOverrideAndParamInterface<HostConflictTypeTestCase>,
      public FamilyLinkUrlFilterManualBehaviorTestBase {
 protected:
  FamilyLinkUrlFilterManualBehaviorHostConflictTypesTest()
      : WithFeatureOverrideAndParamInterface<HostConflictTypeTestCase>(
            kSupervisedUserUseUrlFilteringService),
        FamilyLinkUrlFilterManualBehaviorTestBase() {}

  void SetUp() override {
    EnableParentalControls(*test_env().pref_service());
    test_env().SetWebFilterType(WebFilterType::kCertainSites);
    test_env().SetManualFilterForHosts(GetTestCase().host_exceptions);
  }
};

TEST_P(FamilyLinkUrlFilterManualBehaviorHostConflictTypesTest,
       SubdomainConflictTypes) {
  EXPECT_EQ(under_test()
                ->GetFilteringBehavior(GURL("https://www.google.com"))
                .behavior,
            FilteringBehavior::kBlock);

  if (GetTestCase().conflict_type.has_value()) {
    histogram_tester().ExpectBucketCount(
        "FamilyUser.ManagedSiteList.SubdomainConflictType",
        GetTestCase().conflict_type.value(), /*expected_count=*/1);
  } else {
    // When there is no conflict, no entries are recorded.
    histogram_tester().ExpectTotalCount(
        "FamilyUser.ManagedSiteList.SubdomainConflictType",
        /*expected_count=*/0);
  }
}

// Note: test cases must be unique. They are evaluated against the same url (see
// test body).
const HostConflictTypeTestCase kHostConflictTypesTestCases[] = {
    {
        "TrivialConflictOnly_1",
        {{"www.google.com", true}, {"https://google.com", false}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictOnly,
    },
    {
        "TrivialConflictOnly_2",
        {{"www.google.com", false}, {"https://google.com", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictOnly,
    },
    {
        "TrivialConflictOnly_3",
        {{"http://www.google.*", false}, {"google.*", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictOnly,
    },
    {
        "TrivialConflictOnly_4_Between_GoogleCom_AndOtherEntries",
        {{"https://www.google.com", false},
         {"google.com", true},
         {"www.google.com", false},
         {"http://www.google.com", false}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictOnly,
    },
    {
        "TrivialConflictOnly_5_Between_GoogleCom_And_WwwGoogleCom",
        {{"https://google.com", false},
         {"www.google.com", true},
         {"*.google.*", false}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictOnly,
    },
    {
        "TrivialConflictOnly_6_Between_GoogleCom_And_WwwGoogleCom",
        {{"https://www.google.com", false},
         {"www.google.*", false},
         {"google.com", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictOnly,
    },

    {
        "OtherConflictOnly_1",
        {{"http://www.google.com", false}, {"*.google.*", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::kOtherConflictOnly,
    },
    {
        "OtherConflictOnly_2",
        {{"*.google.com", false}, {"www.google.com", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::kOtherConflictOnly,
    },
    {
        "OtherConflictOnly_3",
        {{"http://www.google.com", false}, {"www.google.*", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::kOtherConflictOnly,
    },
    {
        "OtherConflictOnly_4",
        {{"http://google.com", false},
         {"https://google.com", true},
         {"*.google.com", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::kOtherConflictOnly,
    },

    {
        "NoConflict_1",
        {{"http://google.com", false}, {"www.google.com", false}},
        std::nullopt,
    },

    {
        "MixedConflicts_1",
        {{"https://google.com", false},
         {"www.google.com", true},
         {"*.google.com", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictAndOtherConflict,
    },
    {
        "MixedConflicts_2",
        {{"https://google.com", true},
         {"www.google.com", false},
         {"*.google.*", true}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictAndOtherConflict,
    },
    {
        "MixedConflicts_3",
        {{"https://www.google.com", true},
         {"google.com", false},
         {"google.*", true},
         {"*.google.*", false}},
        FamilyLinkUrlFilter::FilteringSubdomainConflictType::
            kTrivialSubdomainConflictAndOtherConflict,
    },
};

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyLinkUrlFilterManualBehaviorHostConflictTypesTest,
    ::testing::Combine(
        /*kSupervisedUserUseUrlFilteringService=*/testing::Bool(),
        testing::ValuesIn(kHostConflictTypesTestCases)),
    [](const auto& info) {
      bool is_feature_enabled = std::get<0>(info.param);
      return std::get<1>(info.param).test_name + "_With" +
             kSupervisedUserUseUrlFilteringService.name +
             (is_feature_enabled ? "Enabled" : "Disabled");
    });

}  // namespace
}  // namespace supervised_user
