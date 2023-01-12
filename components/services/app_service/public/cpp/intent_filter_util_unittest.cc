// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter_util.h"

#include "base/values.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kHostUrlGoogle[] = "www.google.com";
const char kHostUrlGmail[] = "www.gmail.com";
const char kPathLiteral[] = "/a";
const char kPathPrefix[] = "/b/";
const char kPathGlob[] = "/c/*/d";
const char kUrlGoogleLiteral[] = "www.google.com/a";
const char kUrlGooglePrefix[] = "www.google.com/b/*";
const char kUrlGoogleGlob[] = "www.google.com/c/*/d";
const char kUrlGmailLiteral[] = "www.gmail.com/a";
const char kUrlGmailPrefix[] = "www.gmail.com/b/*";
const char kUrlGmailGlob[] = "www.gmail.com/c/*/d";
const char kAppId[] = "aaa";
}  // namespace

class IntentFilterUtilTest : public testing::Test {
 protected:
  apps::IntentFilterPtr MakeFilter(std::string scheme,
                                   std::string host,
                                   std::string path,
                                   apps::PatternMatchType pattern) {
    auto intent_filter = std::make_unique<apps::IntentFilter>();

    intent_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                           apps_util::kIntentActionView,
                                           apps::PatternMatchType::kLiteral);

    intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, scheme,
                                           apps::PatternMatchType::kLiteral);

    intent_filter->AddSingleValueCondition(apps::ConditionType::kHost, host,
                                           apps::PatternMatchType::kLiteral);

    intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, path,
                                           pattern);

    return intent_filter;
  }

  apps::IntentFilterPtr MakeHostOnlyFilter(std::string host,
                                           apps::PatternMatchType pattern) {
    auto intent_filter = std::make_unique<apps::IntentFilter>();
    intent_filter->AddSingleValueCondition(apps::ConditionType::kHost, host,
                                           pattern);

    return intent_filter;
  }
};

TEST_F(IntentFilterUtilTest, EmptyConditionList) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  EXPECT_EQ(intent_filter->GetSupportedLinksForAppManagement().size(), 0u);
}

TEST_F(IntentFilterUtilTest, SingleHostAndManyPaths) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url::kHttpScheme,
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost,
                                         kHostUrlGoogle,
                                         apps::PatternMatchType::kLiteral);

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 0u);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath,
                                         kPathLiteral,
                                         apps::PatternMatchType::kLiteral);

  links = intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);

  intent_filter->AddSingleValueCondition(
      apps::ConditionType::kPath, kPathPrefix, apps::PatternMatchType::kPrefix);

  links = intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 2u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGooglePrefix), 1u);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, kPathGlob,
                                         apps::PatternMatchType::kGlob);

  links = intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 3u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGooglePrefix), 1u);
  EXPECT_EQ(links.count(kUrlGoogleGlob), 1u);
}

TEST_F(IntentFilterUtilTest, InvalidScheme) {
  auto intent_filter = MakeFilter(url::kTelScheme, kHostUrlGoogle, kPathLiteral,
                                  apps::PatternMatchType::kLiteral);

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 0u);
}

TEST_F(IntentFilterUtilTest, ManyHostsAndOnePath) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url::kHttpScheme,
                                         apps::PatternMatchType::kLiteral);

  std::vector<apps::ConditionValuePtr> condition_values;

  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kHostUrlGoogle, apps::PatternMatchType::kLiteral));

  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kHostUrlGmail, apps::PatternMatchType::kLiteral));

  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kHost, std::move(condition_values)));

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath,
                                         kPathLiteral,
                                         apps::PatternMatchType::kLiteral);

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 2u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGmailLiteral), 1u);
}

