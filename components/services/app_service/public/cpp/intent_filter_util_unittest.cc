// Copyright 2020 The Chromium Authors. All rights reserved.
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
  apps::mojom::IntentFilterPtr MakeFilter(
      std::string scheme,
      std::string host,
      std::string path,
      apps::mojom::PatternMatchType pattern) {
    auto intent_filter = apps::mojom::IntentFilter::New();

    apps_util::AddSingleValueCondition(
        apps::mojom::ConditionType::kAction, apps_util::kIntentActionView,
        apps::mojom::PatternMatchType::kNone, intent_filter);

    apps_util::AddSingleValueCondition(
        apps::mojom::ConditionType::kScheme, scheme,
        apps::mojom::PatternMatchType::kNone, intent_filter);

    apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kHost, host,
                                       apps::mojom::PatternMatchType::kNone,
                                       intent_filter);

    apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kPattern,
                                       path, pattern, intent_filter);

    return intent_filter;
  }

  apps::mojom::IntentFilterPtr MakeHostOnlyFilter(
      std::string host,
      apps::mojom::PatternMatchType pattern) {
    auto intent_filter = apps::mojom::IntentFilter::New();

    apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kHost, host,
                                       pattern, intent_filter);

    return intent_filter;
  }
};

TEST_F(IntentFilterUtilTest, EmptyConditionList) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  EXPECT_EQ(apps_util::AppManagementGetSupportedLinks(intent_filter).size(),
            0u);
}

TEST_F(IntentFilterUtilTest, SingleHostAndManyPaths) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kHttpScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, kHostUrlGoogle,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 0u);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, kPathLiteral,
      apps::mojom::PatternMatchType::kLiteral, intent_filter);

  links = apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, kPathPrefix,
      apps::mojom::PatternMatchType::kPrefix, intent_filter);

  links = apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 2u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGooglePrefix), 1u);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, kPathGlob,
      apps::mojom::PatternMatchType::kGlob, intent_filter);

  links = apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 3u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGooglePrefix), 1u);
  EXPECT_EQ(links.count(kUrlGoogleGlob), 1u);
}

TEST_F(IntentFilterUtilTest, InvalidScheme) {
  auto intent_filter = MakeFilter(url::kTelScheme, kHostUrlGoogle, kPathLiteral,
                                  apps::mojom::PatternMatchType::kLiteral);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 0u);
}

TEST_F(IntentFilterUtilTest, ManyHostsAndOnePath) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kHttpScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::vector<apps::mojom::ConditionValuePtr> condition_values;

  condition_values.push_back(apps_util::MakeConditionValue(
      kHostUrlGoogle, apps::mojom::PatternMatchType::kNone));

  condition_values.push_back(apps_util::MakeConditionValue(
      kHostUrlGmail, apps::mojom::PatternMatchType::kNone));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kHost, std::move(condition_values)));

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, kPathLiteral,
      apps::mojom::PatternMatchType::kLiteral, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 2u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
  EXPECT_EQ(links.count(kUrlGmailLiteral), 1u);
}

TEST_F(IntentFilterUtilTest, ManyHostsAndManyPaths) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kHttpScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::vector<apps::mojom::ConditionValuePtr> host_condition_values;

  host_condition_values.push_back(apps_util::MakeConditionValue(
      kHostUrlGoogle, apps::mojom::PatternMatchType::kNone));
  host_condition_values.push_back(apps_util::MakeConditionValue(
      kHostUrlGmail, apps::mojom::PatternMatchType::kNone));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kHost, std::move(host_condition_values)));

  std::vector<apps::mojom::ConditionValuePtr> path_condition_values;

  path_condition_values.push_back(apps_util::MakeConditionValue(
      kPathLiteral, apps::mojom::PatternMatchType::kLiteral));
  path_condition_values.push_back(apps_util::MakeConditionValue(
      kPathPrefix, apps::mojom::PatternMatchType::kPrefix));
  path_condition_values.push_back(apps_util::MakeConditionValue(
      kPathGlob, apps::mojom::PatternMatchType::kGlob));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kPattern, std::move(path_condition_values)));

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

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
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kHttpScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kHost, host,
                                     apps::mojom::PatternMatchType::kSuffix,
                                     intent_filter);
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, kPathLiteral,
      apps::mojom::PatternMatchType::kLiteral, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count("*.google.com/a"), 1u);
}

