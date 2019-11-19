// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/preferred_apps.h"

#include "base/values.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/cpp/intent_test_util.h"
#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAppId1[] = "abcdefg";
const char kAppId2[] = "gfedcba";
const char kAppId3[] = "hahahahaha";
const char kAppIdKey[] = "app_id";

}  // namespace

class PreferredAppTest : public testing::Test {
 protected:
  apps::mojom::IntentFilterPtr CreatePatternFilter(
      const std::string& pattern,
      apps::mojom::PatternMatchType match_type) {
    auto intent_filter =
        apps_util::CreateSchemeAndHostOnlyFilter("https", "www.google.com");
    auto pattern_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kPattern,
                                 std::vector<apps::mojom::ConditionValuePtr>());
    intent_filter->conditions.push_back(std::move(pattern_condition));
    auto condition_value = apps_util::MakeConditionValue(pattern, match_type);
    intent_filter->conditions[2]->condition_values.push_back(
        std::move(condition_value));
    return intent_filter;
  }

  apps::PreferredApps preferred_apps_;
};

// Test that for a single preferred app with URL filter, we can add
// and find (or not find) the correct preferred app id for different
// URLs.
TEST_F(PreferredAppTest, AddPreferredAppForURL) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  GURL url_in_scope = GURL("https://www.google.com/abcde");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_wrong_scheme = GURL("tel://www.google.com/");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));

  GURL url_wrong_host = GURL("https://www.hahaha.com/");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_host));

  GURL url_not_in_scope = GURL("https://www.google.com/a");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for preferred app with filter that does not have all condition
// types. E.g. add preferred app with intent filter that only have scheme.
TEST_F(PreferredAppTest, TopLayerFilters) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  auto intent_filter = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_not_in_scope = GURL("http://www.google.com");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for multiple preferred app setting with different number of condition
// types.
TEST_F(PreferredAppTest, MixLayerFilters) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  auto intent_filter_scheme = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_scheme);

  auto intent_filter_scheme_host =
      apps_util::CreateSchemeAndHostOnlyFilter("http", "www.abc.com");
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_scheme_host);

  auto intent_filter_url =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_url);

  GURL url_1 = GURL("tel://1234556/");
  GURL url_2 = GURL("http://www.abc.com/");
  GURL url_3 = GURL("https://www.google.com/");
  GURL url_out_scope = GURL("https://www.abc.com/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that when there are multiple preferred apps for one intent, the best
// matching one will be picked.
TEST_F(PreferredAppTest, MultiplePreferredApps) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  GURL url = GURL("https://www.google.com/");

  auto intent_filter_scheme = apps_util::CreateSchemeOnlyFilter("https");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_scheme);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url));

  auto intent_filter_scheme_host =
      apps_util::CreateSchemeAndHostOnlyFilter("https", "www.google.com");
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_scheme_host);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url));

  auto intent_filter_url =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_url);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url));
}

// Test that we can properly add and search for filters that has multiple
// condition values for a condition type.
TEST_F(PreferredAppTest, MultipleConditionValues) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  intent_filter->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue("http",
                                    apps::mojom::PatternMatchType::kNone));

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  GURL url_http_out_of_scope = GURL("http://www.abc.com/");
  GURL url_wrong_scheme = GURL("tel://1234567/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_http_out_of_scope));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));
}

// Test for more than one pattern available, we can find the correct match.
TEST_F(PreferredAppTest, DifferentPatterns) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  auto intent_filter_literal =
      CreatePatternFilter("/bc", apps::mojom::PatternMatchType::kLiteral);
  auto intent_filter_prefix =
      CreatePatternFilter("/a", apps::mojom::PatternMatchType::kPrefix);
  auto intent_filter_glob =
      CreatePatternFilter("/c.*d", apps::mojom::PatternMatchType::kGlob);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_literal);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_prefix);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_glob);

  GURL url_1 = GURL("https://www.google.com/bc");
  GURL url_2 = GURL("https://www.google.com/abbb");
  GURL url_3 = GURL("https://www.google.com/ccccccd");
  GURL url_out_scope = GURL("https://www.google.com/dfg");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that for same intent filter, the app id will overwrite the old setting.
