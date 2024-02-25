// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_converter.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAppId1[] = "abcdefg";

}  // namespace

class PreferredAppsConverterTest : public testing::Test {};

// Test one simple entry with simple filter.
TEST_F(PreferredAppsConverterTest, ConvertSimpleEntry) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);

  apps::PreferredAppsList preferred_apps;
  preferred_apps.Init();
  preferred_apps.AddPreferredApp(kAppId1, intent_filter);
  auto converted_value =
      apps::ConvertPreferredAppsToValue(preferred_apps.GetReference());

  auto* converted_preferred_apps =
      converted_value.GetDict().Find(apps::kPreferredAppsKey);
  // Check that each entry is correct.
  ASSERT_EQ(1u, converted_preferred_apps->GetList().size());
  const base::Value& entry_val = converted_preferred_apps->GetList()[0];
  const base::Value::Dict& entry = entry_val.GetDict();
  EXPECT_EQ(kAppId1, *entry.FindString(apps::kAppIdKey));

  const base::Value::List* converted_intent_filter =
      entry.FindList(apps::kIntentFilterKey);
  ASSERT_EQ(intent_filter->conditions.size(), converted_intent_filter->size());

  for (size_t i = 0; i < intent_filter->conditions.size(); i++) {
    auto& condition = intent_filter->conditions[i];
    const base::Value::Dict& converted_condition =
        (*converted_intent_filter)[i].GetDict();
    auto& condition_values = condition->condition_values;
    const base::Value::List* converted_condition_values =
        converted_condition.FindList(apps_util::kConditionValuesKey);

    EXPECT_EQ(static_cast<int>(condition->condition_type),
              converted_condition.FindInt(apps_util::kConditionTypeKey));
    ASSERT_EQ(1u, converted_condition_values->size());
    EXPECT_EQ(condition_values[0]->value,
              *(*converted_condition_values)[0].GetDict().FindString(
                  apps_util::kValueKey));
    EXPECT_EQ(static_cast<int>(condition_values[0]->match_type),
              (*converted_condition_values)[0].GetDict().FindInt(
                  apps_util::kMatchTypeKey));
  }

  preferred_apps.Init();
  EXPECT_EQ(std::nullopt, preferred_apps.FindPreferredAppForUrl(filter_url));
  preferred_apps.Init(apps::ParseValueToPreferredApps(converted_value));
  EXPECT_EQ(kAppId1, preferred_apps.FindPreferredAppForUrl(filter_url));
  GURL url_wrong_host = GURL("https://www.hahaha.com/");
  EXPECT_EQ(std::nullopt,
            preferred_apps.FindPreferredAppForUrl(url_wrong_host));
}

// Test one upgraded simple entry with json string.
TEST_F(PreferredAppsConverterTest, ConvertUpgradedSimpleEntryJson) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);

  apps::PreferredAppsList preferred_apps;
  preferred_apps.Init();
  preferred_apps.AddPreferredApp(kAppId1, intent_filter);
  auto converted_value =
      apps::ConvertPreferredAppsToValue(preferred_apps.GetReference());

  const char expected_output_string[] =
      "{\"preferred_apps\": [ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 3,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 1,"
      "         \"value\": \"view\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 1,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 1,"
      "         \"value\": \"www.google.com:443\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ],"
      "\"version\": 1}";
  std::optional<base::Value> expected_output =
      base::JSONReader::Read(expected_output_string);
  ASSERT_TRUE(expected_output);
  EXPECT_EQ(expected_output.value(), converted_value);

  // Make sure this round trips.
  EXPECT_TRUE(IsEqual(apps::ParseValueToPreferredApps(converted_value),
                      preferred_apps.GetReference()));
}

// Test parse simple entry from json string (old format).
TEST_F(PreferredAppsConverterTest, ParseSimpleEntryJson) {
  const char test_string[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_FALSE(apps::IsUpgradedForSharing(test_value.value()));

  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(
      filter_url, /*omit_port_for_testing=*/true);
  intent_filter->conditions.erase(intent_filter->conditions.begin());
  apps::PreferredAppsList preferred_apps;
  preferred_apps.Init();
  preferred_apps.AddPreferredApp(kAppId1, intent_filter);
  auto& expected_entry = preferred_apps.GetReference();

  EXPECT_TRUE(IsEqual(expected_entry, parsed_entry));
}

// Test parse simple entry from json string (upgraded for sharing).
TEST_F(PreferredAppsConverterTest, ParseUpgradedSimpleEntryJson) {
  const char test_string[] =
      "{\"preferred_apps\": [ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 3,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"view\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com:443\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ],"
      "\"version\": 1}";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(apps::IsUpgradedForSharing(test_value.value()));

  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);

  apps::PreferredAppsList preferred_apps;
  preferred_apps.Init();
  preferred_apps.AddPreferredApp(kAppId1, intent_filter);
  auto& expected_entry = preferred_apps.GetReference();

  EXPECT_TRUE(IsEqual(expected_entry, parsed_entry));
}