TEST_F(IntentFilterUtilTest, HttpsScheme) {
  std::set<std::string> links = apps_util::AppManagementGetSupportedLinks(
      MakeFilter(url::kHttpsScheme, kHostUrlGoogle, kPathLiteral,
                 apps::mojom::PatternMatchType::kLiteral));

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
}

TEST_F(IntentFilterUtilTest, HttpAndHttpsSchemes) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> condition_values;

  condition_values.push_back(apps_util::MakeConditionValue(
      url::kHttpScheme, apps::mojom::PatternMatchType::kNone));

  condition_values.push_back(apps_util::MakeConditionValue(
      url::kHttpsScheme, apps::mojom::PatternMatchType::kNone));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kScheme, std::move(condition_values)));

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, kHostUrlGoogle,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, kPathLiteral,
      apps::mojom::PatternMatchType::kLiteral, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kUrlGoogleLiteral), 1u);
}

TEST_F(IntentFilterUtilTest, PathsWithNoSlash) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kHttpScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, "m.youtube.com",
      apps::mojom::PatternMatchType::kNone, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, ".*",
      apps::mojom::PatternMatchType::kGlob, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, ".*/foo",
      apps::mojom::PatternMatchType::kGlob, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kPattern, "",
      apps::mojom::PatternMatchType::kPrefix, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 3u);
  EXPECT_EQ(links.count("m.youtube.com/*"), 1u);
  EXPECT_EQ(links.count("m.youtube.com/.*"), 1u);
  EXPECT_EQ(links.count("m.youtube.com/.*/foo"), 1u);
}

TEST_F(IntentFilterUtilTest, IsSupportedLink) {
  auto filter = MakeFilter("https", "www.google.com", "/maps",
                           apps::mojom::PatternMatchType::kLiteral);
  ASSERT_TRUE(apps_util::IsSupportedLinkForApp(kAppId, filter));

  filter = MakeFilter("https", "www.google.com", ".*",
                      apps::mojom::PatternMatchType::kGlob);
  ASSERT_TRUE(apps_util::IsSupportedLinkForApp(kAppId, filter));
}

TEST_F(IntentFilterUtilTest, NotSupportedLink) {
  ASSERT_FALSE(apps_util::IsSupportedLinkForApp(
      kAppId, apps_util::CreateIntentFilterForMimeType("image/png")));

  auto browser_filter = apps::mojom::IntentFilter::New();
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kAction, apps_util::kIntentActionView,
      apps::mojom::PatternMatchType::kNone, browser_filter);
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, "https",
      apps::mojom::PatternMatchType::kNone, browser_filter);
  ASSERT_FALSE(apps_util::IsSupportedLinkForApp(kAppId, browser_filter));

  auto host_filter = apps::mojom::IntentFilter::New();
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kAction, apps_util::kIntentActionView,
      apps::mojom::PatternMatchType::kNone, host_filter);
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, "https",
      apps::mojom::PatternMatchType::kNone, host_filter);
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, "www.example.com",
      apps::mojom::PatternMatchType::kNone, host_filter);
  ASSERT_FALSE(apps_util::IsSupportedLinkForApp(kAppId, browser_filter));
}

TEST_F(IntentFilterUtilTest, HostMatchOverlapLiteralAndNone) {
  auto google_domain_filter = MakeFilter(
      "https", "www.google.com", "/", apps::mojom::PatternMatchType::kLiteral);

  auto maps_domain_filter = MakeFilter("https", "maps.google.com", "/",
                                       apps::mojom::PatternMatchType::kLiteral);

  ASSERT_FALSE(
      apps_util::FiltersHaveOverlap(maps_domain_filter, google_domain_filter));

  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kHost, "www.google.com",
      apps::mojom::PatternMatchType::kNone, maps_domain_filter);

  ASSERT_TRUE(
      apps_util::FiltersHaveOverlap(maps_domain_filter, google_domain_filter));
}

