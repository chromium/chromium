// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_filter_util.h"

#include "base/values.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
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
        apps::mojom::ConditionType::kScheme, scheme,
        apps::mojom::PatternMatchType::kNone, intent_filter);

    apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kHost, host,
                                       apps::mojom::PatternMatchType::kNone,
                                       intent_filter);

    apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kPattern,
                                       path, pattern, intent_filter);

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