// Test parsing an entry from before URL scopes had port numbers.
TEST_F(PreferredAppsConverterTest, ParseEmptyPortEntryJson) {
  const char test_string[] =
      "{\"preferred_apps\": [ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 3,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"view\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ],"
      "\"version\": 1}";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());

  apps::PreferredAppsList preferred_apps_list;
  preferred_apps_list.Init();
  preferred_apps_list.AddPreferredApp(
      kAppId1,
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.google.com/abc"),
                                             /*omit_port_for_testing=*/true));

  EXPECT_TRUE(IsEqual(parsed_entry, preferred_apps_list.GetReference()));
}

TEST_F(PreferredAppsConverterTest, ParseJsonWithInvalidAppId) {
  // Invalid key.
  const char test_key[] =
      "[ {\"app_idd\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_key);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());

  // Invalid value.
  const char test_string[] =
      "[ {\"app_id\": 0,"
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());
}

TEST_F(PreferredAppsConverterTest, ParseJsonWithInvalidIntentFilter) {
  // Invalid key.
  const char test_key[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filterrr\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_key);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());

  // Invalid value.
  const char test_string[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": \"not_list\""
      "} ]";
  test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());
}

TEST_F(PreferredAppsConverterTest, ParseJsonWithInvalidConditionType) {
  // Invalid key.
  const char test_key[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_typeeee\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_key);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());

  // Invalid value.
  const char test_string[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": \"not_int\","
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());
}

TEST_F(PreferredAppsConverterTest, ParseJsonWithInvalidValues) {
  // Invalid key.
  const char test_key[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_valuessss\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_key);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());

  // Invalid value.
  const char test_string[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": \"not_list\""
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());
}

TEST_F(PreferredAppsConverterTest, ParseJsonWithInvalidMatchType) {
  // Invalid key.
  const char test_key[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_typeeeee\": 0,"
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_key);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());

  // Invalid value.
  const char test_string[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": \"not_int\","
      "         \"value\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());
}

TEST_F(PreferredAppsConverterTest, ParseJsonWithInvalidValue) {
  // Invalid key.
  const char test_key[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"valueeeee\": \"https\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  std::optional<base::Value> test_value = base::JSONReader::Read(test_key);
  ASSERT_TRUE(test_value);
  auto parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());

  // Invalid value.
  const char test_string[] =
      "[ {\"app_id\": \"abcdefg\","
      "   \"intent_filter\": [ {"
      "      \"condition_type\": 0,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": {}"
      "      } ]"
      "   }, {"
      "      \"condition_type\": 1,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 0,"
      "         \"value\": \"www.google.com\""
      "      } ]"
      "   }, {"
      "      \"condition_type\": 2,"
      "      \"condition_values\": [ {"
      "         \"match_type\": 2,"
      "         \"value\": \"/abc\""
      "      } ]"
      "   } ]"
      "} ]";
  test_value = base::JSONReader::Read(test_string);
  ASSERT_TRUE(test_value);
  parsed_entry = apps::ParseValueToPreferredApps(test_value.value());
  EXPECT_TRUE(parsed_entry.empty());
}

TEST_F(PreferredAppsConverterTest, UpgradePreferredApp) {
  // Create preferred app with old filter.
  GURL filter_url = GURL("https://www.google.com/abc");
  auto old_intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);

  apps::PreferredAppsList old_preferred_apps;
  old_preferred_apps.Init();
  old_preferred_apps.AddPreferredApp(kAppId1, old_intent_filter);

  auto new_intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);

  apps::PreferredAppsList new_preferred_apps;
  new_preferred_apps.Init();
  new_preferred_apps.AddPreferredApp(kAppId1, new_intent_filter);

  auto old_preferred_apps_value = old_preferred_apps.GetValue();
  apps::UpgradePreferredApps(old_preferred_apps_value);
  EXPECT_TRUE(
      IsEqual(old_preferred_apps_value, new_preferred_apps.GetReference()));
}