TEST_F(IntentFilterUtilTest, ManyHostsAndManyPaths) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url::kHttpScheme,
                                         apps::PatternMatchType::kLiteral);

  std::vector<apps::ConditionValuePtr> host_condition_values;

  host_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kHostUrlGoogle, apps::PatternMatchType::kLiteral));
  host_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kHostUrlGmail, apps::PatternMatchType::kLiteral));

  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kHost, std::move(host_condition_values)));

  std::vector<apps::ConditionValuePtr> path_condition_values;

  path_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kPathLiteral, apps::PatternMatchType::kLiteral));
  path_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kPathPrefix, apps::PatternMatchType::kPrefix));
  path_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      kPathGlob, apps::PatternMatchType::kGlob));

  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kPath, std::move(path_condition_values)));

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 6u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGooglePrefix), 1u);
  EXPECT_EQ(links.count(kUrlGoogleGlob), 1u);
  EXPECT_EQ(links.count(kUrlGmailLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGmailPrefix), 1u);
  EXPECT_EQ(links.count(kUrlGmailGlob), 1u);
}

TEST_F(IntentFilterUtilTest, WildcardHost) {
  std::string host = ".google.com";
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url::kHttpScheme,
                                         apps::PatternMatchType::kLiteral);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost, host,
                                         apps::PatternMatchType::kSuffix);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath,
                                         kPathLiteral,
                                         apps::PatternMatchType::kLiteral);

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count("*.google.com/a"), 1u);
}

TEST_F(IntentFilterUtilTest, HttpsScheme) {
  auto intent_filter =
      MakeFilter(url::kHttpsScheme, kHostUrlGoogle, kPathLiteral,
                 apps::PatternMatchType::kLiteral);
  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
}

TEST_F(IntentFilterUtilTest, HttpAndHttpsSchemes) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  std::vector<apps::ConditionValuePtr> condition_values;

  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      url::kHttpScheme, apps::PatternMatchType::kLiteral));

  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      url::kHttpsScheme, apps::PatternMatchType::kLiteral));

  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(condition_values)));

  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost,
                                         kHostUrlGoogle,
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath,
                                         kPathLiteral,
                                         apps::PatternMatchType::kLiteral);

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
}

TEST_F(IntentFilterUtilTest, PathsWithNoSlash) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         url::kHttpScheme,
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost,
                                         "m.youtube.com",
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, ".*",
                                         apps::PatternMatchType::kGlob);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, ".*/foo",
                                         apps::PatternMatchType::kGlob);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, "",
                                         apps::PatternMatchType::kPrefix);

  std::set<std::string> links =
      intent_filter->GetSupportedLinksForAppManagement();

  EXPECT_EQ(links.size(), 3u);
  EXPECT_EQ(links.count("m.youtube.com/*"), 1u);
  EXPECT_EQ(links.count("m.youtube.com/.*"), 1u);
  EXPECT_EQ(links.count("m.youtube.com/.*/foo"), 1u);
}

TEST_F(IntentFilterUtilTest, IsSupportedLink) {
  auto filter = MakeFilter("https", "www.google.com", "/maps",
                           apps::PatternMatchType::kLiteral);
  ASSERT_TRUE(apps_util::IsSupportedLinkForApp(kAppId, filter));

  filter = MakeFilter("https", "www.google.com", ".*",
                      apps::PatternMatchType::kGlob);
  ASSERT_TRUE(apps_util::IsSupportedLinkForApp(kAppId, filter));
}

TEST_F(IntentFilterUtilTest, NotSupportedLink) {
  ASSERT_FALSE(apps_util::IsSupportedLinkForApp(
      kAppId, apps_util::MakeIntentFilterForMimeType("image/png")));

  auto browser_filter = std::make_unique<apps::IntentFilter>();
  browser_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                          apps_util::kIntentActionView,
                                          apps::PatternMatchType::kLiteral);
  browser_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "https",
                                          apps::PatternMatchType::kLiteral);
  ASSERT_FALSE(apps_util::IsSupportedLinkForApp(kAppId, browser_filter));

  auto host_filter = std::make_unique<apps::IntentFilter>();
  host_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                       apps_util::kIntentActionView,
                                       apps::PatternMatchType::kLiteral);
  host_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "https",
                                       apps::PatternMatchType::kLiteral);
  host_filter->AddSingleValueCondition(apps::ConditionType::kHost,
                                       "www.example.com",
                                       apps::PatternMatchType::kLiteral);
  ASSERT_FALSE(apps_util::IsSupportedLinkForApp(kAppId, browser_filter));
}

