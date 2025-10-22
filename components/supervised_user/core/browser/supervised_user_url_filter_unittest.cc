// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "supervised_user_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {

void PrintTo(FilteringBehavior behavior, std::ostream* os) {
  switch (behavior) {
    case supervised_user::FilteringBehavior::kAllow:
      *os << "kAllow";
      break;
    case supervised_user::FilteringBehavior::kBlock:
      *os << "kBlock";
      break;
    case supervised_user::FilteringBehavior::kInvalid:
      *os << "kInvalid";
      break;
  }
}

namespace {

using safe_search_api::ClassificationDetails;

class SupervisedUserURLFilterTest : public ::testing::Test,
                                    public SupervisedUserURLFilter::Observer {
 public:
  SupervisedUserURLFilterTest() {
    EnableParentalControls(*supervised_user_test_environment_.pref_service());
    supervised_user_test_environment_.SetWebFilterType(
        WebFilterType::kCertainSites);
    supervised_user_test_environment_.url_filter()->AddObserver(this);
  }

  ~SupervisedUserURLFilterTest() override {
    supervised_user_test_environment_.url_filter()->RemoveObserver(this);
    supervised_user_test_environment_.Shutdown();
  }

  // SupervisedUserURLFilter::Observer:
  void OnURLChecked(SupervisedUserURLFilter::Result result) override {
    behavior_ = result.behavior;
    reason_ = result.reason;
  }

 protected:
  // Calls GetFilteringBehavior of the underlying url filter.
  FilteringBehavior GetFilteringBehavior(std::string_view url) const {
    return supervised_user_test_environment_.url_filter()
        ->GetFilteringBehavior(GURL(url))
        .behavior;
  }

  void ExpectURLInDefaultAllowlist(const std::string& url) {
    ExpectURLCheckMatches(url, FilteringBehavior::kAllow,
                          FilteringBehaviorReason::DEFAULT);
  }

  void ExpectURLInDefaultDenylist(const std::string& url) {
    ExpectURLCheckMatches(url, FilteringBehavior::kBlock,
                          FilteringBehaviorReason::DEFAULT);
  }

  void ExpectURLInManualAllowlist(const std::string& url) {
    ExpectURLCheckMatches(url, FilteringBehavior::kAllow,
                          FilteringBehaviorReason::MANUAL);
  }

  void ExpectURLInManualDenylist(const std::string& url) {
    ExpectURLCheckMatches(url, FilteringBehavior::kBlock,
                          FilteringBehaviorReason::MANUAL);
  }

  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;

  FilteringBehavior behavior_;
  FilteringBehaviorReason reason_;

 private:
  void ExpectURLCheckMatches(const std::string& url,
                             FilteringBehavior expected_behavior,
                             FilteringBehaviorReason expected_reason,
                             bool skip_manual_parent_filter = false) {
    bool called_synchronously =
        supervised_user_test_environment_.url_filter()
            ->GetFilteringBehaviorWithAsyncChecks(GURL(url), base::DoNothing(),
                                                  skip_manual_parent_filter);
    ASSERT_TRUE(called_synchronously);

    EXPECT_EQ(behavior_, expected_behavior);
    EXPECT_EQ(reason_, expected_reason);
  }
};

TEST_F(SupervisedUserURLFilterTest, Basic) {
  supervised_user_test_environment_.SetManualFilterForHost("*.google.com",
                                                           true);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  EXPECT_EQ(GetFilteringBehavior("http://google.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://google.com/whatever"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://google.com/"),
            FilteringBehavior::kAllow);

  EXPECT_EQ(GetFilteringBehavior("http://mail.google.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://x.mail.google.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://x.mail.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://x.y.google.com/a/b"),
            FilteringBehavior::kAllow);

  EXPECT_EQ(GetFilteringBehavior("http://youtube.com/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("http://notgoogle.com/"),
            FilteringBehavior::kBlock);

  EXPECT_EQ(GetFilteringBehavior("bogus://youtube.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("chrome://youtube.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("chrome://extensions/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("chrome-extension://foo/main.html"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "file:///home/chronos/user/MyFiles/Downloads/img.jpg"),
            FilteringBehavior::kAllow);
}

TEST_F(SupervisedUserURLFilterTest, EffectiveURL) {
  supervised_user_test_environment_.SetManualFilterForHost("example.com", true);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  ASSERT_EQ(GetFilteringBehavior("http://example.com"),
            FilteringBehavior::kAllow);
  ASSERT_EQ(GetFilteringBehavior("https://example.com"),
            FilteringBehavior::kAllow);

  // AMP Cache URLs.
  EXPECT_EQ(GetFilteringBehavior("https://cdn.ampproject.org"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://cdn.ampproject.org/c/example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("https://cdn.ampproject.org/c/www.example.com"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("https://cdn.ampproject.org/c/example.com/path"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://cdn.ampproject.org/c/s/example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://cdn.ampproject.org/c/other.com"),
            FilteringBehavior::kBlock);

  EXPECT_EQ(GetFilteringBehavior("https://sub.cdn.ampproject.org"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(
      GetFilteringBehavior("https://sub.cdn.ampproject.org/c/example.com"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("https://sub.cdn.ampproject.org/c/www.example.com"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("https://sub.cdn.ampproject.org/c/example.com/path"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("https://sub.cdn.ampproject.org/c/s/example.com"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://sub.cdn.ampproject.org/c/other.com"),
            FilteringBehavior::kBlock);

  // Google AMP viewer URLs.
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/amp/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/amp/example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/amp/www.example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/amp/s/example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("https://www.google.com/amp/s/example.com/path"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/amp/other.com"),
            FilteringBehavior::kBlock);

  // Google web cache URLs.
  EXPECT_EQ(GetFilteringBehavior("https://webcache.googleusercontent.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(
      GetFilteringBehavior("https://webcache.googleusercontent.com/search"),
      FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior(
                "https://webcache.googleusercontent.com/search?q=example.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(
      GetFilteringBehavior(
          "https://webcache.googleusercontent.com/search?q=cache:example.com"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://webcache.googleusercontent.com/"
                                 "search?q=cache:example.com+search_query"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://webcache.googleusercontent.com/"
                "search?q=cache:123456789-01:example.com+search_query"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior(
          "https://webcache.googleusercontent.com/search?q=cache:other.com"),
      FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://webcache.googleusercontent.com/"
                                 "search?q=cache:other.com+example.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(
      GetFilteringBehavior("https://webcache.googleusercontent.com/"
                           "search?q=cache:123456789-01:other.com+example.com"),
      FilteringBehavior::kBlock);

  // Google Translate URLs.
  EXPECT_EQ(GetFilteringBehavior("https://translate.google.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://translate.googleusercontent.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior(
                "https://translate.google.com/translate?u=example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior(
          "https://translate.googleusercontent.com/translate?u=example.com"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://translate.google.com/translate?u=www.example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://translate.google.com/translate?u=https://example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://translate.google.com/translate?u=other.com"),
            FilteringBehavior::kBlock);
}

TEST_F(SupervisedUserURLFilterTest, Inactive) {
  supervised_user_test_environment_.SetManualFilterForHost("google.com", true);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  // If the filter is inactive, every URL should be allowed.
  EXPECT_EQ(GetFilteringBehavior("http://google.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://www.example.com"),
            FilteringBehavior::kAllow);
}

TEST_F(SupervisedUserURLFilterTest, IPAddress) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  supervised_user_test_environment_.SetManualFilterForHost("123.123.123.123",
                                                           true);

  EXPECT_EQ(GetFilteringBehavior("http://123.123.123.123/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://123.123.123.124/"),
            FilteringBehavior::kBlock);
}

TEST_F(SupervisedUserURLFilterTest, Canonicalization) {
  // We assume that the hosts and URLs are already canonicalized.
  supervised_user_test_environment_.SetManualFilterForHost("www.moose.org",
                                                           true);
  supervised_user_test_environment_.SetManualFilterForHost("www.xn--n3h.net",
                                                           true);
  supervised_user_test_environment_.SetManualFilterForUrl(
      "http://www.example.com/foo/", true);
  supervised_user_test_environment_.SetManualFilterForUrl(
      "http://www.example.com/%C3%85t%C3%B8mstr%C3%B6m", true);

  // Base cases.
  EXPECT_EQ(GetFilteringBehavior("http://www.example.com/foo/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(
      GetFilteringBehavior("http://www.example.com/%C3%85t%C3%B8mstr%C3%B6m"),
      FilteringBehavior::kAllow);

  // Verify that non-URI characters are escaped.
  EXPECT_EQ(GetFilteringBehavior(
                "http://www.example.com/\xc3\x85t\xc3\xb8mstr\xc3\xb6m"),
            FilteringBehavior::kAllow);

  // Verify that unnecessary URI escapes remain escaped.
  EXPECT_EQ(GetFilteringBehavior("http://www.example.com/%66%6F%6F/"),
            FilteringBehavior::kBlock);

  // Verify that the default port are removed.
  EXPECT_EQ(GetFilteringBehavior("http://www.example.com:80/foo/"),
            FilteringBehavior::kAllow);

  // Verify that scheme and hostname are lowercased.
  EXPECT_EQ(GetFilteringBehavior("htTp://wWw.eXamPle.com/foo/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("HttP://WwW.mOOsE.orG/blurp/"),
            FilteringBehavior::kAllow);

  // Verify that UTF-8 in hostnames are converted to punycode.
  EXPECT_EQ(GetFilteringBehavior("http://www.\xe2\x98\x83\x0a.net/bla/"),
            FilteringBehavior::kAllow);

  // Verify that query and ref are stripped.
  EXPECT_EQ(GetFilteringBehavior("http://www.example.com/foo/?bar=baz#ref"),
            FilteringBehavior::kAllow);
}

TEST_F(SupervisedUserURLFilterTest, UrlWithNonStandardUrlSchemeAllowed) {
  // Non-standard url scheme.
  EXPECT_EQ(GetFilteringBehavior("file://example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("filesystem://80cols.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("chrome://example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("wtf://example.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("gopher://example.com"),
            FilteringBehavior::kAllow);

  // Standard url scheme.
  EXPECT_EQ(GetFilteringBehavior(("http://example.com")),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://example.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("ftp://example.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("ws://example.com"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("wss://example.com"),
            FilteringBehavior::kBlock);
}

TEST_F(SupervisedUserURLFilterTest, HostMatchesPattern) {
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                          "google.com"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                          "*.google.com"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("google.com",
                                                          "*.google.com"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("accounts.google.com",
                                                          "*.google.com"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.google.de",
                                                           "*.google.com"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("notgoogle.com",
                                                           "*.google.com"));

  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                          "www.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.de",
                                                          "www.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.co.uk",
                                                          "www.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern(
      "www.google.blogspot.com", "www.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.google",
                                                           "www.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("google.com",
                                                          "www.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("mail.google.com",
                                                           "www.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.googleplex.com",
                                                           "www.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.googleco.uk",
                                                           "www.google.*"));

  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                          "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("google.com", "*.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("accounts.google.com",
                                                          "*.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("mail.google.com",
                                                          "*.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.de",
                                                          "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("google.de", "*.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern(
      "google.blogspot.com", "*.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("google", "*.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("notgoogle.com",
                                                           "*.google.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.googleplex.com",
                                                           "*.google.*"));

  // Now test a few invalid patterns. They should never match.
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", ""));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "."));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", ".*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*."));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google..com", "*..*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*.*.com"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "www.*.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                           "*.goo.*le.*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                           "*google*"));
  EXPECT_FALSE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                           "www.*.google.com"));
}

TEST_F(SupervisedUserURLFilterTest, PatternsWithoutConflicts) {
  // The third rule is redundant with the first, but it's not a conflict
  // since they have the same value (allow).
  supervised_user_test_environment_.SetManualFilterForHost("*.google.com",
                                                           true);
  supervised_user_test_environment_.SetManualFilterForHost(
      "calendar.google.com", false);
  supervised_user_test_environment_.SetManualFilterForHost("mail.google.com",
                                                           true);

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  EXPECT_EQ(GetFilteringBehavior("http://www.google.com/foo/"),
            FilteringBehavior::kAllow)
      << "Manual allow list should take precendence";
  EXPECT_EQ(GetFilteringBehavior("http://calendar.google.com/bar/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("http://mail.google.com/moose/"),
            FilteringBehavior::kAllow)
      << "Manual allow list should take precendence";
  EXPECT_EQ(GetFilteringBehavior("http://www.google.co.uk/blurp/"),
            FilteringBehavior::kBlock);

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  EXPECT_EQ(GetFilteringBehavior("http://www.google.com/foo/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://calendar.google.com/bar/"),
            FilteringBehavior::kBlock)
      << "Manual block list should take precendence";
  EXPECT_EQ(GetFilteringBehavior("http://mail.google.com/moose/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://www.google.co.uk/blurp/"),
            FilteringBehavior::kAllow);
}

TEST_F(SupervisedUserURLFilterTest, PatternsWithConflicts) {
  std::map<std::string, bool> hosts;
  base::HistogramTester histogram_tester;

  // First and second rule always conflicting.
  // The fourth rule conflicts with the first for "www.google.com" host.
  // Blocking then takes precedence.
  supervised_user_test_environment_.SetManualFilterForHost("*.google.com",
                                                           true);
  supervised_user_test_environment_.SetManualFilterForHost(
      "calendar.google.com", false);
  supervised_user_test_environment_.SetManualFilterForHost("mail.google.com",
                                                           true);
  supervised_user_test_environment_.SetManualFilterForHost("www.google.*",
                                                           false);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  EXPECT_EQ(GetFilteringBehavior("http://www.google.com/foo/"),
            FilteringBehavior::kBlock);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      1, 1);
  // Match with conflicting first and second rule.
  EXPECT_EQ(GetFilteringBehavior("http://calendar.google.com/bar/"),
            FilteringBehavior::kBlock);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      1, 2);

  // Match with first and third rule both allowed, no conflict.
  EXPECT_EQ(GetFilteringBehavior("http://mail.google.com/moose/"),
            FilteringBehavior::kAllow);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      0, 1);

  // Match with fourth rule.
  EXPECT_EQ(GetFilteringBehavior("http://www.google.co.uk/blurp/"),
            FilteringBehavior::kBlock);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      0, 2);

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  EXPECT_EQ(GetFilteringBehavior("http://www.google.com/foo/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("http://calendar.google.com/bar/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("http://mail.google.com/moose/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://www.google.co.uk/blurp/"),
            FilteringBehavior::kBlock);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      1, 4);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      0, 4);

  // No known rule, the metric is not recorded.
  EXPECT_EQ(GetFilteringBehavior("https://youtube.com"),
            FilteringBehavior::kAllow);
  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      8);
}

TEST_F(SupervisedUserURLFilterTest, Reason) {
  supervised_user_test_environment_.SetManualFilterForHost("youtube.com", true);
  supervised_user_test_environment_.SetManualFilterForHost("*.google.*", true);
  supervised_user_test_environment_.SetManualFilterForUrl(
      "https://youtube.com/robots.txt", false);
  supervised_user_test_environment_.SetManualFilterForUrl(
      "https://google.co.uk/robots.txt", false);

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  ExpectURLInDefaultDenylist("https://m.youtube.com/feed/trending");
  ExpectURLInDefaultDenylist("https://com.google");
  ExpectURLInManualAllowlist("https://youtube.com/feed/trending");
  ExpectURLInManualAllowlist("https://google.com/humans.txt");
  ExpectURLInManualDenylist("https://youtube.com/robots.txt");
  ExpectURLInManualDenylist("https://google.co.uk/robots.txt");

  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  ExpectURLInManualAllowlist("https://youtube.com/feed/trending");
  ExpectURLInManualAllowlist("https://google.com/humans.txt");
  ExpectURLInManualDenylist("https://youtube.com/robots.txt");
  ExpectURLInManualDenylist("https://google.co.uk/robots.txt");
}

TEST_F(SupervisedUserURLFilterTest, UrlsNotRequiringGuardianApprovalAllowed) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  EXPECT_EQ(GetFilteringBehavior("https://families.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://families.google.com"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://families.google.com/something"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("http://families.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://subdomain.families.google.com/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://myaccount.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://accounts.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://familylink.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://policies.google.com/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://support.google.com/"),
            FilteringBehavior::kAllow);

  // Chrome sync dashboard URLs (base initial URL, plus the version with locale
  // appended, and the redirect URL with locale appended).
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/settings/chrome/sync"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://www.google.com/settings/chrome/data"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://www.google.com/settings/chrome/sync?hl=en-US"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://chrome.google.com/sync?hl=en-US"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://chrome.google.com/data?hl=en-US"),
            FilteringBehavior::kAllow);
}

TEST_F(SupervisedUserURLFilterTest, PlayTermsAlwaysAllowed) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);
  EXPECT_EQ(GetFilteringBehavior("https://play.google/play-terms"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://play.google.com/about/play-terms"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://play.google/play-terms/"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior("https://play.google.com/about/play-terms/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(
      GetFilteringBehavior("https://play.google/intl/pt-BR_pt/play-terms/"),
      FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://play.google.com/intl/pt-BR_pt/about/play-terms/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://play.google/play-terms/index.html"),
            FilteringBehavior::kAllow);
  EXPECT_EQ(GetFilteringBehavior(
                "https://play.google.com/about/play-terms/index.html"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("http://play.google/play-terms/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("http://play.google.com/about/play-terms/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://subdomain.play.google/play-terms/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior(
                "https://subdomain.play.google.com/about/play-terms/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://play.google/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://play.google.com/"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://play.google/about"),
            FilteringBehavior::kBlock);
  EXPECT_EQ(GetFilteringBehavior("https://play.google.com/about"),
            FilteringBehavior::kBlock);
}

TEST_F(SupervisedUserURLFilterTest,
       PlainWebFilterConfigurationWontDoAsyncCheck) {
  // The url filter crashes without a checker client if asked to do an
  // asynchronous classification, unless the filter managed to decide
  // synchronously.
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  SupervisedUserURLFilter::Result result;
  EXPECT_TRUE(
      supervised_user_test_environment_.url_filter()
          ->GetFilteringBehaviorWithAsyncChecks(
              GURL("http://example.com"),
              base::BindLambdaForTesting(
                  [&result](SupervisedUserURLFilter::Result r) { result = r; }),
              /*skip_manual_parent_filter=*/false))
      << "The check should be synchronous";
  EXPECT_TRUE(result.IsAllowed())
      << "Plain filter configuration should classify urls as allowed";
}

class SupervisedUserURLFilteringWithConflictsTest
    : public SupervisedUserURLFilterTest,
      public testing::WithParamInterface<std::tuple<
          std::map<std::string, bool>,
          std::optional<
              SupervisedUserURLFilter::FilteringSubdomainConflictType>>> {
 public:
  SupervisedUserURLFilteringWithConflictsTest() {
    EnableParentalControls(*supervised_user_test_environment_.pref_service());
    supervised_user_test_environment_.SetWebFilterType(
        WebFilterType::kCertainSites);
  }
};

// Tests that the new histogram that records www-subdomain conflicts
// increases only when the corresponding conflict types occurs.
TEST_P(SupervisedUserURLFilteringWithConflictsTest,
       PatternsWithSubdomainConflicts) {
  base::HistogramTester histogram_tester;

  auto conflict_type = std::get<1>(GetParam());

  supervised_user_test_environment_.SetManualFilterForHosts(
      std::get<0>(GetParam()));

  EXPECT_EQ(GetFilteringBehavior("https://www.google.com"),
            FilteringBehavior::kBlock);

  if (conflict_type.has_value()) {
    histogram_tester.ExpectBucketCount(
        SupervisedUserURLFilter::
            GetManagedSiteListConflictTypeHistogramNameForTest(),
        /*sample=*/conflict_type.value(), /*expected_count=*/1);
  } else {
    // When there is no conflict, no entries are recorded.
    histogram_tester.ExpectTotalCount(
        SupervisedUserURLFilter::
            GetManagedSiteListConflictTypeHistogramNameForTest(),
        /*expected_count=*/0);
  }
}

// Tests that conflict tracking histogram records a result for no conflicts
// even for paths that determine a result and exit early.
TEST_F(SupervisedUserURLFilteringWithConflictsTest,
       PatternWithoutConflictOnEarlyExit) {
  base::HistogramTester histogram_tester;
  // The host map is empty but the url map contains an exact match.
  supervised_user_test_environment_.SetManualFilterForUrl(
      "https://www.google.com", true);

  EXPECT_EQ(GetFilteringBehavior("https://www.google.com"),
            FilteringBehavior::kAllow);

  // When there is no conflict, no entries as recorded in the conflict type
  // histogram. A non-conflict entry is recorded on the conflict tracking histogram.
  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::
          GetManagedSiteListConflictTypeHistogramNameForTest(),
      /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    SubdomainConflicts,
    SupervisedUserURLFilteringWithConflictsTest,
    testing::Values(
        /* Only trivial subdomain conflicts: */
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"www.google.com", true}, {"https://google.com", false}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictOnly),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"www.google.com", false}, {"https://google.com", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictOnly),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"http://www.google.*", false}, {"google.*", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictOnly),
        std::make_tuple(
            // The collision happens because of the trivial subdomain collision
            // between google.com and other entries.
            /* host_map= */ std::map<std::string, bool>(
                {{"https://www.google.com", false},
                 {"google.com", true},
                 {"www.google.com", false},
                 {"http://www.google.com", false}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictOnly),
        std::make_tuple(
            // The collision happens because of the trivial subdomain collision
            // between https://google.com and www.google.com.
            /* host_map= */ std::map<std::string, bool>(
                {{"https://google.com", false},
                 {"www.google.com", true},
                 {"*.google.*", false}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictOnly),
        std::make_tuple(
            // The collision happens because of the trivial subdomain collision
            // between https://google.com and www.google.com.
            std::map<std::string, bool>({{"https://www.google.com", false},
                                         {"www.google.*", false},
                                         {"google.com", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictOnly),
        /* Only other conflicts: */
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"http://www.google.com", false}, {"*.google.*", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kOtherConflictOnly),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"*.google.com", false}, {"www.google.com", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kOtherConflictOnly),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"http://www.google.com", false}, {"www.google.*", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kOtherConflictOnly),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"http://google.com", false},
                 {"https://google.com", true},
                 {"*.google.com", true}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kOtherConflictOnly),
        /* No conflicts: */
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"http://google.com", false}, {"www.google.com", false}}),
            std::nullopt),
        /* Mix of www-subdomain conflicts and other conflicts */
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"https://google.com", false},
                 {"www.google.com", true},
                 {"*.google.com", true}}),  // Other conflict entry
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictAndOtherConflict),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"https://google.com", true},
                 {"www.google.com", false},
                 {"*.google.*", true}}),  // Other conflict entry
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictAndOtherConflict),
        std::make_tuple(
            /* host_map= */ std::map<std::string, bool>(
                {{"https://www.google.com", true},
                 {"google.com", false},
                 {"google.*", true},  // Other conflict entry
                 {"*.google.*", false}}),
            SupervisedUserURLFilter::FilteringSubdomainConflictType::
                kTrivialSubdomainConflictAndOtherConflict)));

