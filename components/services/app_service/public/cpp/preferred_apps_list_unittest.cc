// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list.h"

#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAppId1[] = "abcdefg";
const char kAppId2[] = "gfedcba";
const char kAppId3[] = "hahahahaha";

}  // namespace

class PreferredAppListTest : public testing::Test {
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

  apps::PreferredAppsList preferred_apps_;
};

// Test that for a single preferred app with URL filter, we can add
// and find (or not find) the correct preferred app id for different
// URLs.
TEST_F(PreferredAppListTest, AddPreferredAppForURL) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  GURL url_in_scope = GURL("https://www.google.com/abcde");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_wrong_scheme = GURL("tel://www.google.com/");
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));

  GURL url_wrong_host = GURL("https://www.hahaha.com/");
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_host));

  GURL url_not_in_scope = GURL("https://www.google.com/a");
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for preferred app with filter that does not have all condition
// types. E.g. add preferred app with intent filter that only have scheme.
TEST_F(PreferredAppListTest, TopLayerFilters) {
  auto intent_filter = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_not_in_scope = GURL("http://www.google.com");
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for multiple preferred app setting with different number of condition
// types.
TEST_F(PreferredAppListTest, MixLayerFilters) {
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
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that when there are multiple preferred apps for one intent, the best
// matching one will be picked.
TEST_F(PreferredAppListTest, MultiplePreferredApps) {
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
TEST_F(PreferredAppListTest, MultipleConditionValues) {
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
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_http_out_of_scope));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));
}

// Test for more than one pattern available, we can find the correct match.
TEST_F(PreferredAppListTest, DifferentPatterns) {
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
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that for same intent filter, the app id will overwrite the old setting.
TEST_F(PreferredAppListTest, OverwritePreferredApp) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.AddPreferredApp(kAppId2, intent_filter);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test that when overlap happens, the previous setting will be removed.
TEST_F(PreferredAppListTest, OverlapPreferredApp) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  intent_filter_1->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_1->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.host(),
                                    apps::mojom::PatternMatchType::kNone));
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  intent_filter_2->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_2->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.host(),
                                    apps::mojom::PatternMatchType::kNone));
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
}

// Test that the replaced app preferences is correct.
TEST_F(PreferredAppListTest, ReplacedAppPreference) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  intent_filter_1->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_1->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.host(),
                                    apps::mojom::PatternMatchType::kNone));
  auto replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(0u, replaced_app_preferences->replaced_preference.size());

  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  intent_filter_2->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_2->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.host(),
                                    apps::mojom::PatternMatchType::kNone));
  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);
  EXPECT_EQ(1u, replaced_app_preferences->replaced_preference.size());
  EXPECT_TRUE(replaced_app_preferences->replaced_preference.find(kAppId1) !=
              replaced_app_preferences->replaced_preference.end());

  GURL filter_url_4 = GURL("http://www.example.com/abc");
  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  intent_filter_3->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_4.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_3->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_4.host(),
                                    apps::mojom::PatternMatchType::kNone));

  // Test when replacing multiple preferred app entries with same app id.
  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(1u, replaced_app_preferences->replaced_preference.size());
  EXPECT_TRUE(replaced_app_preferences->replaced_preference.find(kAppId2) !=
              replaced_app_preferences->replaced_preference.end());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_3);
  EXPECT_EQ(0u, replaced_app_preferences->replaced_preference.size());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);
  EXPECT_EQ(1u, replaced_app_preferences->replaced_preference.size());
  auto entry = replaced_app_preferences->replaced_preference.find(kAppId1);
  EXPECT_TRUE(entry != replaced_app_preferences->replaced_preference.end());
  EXPECT_EQ(2u, entry->second.size());

  // Test when replacing multiple preferred app entries with different app id.
  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(1u, replaced_app_preferences->replaced_preference.size());
  EXPECT_TRUE(replaced_app_preferences->replaced_preference.find(kAppId2) !=
              replaced_app_preferences->replaced_preference.end());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId2, intent_filter_3);
  EXPECT_EQ(0u, replaced_app_preferences->replaced_preference.size());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId3, intent_filter_2);
  EXPECT_EQ(2u, replaced_app_preferences->replaced_preference.size());
  entry = replaced_app_preferences->replaced_preference.find(kAppId1);
  EXPECT_TRUE(entry != replaced_app_preferences->replaced_preference.end());
  EXPECT_EQ(1u, entry->second.size());
  entry = replaced_app_preferences->replaced_preference.find(kAppId2);
  EXPECT_TRUE(entry != replaced_app_preferences->replaced_preference.end());
  EXPECT_EQ(1u, entry->second.size());
}