TEST_F(IntentFilterUtilTest, HostMatchOverlapLiteralAndNone) {
  auto google_domain_filter = MakeFilter("https", "www.google.com", "/",
                                         apps::PatternMatchType::kLiteral);

  auto maps_domain_filter = MakeFilter("https", "maps.google.com", "/",
                                       apps::PatternMatchType::kLiteral);

  ASSERT_FALSE(
      apps_util::FiltersHaveOverlap(maps_domain_filter, google_domain_filter));

  apps_util::AddConditionValue(apps::ConditionType::kHost, "www.google.com",
                               apps::PatternMatchType::kLiteral,
                               maps_domain_filter);

  ASSERT_TRUE(
      apps_util::FiltersHaveOverlap(maps_domain_filter, google_domain_filter));
}

TEST_F(IntentFilterUtilTest, HostMatchOverlapSuffix) {
  // Wildcard host filter
  auto wikipedia_wildcard_filter =
      MakeHostOnlyFilter(".wikipedia.org", apps::PatternMatchType::kSuffix);

  // Filters that shouldn't overlap
  auto wikipedia_com_filter =
      MakeHostOnlyFilter(".wikipedia.com", apps::PatternMatchType::kLiteral);
  auto wikipedia_no_subdomain_filter =
      MakeHostOnlyFilter("wikipedia.org", apps::PatternMatchType::kLiteral);

  ASSERT_FALSE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                             wikipedia_com_filter));
  ASSERT_FALSE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                             wikipedia_no_subdomain_filter));

  // Filters that should overlap
  auto wikipedia_subdomain_filter =
      MakeHostOnlyFilter("es.wikipedia.org", apps::PatternMatchType::kLiteral);
  auto wikipedia_empty_subdomain_filter =
      MakeHostOnlyFilter(".wikipedia.org", apps::PatternMatchType::kLiteral);
  auto wikipedia_literal_filter =
      MakeHostOnlyFilter("fr.wikipedia.org", apps::PatternMatchType::kLiteral);
  auto wikipedia_other_wildcard_filter =
      MakeHostOnlyFilter(".wikipedia.org", apps::PatternMatchType::kSuffix);
  auto wikipedia_subsubdomain_filter =
      MakeHostOnlyFilter(".es.wikipedia.org", apps::PatternMatchType::kSuffix);

  ASSERT_TRUE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                            wikipedia_subdomain_filter));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                            wikipedia_empty_subdomain_filter));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                            wikipedia_literal_filter));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                            wikipedia_other_wildcard_filter));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                            wikipedia_subsubdomain_filter));
}

TEST_F(IntentFilterUtilTest, PatternMatchOverlap) {
  auto literal_pattern_filter1 = MakeFilter("https", "www.example.com", "/",
                                            apps::PatternMatchType::kLiteral);
  apps_util::AddConditionValue(apps::ConditionType::kPath, "/foo",
                               apps::PatternMatchType::kLiteral,
                               literal_pattern_filter1);

  auto literal_pattern_filter2 = MakeFilter(
      "https", "www.example.com", "/foo/bar", apps::PatternMatchType::kLiteral);
  apps_util::AddConditionValue(apps::ConditionType::kPath, "/bar",
                               apps::PatternMatchType::kLiteral,
                               literal_pattern_filter2);

  ASSERT_FALSE(apps_util::FiltersHaveOverlap(literal_pattern_filter1,
                                             literal_pattern_filter2));

  auto root_prefix_filter = MakeFilter("https", "www.example.com", "/",
                                       apps::PatternMatchType::kPrefix);
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(root_prefix_filter,
                                            literal_pattern_filter1));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(root_prefix_filter,
                                            literal_pattern_filter2));

  auto bar_prefix_filter = MakeFilter("https", "www.example.com", "/bar",
                                      apps::PatternMatchType::kPrefix);
  ASSERT_FALSE(apps_util::FiltersHaveOverlap(bar_prefix_filter,
                                             literal_pattern_filter1));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(bar_prefix_filter,
                                            literal_pattern_filter2));
  ASSERT_TRUE(
      apps_util::FiltersHaveOverlap(bar_prefix_filter, root_prefix_filter));

  auto foo_prefix_filter = MakeFilter("https", "www.example.com", "/foo",
                                      apps::PatternMatchType::kPrefix);
  ASSERT_FALSE(
      apps_util::FiltersHaveOverlap(foo_prefix_filter, bar_prefix_filter));
}