struct MetricTestParam {
  // Context of filtering
  FilteringContext context;

  // Name of the histogram to emit that is specific for context (alongside
  // aggregated and legacy histograms).
  std::string context_specific_histogram;

  // Human-readable label of test case.
  std::string label;
};

class SupervisedUserURLFilterMetricsTest
    : public ::testing::TestWithParam<MetricTestParam> {
 protected:
  void SetUp() override {
    EnableParentalControls(*supervised_user_test_environment_.pref_service());
  }
  void TearDown() override { supervised_user_test_environment_.Shutdown(); }

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
};

TEST_P(SupervisedUserURLFilterMetricsTest,
       RecordsTopLevelMetricsForBlockNotInAllowlist) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  ASSERT_TRUE(supervised_user_test_environment_.url_filter()
                  ->GetFilteringBehaviorWithAsyncChecks(
                      GURL("http://example.com"), base::DoNothing(), false,
                      GetParam().context));

  if (GetParam().context == FilteringContext::kNavigationThrottle) {
    histogram_tester_.ExpectBucketCount(
        "ManagedUsers.TopLevelFilteringResult",
        SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
  }
  histogram_tester_.ExpectBucketCount(
      "ManagedUsers.TopLevelFilteringResult2",
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
  histogram_tester_.ExpectBucketCount(
      GetParam().context_specific_histogram,
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
}

TEST_P(SupervisedUserURLFilterMetricsTest, RecordsTopLevelMetricsForAllow) {
  supervised_user_test_environment_.SetManualFilterForHost("http://example.com",
                                                           true);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  ASSERT_TRUE(supervised_user_test_environment_.url_filter()
                  ->GetFilteringBehaviorWithAsyncChecks(
                      GURL("http://example.com"), base::DoNothing(), false,
                      GetParam().context));

  if (GetParam().context == FilteringContext::kNavigationThrottle) {
    histogram_tester_.ExpectBucketCount(
        "ManagedUsers.TopLevelFilteringResult",
        SupervisedUserFilterTopLevelResult::kAllow, 1);
  }
  histogram_tester_.ExpectBucketCount(
      "ManagedUsers.TopLevelFilteringResult2",
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester_.ExpectBucketCount(
      GetParam().context_specific_histogram,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
}

TEST_P(SupervisedUserURLFilterMetricsTest,
       RecordsTopLevelMetricsForBlockManual) {
  supervised_user_test_environment_.SetManualFilterForHost("http://example.com",
                                                           false);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  ASSERT_TRUE(supervised_user_test_environment_.url_filter()
                  ->GetFilteringBehaviorWithAsyncChecks(
                      GURL("http://example.com"), base::DoNothing(), false,
                      GetParam().context));

  if (GetParam().context == FilteringContext::kNavigationThrottle) {
    histogram_tester_.ExpectBucketCount(
        "ManagedUsers.TopLevelFilteringResult",
        SupervisedUserFilterTopLevelResult::kBlockManual, 1);
  }
  histogram_tester_.ExpectBucketCount(
      "ManagedUsers.TopLevelFilteringResult2",
      SupervisedUserFilterTopLevelResult::kBlockManual, 1);
  histogram_tester_.ExpectBucketCount(
      GetParam().context_specific_histogram,
      SupervisedUserFilterTopLevelResult::kBlockManual, 1);
}

TEST_P(SupervisedUserURLFilterMetricsTest,
       RecordsTopLevelMetricsForAsyncBlock) {
  ASSERT_FALSE(supervised_user_test_environment_.url_filter()
                   ->GetFilteringBehaviorWithAsyncChecks(
                       GURL("http://example.com"), base::DoNothing(), false,
                       GetParam().context));
  supervised_user_test_environment_.url_checker_client()->RunCallback(
      safe_search_api::ClientClassification::kRestricted);

  if (GetParam().context == FilteringContext::kNavigationThrottle) {
    histogram_tester_.ExpectBucketCount(
        "ManagedUsers.TopLevelFilteringResult",
        SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
  }
  histogram_tester_.ExpectBucketCount(
      "ManagedUsers.TopLevelFilteringResult2",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
  histogram_tester_.ExpectBucketCount(
      GetParam().context_specific_histogram,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
}

TEST_P(SupervisedUserURLFilterMetricsTest,
       RecordsTopLevelMetricsForAsyncAllow) {
  ASSERT_FALSE(supervised_user_test_environment_.url_filter()
                   ->GetFilteringBehaviorWithAsyncChecks(
                       GURL("http://example.com"), base::DoNothing(), false,
                       GetParam().context));
  supervised_user_test_environment_.url_checker_client()->RunCallback(
      safe_search_api::ClientClassification::kAllowed);

  if (GetParam().context == FilteringContext::kNavigationThrottle) {
    histogram_tester_.ExpectBucketCount(
        "ManagedUsers.TopLevelFilteringResult",
        SupervisedUserFilterTopLevelResult::kAllow, 1);
  }
  histogram_tester_.ExpectBucketCount(
      "ManagedUsers.TopLevelFilteringResult2",
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester_.ExpectBucketCount(
      GetParam().context_specific_histogram,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
}

const MetricTestParam kMetricTestParams[] = {
    {.context = FilteringContext::kNavigationThrottle,
     .context_specific_histogram =
         "ManagedUsers.TopLevelFilteringResult2.NavigationThrottle",
     .label = "NavigationThrottleContext"},
    {.context = FilteringContext::kDefault,
     .context_specific_histogram =
         "ManagedUsers.TopLevelFilteringResult2.Default",
     .label = "DefaultContext"},
    {.context = FilteringContext::kNavigationObserver,
     .context_specific_histogram =
         "ManagedUsers.TopLevelFilteringResult2.NavigationObserver",
     .label = "NavigationObserverContext"},
    {.context = FilteringContext::kFamilyLinkSettingsUpdated,
     .context_specific_histogram =
         "ManagedUsers.TopLevelFilteringResult2.FamilyLinkSettingsUpdated",
     .label = "FamilyLinkSettingsUpdated"},
};

INSTANTIATE_TEST_SUITE_P(,
                         SupervisedUserURLFilterMetricsTest,
                         testing::ValuesIn(kMetricTestParams),
                         [](const auto& info) { return info.param.label; });

TEST(SupervisedUserURLFilterResultTest, IsFromManualList) {
  SupervisedUserURLFilter::Result allow{GURL("http://example.com"),
                                        FilteringBehavior::kAllow,
                                        FilteringBehaviorReason::MANUAL};
  SupervisedUserURLFilter::Result block{GURL("http://example.com"),
                                        FilteringBehavior::kBlock,
                                        FilteringBehaviorReason::MANUAL};
  SupervisedUserURLFilter::Result invalid{GURL("http://example.com"),
                                          FilteringBehavior::kInvalid,
                                          FilteringBehaviorReason::MANUAL};

  EXPECT_TRUE(allow.IsFromManualList());
  EXPECT_TRUE(block.IsFromManualList());
  EXPECT_TRUE(invalid.IsFromManualList());

  EXPECT_FALSE(allow.IsFromDefaultSetting());
  EXPECT_FALSE(block.IsFromDefaultSetting());
  EXPECT_FALSE(invalid.IsFromDefaultSetting());
}

TEST(SupervisedUserURLFilterResultTest, IsFromDefaultSetting) {
  SupervisedUserURLFilter::Result allow{GURL("http://example.com"),
                                        FilteringBehavior::kAllow,
                                        FilteringBehaviorReason::DEFAULT};
  SupervisedUserURLFilter::Result block{GURL("http://example.com"),
                                        FilteringBehavior::kBlock,
                                        FilteringBehaviorReason::DEFAULT};
  SupervisedUserURLFilter::Result invalid{GURL("http://example.com"),
                                          FilteringBehavior::kInvalid,
                                          FilteringBehaviorReason::DEFAULT};

  EXPECT_TRUE(allow.IsFromDefaultSetting());
  EXPECT_TRUE(block.IsFromDefaultSetting());
  EXPECT_TRUE(invalid.IsFromDefaultSetting());

  EXPECT_FALSE(allow.IsFromManualList());
  EXPECT_FALSE(block.IsFromManualList());
  EXPECT_FALSE(invalid.IsFromManualList());
}

TEST(SupervisedUserURLFilterResultTest, IsClassificationSuccessful) {
  SupervisedUserURLFilter::Result allow_from_list{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::MANUAL};
  SupervisedUserURLFilter::Result allow_from_settings{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::DEFAULT};
  SupervisedUserURLFilter::Result allow_from_server{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFreshServerResponse})};
  SupervisedUserURLFilter::Result allow_from_cache{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kCachedResponse})};

  SupervisedUserURLFilter::Result block_from_list{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::MANUAL};
  SupervisedUserURLFilter::Result block_from_settings{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::DEFAULT};
  SupervisedUserURLFilter::Result block_from_server{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFreshServerResponse})};
  SupervisedUserURLFilter::Result block_from_cache{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kCachedResponse})};

  EXPECT_TRUE(allow_from_list.IsClassificationSuccessful());
  EXPECT_TRUE(allow_from_settings.IsClassificationSuccessful());
  EXPECT_TRUE(allow_from_server.IsClassificationSuccessful());
  EXPECT_TRUE(allow_from_cache.IsClassificationSuccessful());
  EXPECT_TRUE(block_from_list.IsClassificationSuccessful());
  EXPECT_TRUE(block_from_settings.IsClassificationSuccessful());
  EXPECT_TRUE(block_from_server.IsClassificationSuccessful());
  EXPECT_TRUE(block_from_cache.IsClassificationSuccessful());
}

TEST(SupervisedUserURLFilterResultTest, IsClassificationNotSuccessful) {
  SupervisedUserURLFilter::Result allow_from_server{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};
  SupervisedUserURLFilter::Result allow_from_cache{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};

  SupervisedUserURLFilter::Result block_from_server{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};
  SupervisedUserURLFilter::Result block_from_cache{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};

  EXPECT_FALSE(allow_from_server.IsClassificationSuccessful());
  EXPECT_FALSE(allow_from_cache.IsClassificationSuccessful());
  EXPECT_FALSE(block_from_server.IsClassificationSuccessful());
  EXPECT_FALSE(block_from_cache.IsClassificationSuccessful());
}

}  // namespace
}  // namespace supervised_user