TEST_F(PreferredAppTest, OverwritePreferredApp) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.AddPreferredApp(kAppId2, intent_filter);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test the preferred apps dictionary structure is as expected.
TEST_F(PreferredAppTest, VerifyPreferredApps) {
  base::Value preferred_app = base::DictionaryValue();

  EXPECT_TRUE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));

  auto* abc_dict = preferred_app.SetKey("abc", base::DictionaryValue());
  auto* efg_dict = preferred_app.SetKey("efg", base::DictionaryValue());
  auto* hij_dict = abc_dict->SetKey("hij", base::DictionaryValue());
  hij_dict->SetStringKey(kAppIdKey, kAppId1);

  EXPECT_TRUE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));

  // Test if there is a app id key with non string value.
  efg_dict->SetKey(kAppIdKey, base::DictionaryValue());

  EXPECT_FALSE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));

  efg_dict->SetStringKey(kAppIdKey, kAppId2);

  EXPECT_TRUE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));

  efg_dict->RemoveKey(kAppIdKey);

  efg_dict->SetKey("abced", base::DictionaryValue());

  EXPECT_TRUE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));

  // Test if there is a non app id key with string value.
  efg_dict->SetStringKey("abced", "fgijk");

  EXPECT_FALSE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));

  // Test having other value types in the dictionary.
  efg_dict->RemoveKey("abced");

  efg_dict->SetKey("klm", base::ListValue());
  EXPECT_FALSE(apps::PreferredApps::VerifyPreferredApps(&preferred_app));
}

// Test that for a single preferred app with URL filter, we can delete
// the preferred app id.
TEST_F(PreferredAppTest, DeletePreferredAppForURL) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  // If try to delete with wrong ID, won't delete.
  preferred_apps_.DeletePreferredApp(kAppId2, intent_filter);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url));
  EXPECT_TRUE(preferred_apps_.GetValue().DictEmpty());
}

// Test for preferred app with filter that does not have all condition
// types. E.g. delete preferred app with intent filter that only have scheme.
TEST_F(PreferredAppTest, DeleteForTopLayerFilters) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  auto intent_filter = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_in_scope));
}

// Test that we can properly delete for filters that has multiple
// condition values for a condition type.
TEST_F(PreferredAppTest, DeleteMultipleConditionValues) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  intent_filter->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue("http",
                                    apps::mojom::PatternMatchType::kNone));

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_http));
}

// Test for more than one pattern available, we can delete the filter.
TEST_F(PreferredAppTest, DeleteDifferentPatterns) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  auto intent_filter_literal =
      CreatePatternFilter("/bc", apps::mojom::PatternMatchType::kLiteral);
  auto intent_filter_prefix =
      CreatePatternFilter("/a", apps::mojom::PatternMatchType::kPrefix);
  auto intent_filter_glob =
      CreatePatternFilter("/c.*d", apps::mojom::PatternMatchType::kGlob);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_literal);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_prefix);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_glob);

  GURL url_1 = GURL("https://www.google.com/bc");
  GURL url_2 = GURL("https://www.google.com/abbb");
  GURL url_3 = GURL("https://www.google.com/ccccccd");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter_literal);
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  preferred_apps_.DeletePreferredApp(kAppId2, intent_filter_prefix);
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  preferred_apps_.DeletePreferredApp(kAppId3, intent_filter_glob);
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_3));
}

// Test that can delete properly for super set filters. E.g. the filter
// to delete has more condition values compare with filter that was set.
TEST_F(PreferredAppTest, DeleteForNotCompletedFilter) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  auto intent_filter_set =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));

  auto intent_filter_to_delete =
      apps_util::CreateIntentFilterForUrlScope(GURL("http://www.google.com/"));
  intent_filter_to_delete->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue("https",
                                    apps::mojom::PatternMatchType::kNone));

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_set);

  GURL url = GURL("https://www.google.com/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter_to_delete);

  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url));
}

// Test that DeleteAppId() can delete the setting for one filter.
TEST_F(PreferredAppTest, DeleteAppIdForOneFilter) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.DeleteAppId(kAppId1);

  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url));
  EXPECT_TRUE(preferred_apps_.GetValue().DictEmpty());
}

// Test that when multiple filters set to the same app id, DeleteAppId() can
// delete all of them.
TEST_F(PreferredAppTest, DeleteAppIdForMultipleFilters) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.abc.com/google");
  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_2);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("tel://12345678/");
  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_3);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  preferred_apps_.DeleteAppId(kAppId1);

  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(preferred_apps_.GetValue().DictEmpty());
}

// Test that for filter with multiple condition values, DeleteAppId() can
// delete them all.
TEST_F(PreferredAppTest, DeleteAppIdForMultipleConditionValues) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  intent_filter->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue("http",
                                    apps::mojom::PatternMatchType::kNone));

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));

  preferred_apps_.DeleteAppId(kAppId1);
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(base::nullopt, preferred_apps_.FindPreferredAppForUrl(url_http));
}

// Test that for multiple filters set to different app ids, DeleteAppId() only
// deletes the correct app id.
TEST_F(PreferredAppTest, DeleteAppIdForMultipleAppIds) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY));
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.abc.com/google");
  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("tel://12345678/");
  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_3);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  preferred_apps_.DeleteAppId(kAppId1);
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  preferred_apps_.DeleteAppId(kAppId2);
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  preferred_apps_.DeleteAppId(kAppId3);
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  EXPECT_TRUE(preferred_apps_.GetValue().DictEmpty());
}