TEST_F(IntentFilterUtilTest, HostMatchOverlapSuffix) {
  // Wildcard host filter
  auto wikipedia_wildcard_filter = MakeHostOnlyFilter(
      ".wikipedia.org", apps::mojom::PatternMatchType::kSuffix);

  // Filters that shouldn't overlap
  auto wikipedia_com_filter = MakeHostOnlyFilter(
      ".wikipedia.com", apps::mojom::PatternMatchType::kNone);
  auto wikipedia_no_subdomain_filter =
      MakeHostOnlyFilter("wikipedia.org", apps::mojom::PatternMatchType::kNone);

  ASSERT_FALSE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                             wikipedia_com_filter));
  ASSERT_FALSE(apps_util::FiltersHaveOverlap(wikipedia_wildcard_filter,
                                             wikipedia_no_subdomain_filter));

  // Filters that should overlap
  auto wikipedia_subdomain_filter = MakeHostOnlyFilter(
      "es.wikipedia.org", apps::mojom::PatternMatchType::kNone);
  auto wikipedia_empty_subdomain_filter = MakeHostOnlyFilter(
      ".wikipedia.org", apps::mojom::PatternMatchType::kNone);
  auto wikipedia_literal_filter = MakeHostOnlyFilter(
      "fr.wikipedia.org", apps::mojom::PatternMatchType::kLiteral);
  auto wikipedia_other_wildcard_filter = MakeHostOnlyFilter(
      ".wikipedia.org", apps::mojom::PatternMatchType::kSuffix);
  auto wikipedia_subsubdomain_filter = MakeHostOnlyFilter(
      ".es.wikipedia.org", apps::mojom::PatternMatchType::kSuffix);

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
  auto literal_pattern_filter1 = MakeFilter(
      "https", "www.example.com", "/", apps::mojom::PatternMatchType::kLiteral);
  apps_util::AddConditionValue(apps::mojom::ConditionType::kPattern, "/foo",
                               apps::mojom::PatternMatchType::kLiteral,
                               literal_pattern_filter1);

  auto literal_pattern_filter2 =
      MakeFilter("https", "www.example.com", "/foo/bar",
                 apps::mojom::PatternMatchType::kLiteral);
  apps_util::AddConditionValue(apps::mojom::ConditionType::kPattern, "/bar",
                               apps::mojom::PatternMatchType::kLiteral,
                               literal_pattern_filter2);

  ASSERT_FALSE(apps_util::FiltersHaveOverlap(literal_pattern_filter1,
                                             literal_pattern_filter2));

  auto root_prefix_filter = MakeFilter("https", "www.example.com", "/",
                                       apps::mojom::PatternMatchType::kPrefix);
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(root_prefix_filter,
                                            literal_pattern_filter1));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(root_prefix_filter,
                                            literal_pattern_filter2));

  auto bar_prefix_filter = MakeFilter("https", "www.example.com", "/bar",
                                      apps::mojom::PatternMatchType::kPrefix);
  ASSERT_FALSE(apps_util::FiltersHaveOverlap(bar_prefix_filter,
                                             literal_pattern_filter1));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(bar_prefix_filter,
                                            literal_pattern_filter2));
  ASSERT_TRUE(
      apps_util::FiltersHaveOverlap(bar_prefix_filter, root_prefix_filter));

  auto foo_prefix_filter = MakeFilter("https", "www.example.com", "/foo",
                                      apps::mojom::PatternMatchType::kPrefix);
  ASSERT_FALSE(
      apps_util::FiltersHaveOverlap(foo_prefix_filter, bar_prefix_filter));
}

