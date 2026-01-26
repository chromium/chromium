// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_url_filter.h"

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

class FamilyLinkUrlFilterTest : public ::testing::Test,
                                public FamilyLinkUrlFilter::Observer {
 public:
  FamilyLinkUrlFilterTest() {
    EnableParentalControls(*supervised_user_test_environment_.pref_service());
    supervised_user_test_environment_.SetWebFilterType(
        WebFilterType::kCertainSites);
    supervised_user_test_environment_.url_filter()->AddObserver(this);
  }

  ~FamilyLinkUrlFilterTest() override {
    supervised_user_test_environment_.url_filter()->RemoveObserver(this);
    supervised_user_test_environment_.Shutdown();
  }

  // FamilyLinkUrlFilter::Observer:
  void OnURLChecked(WebFilteringResult result) override {
    behavior_ = result.behavior;
    reason_ = result.reason;
  }

 protected:
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
    supervised_user_test_environment_.url_filtering_service()
        ->GetFilteringBehavior(GURL(url), skip_manual_parent_filter,
                               base::DoNothing());

    EXPECT_EQ(behavior_, expected_behavior);
    EXPECT_EQ(reason_, expected_reason);
  }
};

TEST_F(FamilyLinkUrlFilterTest, HostMatchesPattern) {
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "google.com"));
  EXPECT_TRUE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com",
                                                      "*.google.com"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("google.com", "*.google.com"));
  EXPECT_TRUE(FamilyLinkUrlFilter::HostMatchesPattern("accounts.google.com",
                                                      "*.google.com"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.de", "*.google.com"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("notgoogle.com", "*.google.com"));

  EXPECT_TRUE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com",
                                                      "www.google.*"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.de", "www.google.*"));
  EXPECT_TRUE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.co.uk",
                                                      "www.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern(
      "www.google.blogspot.com", "www.google.*"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google", "www.google.*"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("google.com", "www.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("mail.google.com",
                                                       "www.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.googleplex.com",
                                                       "www.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.googleco.uk",
                                                       "www.google.*"));

  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*.google.*"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("google.com", "*.google.*"));
  EXPECT_TRUE(FamilyLinkUrlFilter::HostMatchesPattern("accounts.google.com",
                                                      "*.google.*"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("mail.google.com", "*.google.*"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.de", "*.google.*"));
  EXPECT_TRUE(
      FamilyLinkUrlFilter::HostMatchesPattern("google.de", "*.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("google.blogspot.com",
                                                       "*.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("google", "*.google.*"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("notgoogle.com", "*.google.*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.googleplex.com",
                                                       "*.google.*"));

  // Now test a few invalid patterns. They should never match.
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", ""));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "."));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", ".*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*."));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*.*"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google..com", "*..*"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*.*.com"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "www.*.*"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*.goo.*le.*"));
  EXPECT_FALSE(
      FamilyLinkUrlFilter::HostMatchesPattern("www.google.com", "*google*"));
  EXPECT_FALSE(FamilyLinkUrlFilter::HostMatchesPattern("www.google.com",
                                                       "www.*.google.com"));
}

