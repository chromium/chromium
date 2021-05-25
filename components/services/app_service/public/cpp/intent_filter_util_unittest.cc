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
}  // namespace

class IntentFilterUtilTest : public testing::Test {};

TEST_F(IntentFilterUtilTest, EmptyConditionList) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  EXPECT_EQ(apps_util::AppManagementGetSupportedLinks(intent_filter).size(),
            0u);
}

TEST_F(IntentFilterUtilTest, SingleSchemeAndHost) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kHttpScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, kHostUrlGoogle,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kHostUrlGoogle), 1u);
}

TEST_F(IntentFilterUtilTest, InvalidScheme) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, url::kTelScheme,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, kHostUrlGoogle,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 0u);
}

TEST_F(IntentFilterUtilTest, MultipleValidSchemeAndOneHost) {
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

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kHostUrlGoogle), 1u);
}

TEST_F(IntentFilterUtilTest, ValidAndInvalidSchemeAndOneHost) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> condition_values;

  condition_values.push_back(apps_util::MakeConditionValue(
      url::kHttpScheme, apps::mojom::PatternMatchType::kNone));

  condition_values.push_back(apps_util::MakeConditionValue(
      url::kTelScheme, apps::mojom::PatternMatchType::kNone));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kScheme, std::move(condition_values)));

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kHost, kHostUrlGoogle,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 1u);
  EXPECT_EQ(links.count(kHostUrlGoogle), 1u);
}

TEST_F(IntentFilterUtilTest, OneSchemeAndMultipleHosts) {
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

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 2u);
  EXPECT_EQ(links.count(kHostUrlGoogle), 1u);
  EXPECT_EQ(links.count(kHostUrlGmail), 1u);
}

TEST_F(IntentFilterUtilTest, MultipleSchemesAndMultipleHosts) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> condition_values_scheme;

  condition_values_scheme.push_back(apps_util::MakeConditionValue(
      url::kHttpScheme, apps::mojom::PatternMatchType::kNone));

  condition_values_scheme.push_back(apps_util::MakeConditionValue(
      url::kHttpsScheme, apps::mojom::PatternMatchType::kNone));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kScheme, std::move(condition_values_scheme)));

  std::vector<apps::mojom::ConditionValuePtr> condition_values_host;

  condition_values_host.push_back(apps_util::MakeConditionValue(
      kHostUrlGoogle, apps::mojom::PatternMatchType::kNone));

  condition_values_host.push_back(apps_util::MakeConditionValue(
      kHostUrlGmail, apps::mojom::PatternMatchType::kNone));

  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kHost, std::move(condition_values_host)));

  std::set<std::string> links =
      apps_util::AppManagementGetSupportedLinks(intent_filter);

  EXPECT_EQ(links.size(), 2u);
  EXPECT_EQ(links.count(kHostUrlGoogle), 1u);
  EXPECT_EQ(links.count(kHostUrlGmail), 1u);
}