TEST_F(IntentFilterUtilTest, PatternGlobAndLiteralOverlap) {
  auto literal_pattern_filter1 =
      MakeFilter("https", "maps.google.com", "/u/0/maps",
                 apps::mojom::PatternMatchType::kLiteral);
  auto literal_pattern_filter2 =
      MakeFilter("https", "maps.google.com", "/maps",
                 apps::mojom::PatternMatchType::kLiteral);

  auto glob_pattern_filter =
      MakeFilter("https", "maps.google.com", "/u/.*/maps",
                 apps::mojom::PatternMatchType::kGlob);

  ASSERT_TRUE(apps_util::FiltersHaveOverlap(literal_pattern_filter1,
                                            glob_pattern_filter));
  ASSERT_TRUE(apps_util::FiltersHaveOverlap(glob_pattern_filter,
                                            literal_pattern_filter1));

  ASSERT_FALSE(apps_util::FiltersHaveOverlap(literal_pattern_filter2,
                                             glob_pattern_filter));
}

TEST_F(IntentFilterUtilTest, IntentFiltersConvert) {
  base::flat_map<std::string, std::vector<apps::IntentFilterPtr>> filters;

  auto intent_filter1 = std::make_unique<apps::IntentFilter>();
  apps_util::AddSingleValueCondition(apps::ConditionType::kScheme, "1",
                                     apps::PatternMatchType::kNone,
                                     intent_filter1);
  filters["1"].push_back(std::move(intent_filter1));

  auto intent_filter2 = std::make_unique<apps::IntentFilter>();
  apps_util::AddSingleValueCondition(apps::ConditionType::kHost, "2",
                                     apps::PatternMatchType::kLiteral,
                                     intent_filter2);
  apps_util::AddSingleValueCondition(apps::ConditionType::kPattern, "3",
                                     apps::PatternMatchType::kPrefix,
                                     intent_filter2);
  filters["1"].push_back(std::move(intent_filter2));

  apps::IntentFilters intent_filters2;
  auto intent_filter3 = std::make_unique<apps::IntentFilter>();
  apps_util::AddSingleValueCondition(apps::ConditionType::kAction, "4",
                                     apps::PatternMatchType::kGlob,
                                     intent_filter3);
  apps_util::AddSingleValueCondition(apps::ConditionType::kMimeType, "5",
                                     apps::PatternMatchType::kMimeType,
                                     intent_filter3);
  filters["2"].push_back(std::move(intent_filter3));

  auto intent_filter4 = std::make_unique<apps::IntentFilter>();
  apps_util::AddSingleValueCondition(apps::ConditionType::kFile, "6",
                                     apps::PatternMatchType::kMimeType,
                                     intent_filter4);
  apps_util::AddSingleValueCondition(apps::ConditionType::kFile, "7",
                                     apps::PatternMatchType::kFileExtension,
                                     intent_filter4);
  filters["2"].push_back(std::move(intent_filter4));

  auto output = apps::ConvertMojomIntentFiltersToIntentFilters(
      apps::ConvertIntentFiltersToMojomIntentFilters(filters));

  ASSERT_EQ(output.size(), 2U);
  EXPECT_EQ(*filters["1"][0], *output["1"][0]);
  EXPECT_EQ(*filters["1"][1], *output["1"][1]);

  EXPECT_EQ(*filters["2"][0], *output["2"][0]);
  EXPECT_EQ(*filters["2"][1], *output["2"][1]);

  {
    auto& condition = output["1"][0]->conditions[0];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kScheme);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kNone);
    EXPECT_EQ(condition->condition_values[0]->value, "1");
  }
  {
    auto& condition = output["1"][1]->conditions[0];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kHost);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition->condition_values[0]->value, "2");
  }
  {
    auto& condition = output["1"][1]->conditions[1];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kPattern);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kPrefix);
    EXPECT_EQ(condition->condition_values[0]->value, "3");
  }
  {
    auto& condition = output["2"][0]->conditions[0];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kAction);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kGlob);
    EXPECT_EQ(condition->condition_values[0]->value, "4");
  }
  {
    auto& condition = output["2"][0]->conditions[1];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kMimeType);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[0]->value, "5");
  }
  {
    auto& condition = output["2"][1]->conditions[0];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[0]->value, "6");
  }
  {
    auto& condition = output["2"][1]->conditions[1];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kFileExtension);
    EXPECT_EQ(condition->condition_values[0]->value, "7");
  }
}