TEST_F(IntentFilterUtilTest, PatternGlobAndLiteralOverlap) {
  auto literal_pattern_filter1 =
      MakeFilter("https", "maps.google.com", "/u/0/maps",
                 apps::PatternMatchType::kLiteral);
  auto literal_pattern_filter2 = MakeFilter("https", "maps.google.com", "/maps",
                                            apps::PatternMatchType::kLiteral);

  auto glob_pattern_filter = MakeFilter(
      "https", "maps.google.com", "/u/.*/maps", apps::PatternMatchType::kGlob);

  ASSERT_TRUE(apps_util::FiltersHaveOverlap(literal_pattern_filter1,
                                            glob_pattern_filter));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(glob_pattern_filter,
                                            literal_pattern_filter1));

  ASSERT_FALSE(apps_util::FiltersHaveOverlap(literal_pattern_filter2,
                                             glob_pattern_filter));
}

TEST_F(IntentFilterUtilTest, TestIntentFilterUrlMatchLength) {
  const auto kPrefix = apps::PatternMatchType::kPrefix;
  const auto kLiteral = apps::PatternMatchType::kLiteral;
  const auto kGlob = apps::PatternMatchType::kGlob;
  struct Test {
    std::string filter_url;
    std::string matched_url;
    apps::PatternMatchType pattern_match_type;
    size_t expected;
  };
  std::vector<Test> tests{
      {"https://prefix.a.com/a", "https://prefix.a.com/a", kPrefix, 22},
      {"https://prefix.a.com/a", "https://prefix.a.com/a?x=y", kPrefix, 22},
      {"https://prefix.a.com/a", "https://prefix.a.com/a/b", kPrefix, 22},
      {"https://prefix.a.com/", "https://prefix.a.com/", kPrefix, 21},
      {"https://prefix.a.com", "https://prefix.a.com", kPrefix, 21},
      {"https://prefix.a.com/a", "", kPrefix, 0},
      {"https://prefix.a.com/a/b", "https://prefix.a.com/a", kPrefix, 0},
      {"https://prefix.a.com/a", "https://prefix.a.com/", kPrefix, 0},
      {"https://prefix.a.com/a", "https://prefix.a.org/a", kPrefix, 0},
      {"https://prefix.a.com/a", "http://prefix.a.com/a", kPrefix, 0},

      {"https://exact.a.com/a", "https://exact.a.com/a", kLiteral, 21},
      {"https://exact.a.com/", "https://exact.a.com/", kLiteral, 20},
      {"https://exact.a.com", "https://exact.a.com", kLiteral, 20},
      {"https://exact.a.com/a", "https://exact.a.com/a/b", kLiteral, 0},
      {"https://exact.a.com/a/b", "https://exact.a.com/a", kLiteral, 0},
      {"https://exact.a.com/a", "https://exact.a.org/a", kLiteral, 0},
      {"https://exact.a.com/a", "http://exact.a.com/a", kLiteral, 0},

      // Glob is not supported.
      {"https://glob.a.com/a/.*", "https://glob.a.com/a", kGlob, 0},
      {"https://glob.a.com/a/.*", "https://glob.a.com/a/b", kGlob, 0},
      {"https://glob.a.com/a/.*/b", "https://glob.a.com/a/b", kGlob, 0},
  };
  for (size_t i = 0; i < tests.size(); ++i) {
    const auto& test = tests[i];
    GURL filter_url(test.filter_url);
    GURL matched_url(test.matched_url);
    auto filter = MakeFilter(filter_url.scheme(), filter_url.host(),
                             filter_url.path(), test.pattern_match_type);
    EXPECT_EQ(apps_util::IntentFilterUrlMatchLength(filter, matched_url),
              test.expected)
        << "Test #" << i << " url=" << test.matched_url
        << " filter=" << test.filter_url;
  }
}