TEST_F(FamilyLinkUrlFilterTest, Reason) {
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

TEST_F(FamilyLinkUrlFilterTest, PlainWebFilterConfigurationWontDoAsyncCheck) {
  // The url filter crashes without a checker client if asked to do an
  // asynchronous classification, unless the filter managed to decide
  // synchronously.
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  WebFilteringResult result;
  supervised_user_test_environment_.url_filtering_service()
      ->GetFilteringBehavior(
          GURL("http://example.com"),
          /*skip_manual_parent_filter=*/false,
          base::BindLambdaForTesting(
              [&result](WebFilteringResult r) { result = r; }));
  EXPECT_TRUE(result.IsAllowed())
      << "Plain filter configuration should classify urls as allowed";
}

TEST_F(FamilyLinkUrlFilterTest, StripOnDefaultFilteringBehaviour) {
  EXPECT_EQ(
      GURL("http://example.com"),
      supervised_user_test_environment_.url_filter()->GetEffectiveUrlToUnblock(
          {.url = GURL("http://www.example.com"),
           .behavior = FilteringBehavior::kBlock,
           .reason = FilteringBehaviorReason::DEFAULT}));
}

TEST_F(FamilyLinkUrlFilterTest,
       StripOnManualFilteringBehaviourWithoutConflict) {
  EXPECT_EQ(
      GURL("http://example.com"),
      supervised_user_test_environment_.url_filter()->GetEffectiveUrlToUnblock(
          {.url = GURL("http://www.example.com"),
           .behavior = FilteringBehavior::kBlock,
           .reason = FilteringBehaviorReason::MANUAL}));
}

TEST_F(FamilyLinkUrlFilterTest,
       SkipStripOnManualFilteringBehaviourWithConflict) {
  GURL full_url("http://www.example.com");

  // Add an conflicting entry in the blocklist.
  supervised_user_test_environment_.SetManualFilterForHost(full_url.GetHost(),
                                                           /*allowlist=*/false);

  EXPECT_EQ(
      full_url,
      supervised_user_test_environment_.url_filter()->GetEffectiveUrlToUnblock(
          {.url = full_url,
           .behavior = FilteringBehavior::kBlock,
           .reason = FilteringBehaviorReason::MANUAL}));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(FamilyLinkUrlFilterTest, NormalizesUnblockingUrls) {
  GURL full_spec_url("http://admin:password@www.example.com/path?query#ref");

  // First the url has normalized trivial domain, username, password, query and
  // ref.
  ASSERT_EQ(
      GURL("http://example.com/path"),
      supervised_user_test_environment_.url_filter()->GetEffectiveUrlToUnblock(
          {.url = full_spec_url,
           .behavior = FilteringBehavior::kBlock,
           .reason = FilteringBehaviorReason::MANUAL}));

  // Now add it to the manual blocklist.
  supervised_user_test_environment_.SetManualFilterForHost(
      full_spec_url.GetHost(),
      /*allowlist=*/false);

  // This time the url is normalized without trivial domain prefixes because it
  // was added to the manual host blocklist.
  EXPECT_EQ(
      GURL("http://www.example.com/path"),
      supervised_user_test_environment_.url_filter()->GetEffectiveUrlToUnblock(
          {.url = full_spec_url,
           .behavior = FilteringBehavior::kBlock,
           .reason = FilteringBehaviorReason::MANUAL}));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

struct MetricTestParam {
  // Context of filtering
  FilteringContext context;

  // Name of the histogram to emit that is specific for context (alongside
  // aggregated and legacy histograms).
  std::string context_specific_histogram;

  // Human-readable label of test case.
  std::string label;
};

class FamilyLinkUrlFilterMetricsTest
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

TEST_P(FamilyLinkUrlFilterMetricsTest,
       RecordsTopLevelMetricsForBlockNotInAllowlist) {
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  supervised_user_test_environment_.url_filtering_service()
      ->GetFilteringBehavior(
          GURL("http://example.com"),
          /*skip_manual_parent_filter=*/false, base::DoNothing(),
          WebFilterMetricsOptions{.filtering_context = GetParam().context});

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

TEST_P(FamilyLinkUrlFilterMetricsTest, RecordsTopLevelMetricsForAllow) {
  supervised_user_test_environment_.SetManualFilterForHost("http://example.com",
                                                           true);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kCertainSites);

  supervised_user_test_environment_.url_filtering_service()
      ->GetFilteringBehavior(
          GURL("http://example.com"),
          /*skip_manual_parent_filter=*/false, base::DoNothing(),
          WebFilterMetricsOptions{.filtering_context = GetParam().context});

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

TEST_P(FamilyLinkUrlFilterMetricsTest, RecordsTopLevelMetricsForBlockManual) {
  supervised_user_test_environment_.SetManualFilterForHost("http://example.com",
                                                           false);
  supervised_user_test_environment_.SetWebFilterType(
      WebFilterType::kAllowAllSites);

  supervised_user_test_environment_.url_filtering_service()
      ->GetFilteringBehavior(
          GURL("http://example.com"), /*skip_manual_parent_filter=*/false,
          base::DoNothing(),
          WebFilterMetricsOptions{.filtering_context = GetParam().context});

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

TEST_P(FamilyLinkUrlFilterMetricsTest, RecordsTopLevelMetricsForAsyncBlock) {
  supervised_user_test_environment_.url_filtering_service()
      ->GetFilteringBehavior(
          GURL("http://example.com"), /*skip_manual_parent_filter=*/false,
          base::DoNothing(),
          WebFilterMetricsOptions{.filtering_context = GetParam().context});
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

TEST_P(FamilyLinkUrlFilterMetricsTest, RecordsTopLevelMetricsForAsyncAllow) {
  supervised_user_test_environment_.url_filtering_service()
      ->GetFilteringBehavior(
          GURL("http://example.com"), /*skip_manual_parent_filter=*/false,
          base::DoNothing(),
          WebFilterMetricsOptions{.filtering_context = GetParam().context});
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
                         FamilyLinkUrlFilterMetricsTest,
                         testing::ValuesIn(kMetricTestParams),
                         [](const auto& info) { return info.param.label; });

TEST(FamilyLinkUrlFilterResultTest, IsFromManualList) {
  WebFilteringResult allow{GURL("http://example.com"),
                           FilteringBehavior::kAllow,
                           FilteringBehaviorReason::MANUAL};
  WebFilteringResult block{GURL("http://example.com"),
                           FilteringBehavior::kBlock,
                           FilteringBehaviorReason::MANUAL};
  WebFilteringResult invalid{GURL("http://example.com"),
                             FilteringBehavior::kInvalid,
                             FilteringBehaviorReason::MANUAL};

  EXPECT_TRUE(allow.IsFromManualList());
  EXPECT_TRUE(block.IsFromManualList());
  EXPECT_TRUE(invalid.IsFromManualList());

  EXPECT_FALSE(allow.IsFromDefaultSetting());
  EXPECT_FALSE(block.IsFromDefaultSetting());
  EXPECT_FALSE(invalid.IsFromDefaultSetting());
}

TEST(FamilyLinkUrlFilterResultTest, IsFromDefaultSetting) {
  WebFilteringResult allow{GURL("http://example.com"),
                           FilteringBehavior::kAllow,
                           FilteringBehaviorReason::DEFAULT};
  WebFilteringResult block{GURL("http://example.com"),
                           FilteringBehavior::kBlock,
                           FilteringBehaviorReason::DEFAULT};
  WebFilteringResult invalid{GURL("http://example.com"),
                             FilteringBehavior::kInvalid,
                             FilteringBehaviorReason::DEFAULT};

  EXPECT_TRUE(allow.IsFromDefaultSetting());
  EXPECT_TRUE(block.IsFromDefaultSetting());
  EXPECT_TRUE(invalid.IsFromDefaultSetting());

  EXPECT_FALSE(allow.IsFromManualList());
  EXPECT_FALSE(block.IsFromManualList());
  EXPECT_FALSE(invalid.IsFromManualList());
}

TEST(FamilyLinkUrlFilterResultTest, IsClassificationSuccessful) {
  WebFilteringResult allow_from_list{GURL("http://example.com"),
                                     FilteringBehavior::kAllow,
                                     FilteringBehaviorReason::MANUAL};
  WebFilteringResult allow_from_settings{GURL("http://example.com"),
                                         FilteringBehavior::kAllow,
                                         FilteringBehaviorReason::DEFAULT};
  WebFilteringResult allow_from_server{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFreshServerResponse})};
  WebFilteringResult allow_from_cache{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kCachedResponse})};

  WebFilteringResult block_from_list{GURL("http://example.com"),
                                     FilteringBehavior::kBlock,
                                     FilteringBehaviorReason::MANUAL};
  WebFilteringResult block_from_settings{GURL("http://example.com"),
                                         FilteringBehavior::kBlock,
                                         FilteringBehaviorReason::DEFAULT};
  WebFilteringResult block_from_server{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFreshServerResponse})};
  WebFilteringResult block_from_cache{
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

TEST(FamilyLinkUrlFilterResultTest, IsClassificationNotSuccessful) {
  WebFilteringResult allow_from_server{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};
  WebFilteringResult allow_from_cache{
      GURL("http://example.com"), FilteringBehavior::kAllow,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};

  WebFilteringResult block_from_server{
      GURL("http://example.com"), FilteringBehavior::kBlock,
      FilteringBehaviorReason::ASYNC_CHECKER,
      std::optional<ClassificationDetails>(
          {.reason = ClassificationDetails::Reason::kFailedUseDefault})};
  WebFilteringResult block_from_cache{
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