// Test that for a single preferred app with URL filter, we can delete
// the preferred app id.
TEST_F(PreferredAppListTest, DeletePreferredAppForURL) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  // If try to delete with wrong ID, won't delete.
  preferred_apps_.DeletePreferredApp(kAppId2, intent_filter);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test for preferred app with filter that does not have all condition
// types. E.g. delete preferred app with intent filter that only have scheme.
TEST_F(PreferredAppListTest, DeleteForTopLayerFilters) {
  auto intent_filter = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_in_scope));
}

// Test that we can properly delete for filters that has multiple
// condition values for a condition type.
TEST_F(PreferredAppListTest, DeleteMultipleConditionValues) {
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
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_http));
}

// Test for more than one pattern available, we can delete the filter.
TEST_F(PreferredAppListTest, DeleteDifferentPatterns) {
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
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  preferred_apps_.DeletePreferredApp(kAppId2, intent_filter_prefix);
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  preferred_apps_.DeletePreferredApp(kAppId3, intent_filter_glob);
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_3));
}

// Test that can delete properly for super set filters. E.g. the filter
// to delete has more condition values compare with filter that was set.
TEST_F(PreferredAppListTest, DeleteForNotCompletedFilter) {
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

  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url));
}

// Test that when there are more than one entry has overlap filter.
TEST_F(PreferredAppListTest, DeleteOverlapFilters) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  GURL filter_url_4 = GURL("http://www.example.com/abc");

  // Filter 1 handles url 1 and 2.
  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  intent_filter_1->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_1->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.host(),
                                    apps::mojom::PatternMatchType::kNone));

  // Filter 2 handles url 2 and 3.
  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  intent_filter_2->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_2->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_2.host(),
                                    apps::mojom::PatternMatchType::kNone));

  // Filter 3 handles url 3 and 4.
  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  intent_filter_3->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_4.scheme(),
                                    apps::mojom::PatternMatchType::kNone));
  intent_filter_3->conditions[1]->condition_values.push_back(
      apps_util::MakeConditionValue(filter_url_4.host(),
                                    apps::mojom::PatternMatchType::kNone));

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_3);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_4));

  // Filter 2 has overlap with both filter 1 and 3, delete this should remove
  // all entries.
  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter_2);
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_4));
}

// Test that DeleteAppId() can delete the setting for one filter.
TEST_F(PreferredAppListTest, DeleteAppIdForOneFilter) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.DeleteAppId(kAppId1);

  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test that when multiple filters set to the same app id, DeleteAppId() can
// delete all of them.
TEST_F(PreferredAppListTest, DeleteAppIdForMultipleFilters) {
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

  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_3));
}

// Test that for filter with multiple condition values, DeleteAppId() can
// delete them all.
TEST_F(PreferredAppListTest, DeleteAppIdForMultipleConditionValues) {
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
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(absl::nullopt, preferred_apps_.FindPreferredAppForUrl(url_http));
}

// Test that for multiple filters set to different app ids, DeleteAppId() only
// deletes the correct app id.
TEST_F(PreferredAppListTest, DeleteAppIdForMultipleAppIds) {
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
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_3);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  GURL filter_url_4 = GURL("https://www.google.com.au/");
  auto intent_filter_4 = apps_util::CreateIntentFilterForUrlScope(filter_url_4);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_4);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_4));

  GURL filter_url_5 = GURL("https://www.example.com/google");
  auto intent_filter_5 = apps_util::CreateIntentFilterForUrlScope(filter_url_5);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_5);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));

  GURL filter_url_6 = GURL("tel://98765432/");
  auto intent_filter_6 = apps_util::CreateIntentFilterForUrlScope(filter_url_6);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_6);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_6));

  preferred_apps_.DeleteAppId(kAppId1);
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_4));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_6));
  preferred_apps_.DeleteAppId(kAppId2);
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_4));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_6));
  preferred_apps_.DeleteAppId(kAppId3);
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_5));
  EXPECT_EQ(absl::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_6));
}
