// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list.h"

#include <optional>

#include "base/containers/contains.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAppId1[] = "abcdefg";
const char kAppId2[] = "gfedcba";
const char kAppId3[] = "hahahahaha";

}  // namespace

class PreferredAppListTest : public testing::Test {
 protected:
  apps::IntentFilterPtr MakePathFilter(const std::string& pattern,
                                       apps::PatternMatchType match_type) {
    auto intent_filter =
        apps_util::MakeSchemeAndHostOnlyFilter("https", "www.google.com");
    intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, pattern,
                                           match_type);
    return intent_filter;
  }

  apps::PreferredAppsList preferred_apps_;
};

// Test that for a single preferred app with URL filter, we can add
// and find (or not find) the correct preferred app id for different
// URLs.
TEST_F(PreferredAppListTest, AddPreferredAppForURL) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  GURL url_in_scope = GURL("https://www.google.com/abcde");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_wrong_scheme = GURL("tel://www.google.com/");
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));

  GURL url_wrong_host = GURL("https://www.hahaha.com/");
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_host));

  GURL url_not_in_scope = GURL("https://www.google.com/a");
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for preferred app with filter that does not have all condition
// types. E.g. add preferred app with intent filter that only have scheme.
TEST_F(PreferredAppListTest, TopLayerFilters) {
  auto intent_filter = apps_util::MakeSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_not_in_scope = GURL("http://www.google.com");
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for multiple preferred app setting with different number of condition
// types.
TEST_F(PreferredAppListTest, MixLayerFilters) {
  auto intent_filter_scheme = apps_util::MakeSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_scheme);

  auto intent_filter_scheme_host =
      apps_util::MakeSchemeAndHostOnlyFilter("http", "www.abc.com");
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_scheme_host);

  auto intent_filter_url =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.google.com/"));
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_url);

  GURL url_1 = GURL("tel://1234556/");
  GURL url_2 = GURL("http://www.abc.com/");
  GURL url_3 = GURL("https://www.google.com/");
  GURL url_out_scope = GURL("https://www.abc.com/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that when there are multiple preferred apps for one intent, the best
// matching one will be picked.
TEST_F(PreferredAppListTest, MultiplePreferredApps) {
  GURL url = GURL("https://www.google.com/");

  auto intent_filter_scheme = apps_util::MakeSchemeOnlyFilter("https");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_scheme);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url));

  auto intent_filter_scheme_host =
      apps_util::MakeSchemeAndHostOnlyFilter("https", "www.google.com");
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_scheme_host);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url));

  auto intent_filter_url =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.google.com/"));
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_url);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url));
}

// Test that we can properly add and search for filters that has multiple
// condition values for a condition type.
TEST_F(PreferredAppListTest, MultipleConditionValues) {
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(
      GURL("https://www.google.com/"), /*omit_port_for_testing=*/true);
  apps_util::AddConditionValue(apps::ConditionType::kScheme, "http",
                               apps::PatternMatchType::kLiteral, intent_filter);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  GURL url_http_out_of_scope = GURL("http://www.abc.com/");
  GURL url_wrong_scheme = GURL("tel://1234567/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_http_out_of_scope));
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));
}

// Test for more than one pattern available, we can find the correct match.
TEST_F(PreferredAppListTest, DifferentPatterns) {
  auto intent_filter_literal =
      MakePathFilter("/bc", apps::PatternMatchType::kLiteral);
  auto intent_filter_prefix =
      MakePathFilter("/a", apps::PatternMatchType::kPrefix);
  auto intent_filter_glob =
      MakePathFilter("/c.*d", apps::PatternMatchType::kGlob);

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
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that for same intent filter, the app id will overwrite the old setting.
TEST_F(PreferredAppListTest, OverwritePreferredApp) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.AddPreferredApp(kAppId2, intent_filter);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test that when overlap happens, the previous setting will be removed.
TEST_F(PreferredAppListTest, OverlapPreferredApp) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
}

// Test that the replaced app preferences is correct.
TEST_F(PreferredAppListTest, ReplacedAppPreference) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  auto replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(0u, replaced_app_preferences.size());

  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);
  EXPECT_EQ(1u, replaced_app_preferences.size());
  EXPECT_TRUE(replaced_app_preferences.find(kAppId1) !=
              replaced_app_preferences.end());

  GURL filter_url_4 = GURL("http://www.example.com/abc");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_4.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_4.host(),
      apps::PatternMatchType::kLiteral, intent_filter_3);
  // Test when replacing multiple preferred app entries with same app id.
  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(1u, replaced_app_preferences.size());
  EXPECT_TRUE(replaced_app_preferences.find(kAppId2) !=
              replaced_app_preferences.end());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_3);
  EXPECT_EQ(0u, replaced_app_preferences.size());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);
  EXPECT_EQ(1u, replaced_app_preferences.size());
  auto entry = replaced_app_preferences.find(kAppId1);
  EXPECT_TRUE(entry != replaced_app_preferences.end());
  EXPECT_EQ(2u, entry->second.size());

  // Test when replacing multiple preferred app entries with different app id.
  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(1u, replaced_app_preferences.size());
  EXPECT_TRUE(replaced_app_preferences.find(kAppId2) !=
              replaced_app_preferences.end());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId2, intent_filter_3);
  EXPECT_EQ(0u, replaced_app_preferences.size());

  replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId3, intent_filter_2);
  EXPECT_EQ(2u, replaced_app_preferences.size());
  entry = replaced_app_preferences.find(kAppId1);
  EXPECT_TRUE(entry != replaced_app_preferences.end());
  EXPECT_EQ(1u, entry->second.size());
  entry = replaced_app_preferences.find(kAppId2);
  EXPECT_TRUE(entry != replaced_app_preferences.end());
  EXPECT_EQ(1u, entry->second.size());
}

TEST_F(PreferredAppListTest, ReplacedAppPreferencesSameApp) {
  GURL filter_url = GURL("https://www.google.com/abc");

  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  auto replaced_app_preferences =
      preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(0u, replaced_app_preferences.size());
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

TEST_F(PreferredAppListTest, OverlapPreferencesSameApp) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_2);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
}

TEST_F(PreferredAppListTest, AddSameEntry) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(1U, preferred_apps_.GetEntrySize());

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(1U, preferred_apps_.GetEntrySize());
}

// Test that for a single preferred app with URL filter, we can delete
// the preferred app id.
TEST_F(PreferredAppListTest, DeletePreferredAppForURL) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  // If try to delete with wrong ID, won't delete.
  preferred_apps_.DeletePreferredApp(kAppId2, intent_filter);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test for preferred app with filter that does not have all condition
// types. E.g. delete preferred app with intent filter that only have scheme.
TEST_F(PreferredAppListTest, DeleteForTopLayerFilters) {
  auto intent_filter = apps_util::MakeSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_in_scope));
}

// Test that we can properly delete for filters that has multiple
// condition values for a condition type.
TEST_F(PreferredAppListTest, DeleteMultipleConditionValues) {
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(
      GURL("https://www.google.com/"), /*omit_port_for_testing=*/true);
  apps_util::AddConditionValue(apps::ConditionType::kScheme, "http",
                               apps::PatternMatchType::kLiteral, intent_filter);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_http));
}

// Test for more than one pattern available, we can delete the filter.
TEST_F(PreferredAppListTest, DeleteDifferentPatterns) {
  auto intent_filter_literal =
      MakePathFilter("/bc", apps::PatternMatchType::kLiteral);
  auto intent_filter_prefix =
      MakePathFilter("/a", apps::PatternMatchType::kPrefix);
  auto intent_filter_glob =
      MakePathFilter("/c.*d", apps::PatternMatchType::kGlob);

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
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  preferred_apps_.DeletePreferredApp(kAppId2, intent_filter_prefix);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  preferred_apps_.DeletePreferredApp(kAppId3, intent_filter_glob);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_3));
}

// Test that can delete properly for super set filters. E.g. the filter
// to delete has more condition values compare with filter that was set.
TEST_F(PreferredAppListTest, DeleteForNotCompletedFilter) {
  auto intent_filter_set =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.google.com/"));

  auto intent_filter_to_delete =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.google.com/"));
  apps_util::AddConditionValue(apps::ConditionType::kScheme, "http",
                               apps::PatternMatchType::kLiteral,
                               intent_filter_to_delete);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_set);

  GURL url = GURL("https://www.google.com/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url));

  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter_to_delete);

  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url));
}

// Test that when there are more than one entry has overlap filter.
TEST_F(PreferredAppListTest, DeleteOverlapFilters) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");
  GURL filter_url_4 = GURL("http://www.example.com/abc");

  // Filter 1 handles url 1 and 2.
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_1);

  // Filter 2 handles url 2 and 3.
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_2.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_2.host(),
      apps::PatternMatchType::kLiteral, intent_filter_2);

  // Filter 3 handles url 3 and 4.
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kScheme, filter_url_4.scheme(),
      apps::PatternMatchType::kLiteral, intent_filter_3);
  apps_util::AddConditionValue(
      apps::ConditionType::kAuthority, filter_url_4.host(),
      apps::PatternMatchType::kLiteral, intent_filter_3);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_3);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_4));

  // Filter 2 has overlap with both filter 1 and 3, delete this should remove
  // all entries.
  preferred_apps_.DeletePreferredApp(kAppId1, intent_filter_2);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_4));
}

// Test that DeleteAppId() can delete the setting for one filter.
TEST_F(PreferredAppListTest, DeleteAppIdForOneFilter) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.DeleteAppId(kAppId1);

  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url));
}

// Test that when multiple filters set to the same app id, DeleteAppId() can
// delete all of them.
TEST_F(PreferredAppListTest, DeleteAppIdForMultipleFilters) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.abc.com/google");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_2);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("tel://12345678/");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_3);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  apps::IntentFilters removed_filters = preferred_apps_.DeleteAppId(kAppId1);

  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  EXPECT_EQ(3u, removed_filters.size());
  EXPECT_TRUE(apps::Contains(removed_filters, intent_filter_1));
  EXPECT_TRUE(apps::Contains(removed_filters, intent_filter_2));
  EXPECT_TRUE(apps::Contains(removed_filters, intent_filter_3));
}

// Test that for filter with multiple condition values, DeleteAppId() can
// delete them all.
TEST_F(PreferredAppListTest, DeleteAppIdForMultipleConditionValues) {
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(
      GURL("https://www.google.com/"), /*omit_port_for_testing=*/true);
  apps_util::AddConditionValue(apps::ConditionType::kScheme, "http",
                               apps::PatternMatchType::kLiteral, intent_filter);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));

  preferred_apps_.DeleteAppId(kAppId1);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(url_http));
}

// Test that for multiple filters set to different app ids, DeleteAppId() only
// deletes the correct app id.
TEST_F(PreferredAppListTest, DeleteAppIdForMultipleAppIds) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.abc.com/google");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_2);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("tel://12345678/");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_3);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  GURL filter_url_4 = GURL("https://www.google.com.au/");
  auto intent_filter_4 = apps_util::MakeIntentFilterForUrlScope(filter_url_4);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_4);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_4));

  GURL filter_url_5 = GURL("https://www.example.com/google");
  auto intent_filter_5 = apps_util::MakeIntentFilterForUrlScope(filter_url_5);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_5);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));

  GURL filter_url_6 = GURL("tel://98765432/");
  auto intent_filter_6 = apps_util::MakeIntentFilterForUrlScope(filter_url_6);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_6);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_6));

  preferred_apps_.DeleteAppId(kAppId1);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_4));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_6));
  preferred_apps_.DeleteAppId(kAppId2);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_4));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_6));
  preferred_apps_.DeleteAppId(kAppId3);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_5));
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_6));
}

TEST_F(PreferredAppListTest, DeleteSupportedLinks) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);

  GURL filter_url_2 = GURL("tel://12345678/");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_2);

  GURL filter_url_3 = GURL("https://www.google.com.au/");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_3);

  auto deleted = preferred_apps_.DeleteSupportedLinks(kAppId1);

  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(1u, deleted.size());
  EXPECT_EQ(*intent_filter_1, *deleted[0]);

  deleted = preferred_apps_.DeleteSupportedLinks(kAppId2);
  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(1u, deleted.size());
  EXPECT_EQ(*intent_filter_3, *deleted[0]);
}

// Test that DeleteSupportedLinks removes the entire preference, including
// condition values other than http/https links.
TEST_F(PreferredAppListTest, DeleteSupportedLinksForMultipleConditionValues) {
  auto intent_filter =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.example.com/"));
  apps_util::AddConditionValue(apps::ConditionType::kScheme, "ftp",
                               apps::PatternMatchType::kLiteral, intent_filter);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  preferred_apps_.DeleteSupportedLinks(kAppId1);

  EXPECT_EQ(std::nullopt, preferred_apps_.FindPreferredAppForUrl(
                              GURL("ftp://www.example.com")));
}

TEST_F(PreferredAppListTest, ApplyBulkUpdateAdditions) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  GURL filter_url_2 = GURL("https://www.google.com/def");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  GURL filter_url_3 = GURL("https://www.google.com/hij");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);

  auto changes = std::make_unique<apps::PreferredAppChanges>();
  changes->added_filters[kAppId1].push_back(intent_filter_1->Clone());
  changes->added_filters[kAppId1].push_back(intent_filter_2->Clone());
  changes->added_filters[kAppId2].push_back(intent_filter_3->Clone());

  preferred_apps_.ApplyBulkUpdate(std::move(changes));

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));
}

TEST_F(PreferredAppListTest, ApplyBulkUpdateDuplicateAdditions) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  GURL filter_url_2 = GURL("https://www.google.com/def");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  GURL filter_url_3 = GURL("https://www.google.com/hij");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);

  auto changes = std::make_unique<apps::PreferredAppChanges>();
  changes->added_filters[kAppId1].push_back(intent_filter_1->Clone());
  changes->added_filters[kAppId1].push_back(intent_filter_2->Clone());
  changes->added_filters[kAppId2].push_back(intent_filter_3->Clone());

  preferred_apps_.ApplyBulkUpdate(changes->Clone());

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  EXPECT_EQ(3U, preferred_apps_.GetEntrySize());

  preferred_apps_.ApplyBulkUpdate(changes->Clone());

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  EXPECT_EQ(3U, preferred_apps_.GetEntrySize());
}

// Test that you can add and remove overlapping filters with a single call to
// ApplyBulkUpdate.
TEST_F(PreferredAppListTest, ApplyBulkUpdateAddAndRemove) {
  GURL filter_url_base = GURL("https://www.google.com/foo");
  auto intent_filter_base =
      apps_util::MakeIntentFilterForUrlScope(filter_url_base);
  GURL filter_url_ext = GURL("https://www.google.com/foo/bar");
  auto intent_filter_ext =
      apps_util::MakeIntentFilterForUrlScope(filter_url_ext);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_base);

  auto changes = std::make_unique<apps::PreferredAppChanges>();
  changes->added_filters[kAppId1].push_back(intent_filter_ext->Clone());
  changes->removed_filters[kAppId1].push_back(intent_filter_base->Clone());

  preferred_apps_.ApplyBulkUpdate(std::move(changes));

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_ext));
  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_base));
}

// Test that removing a filter using ApplyBulkUpdate only removes filters which
// match exactly, and not anything that overlaps.
TEST_F(PreferredAppListTest, ApplyBulkUpdateRemoveMatchesExactly) {
  GURL filter_url_base = GURL("https://www.google.com/foo");
  auto intent_filter_base =
      apps_util::MakeIntentFilterForUrlScope(filter_url_base);
  GURL filter_url_ext = GURL("https://www.google.com/foo/bar");
  auto intent_filter_ext =
      apps_util::MakeIntentFilterForUrlScope(filter_url_ext);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_ext);

  auto changes = std::make_unique<apps::PreferredAppChanges>();
  changes->removed_filters[kAppId1].push_back(intent_filter_base->Clone());
  preferred_apps_.ApplyBulkUpdate(std::move(changes));

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_ext));

  changes = std::make_unique<apps::PreferredAppChanges>();
  changes->removed_filters[kAppId1].push_back(intent_filter_ext->Clone());
  preferred_apps_.ApplyBulkUpdate(std::move(changes));

  EXPECT_EQ(std::nullopt,
            preferred_apps_.FindPreferredAppForUrl(filter_url_ext));
}

// Test that FindPreferredAppsForFilters() returns an empty flat_set if there
// are no matches.
TEST_F(PreferredAppListTest, FindNoPreferredApps) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.google.com.au/");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL test_url = GURL("https://www.example.com/google");
  auto test_intent_filter = apps_util::MakeIntentFilterForUrlScope(test_url);

  apps::IntentFilters intent_filters;
  intent_filters.push_back(std::move(test_intent_filter));

  auto preferred_apps =
      preferred_apps_.FindPreferredAppsForFilters(intent_filters);

  EXPECT_TRUE(preferred_apps.empty());
}

// Tests that FindPreferredAppsForFilters() returns an app id if a match is
// found.
TEST_F(PreferredAppListTest, FindOnePreferredApps) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.google.com.au/");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_2);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL test_url = GURL("https://www.example.com/google");
  auto test_intent_filter = apps_util::MakeIntentFilterForUrlScope(test_url);

  apps::IntentFilters intent_filters;
  intent_filters.push_back(std::move(intent_filter_2));
  intent_filters.push_back(std::move(test_intent_filter));

  auto preferred_apps =
      preferred_apps_.FindPreferredAppsForFilters(intent_filters);

  EXPECT_EQ(preferred_apps.size(), 1u);
  EXPECT_TRUE(preferred_apps.contains(kAppId2));
}

// Tests that FindPreferredAppsForFilters() returns multiple app ids if matches
// are made.
TEST_F(PreferredAppListTest, FindMultiplePreferredApps) {
  GURL filter_url_1 = GURL("https://www.google.com/abc");
  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_1);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_1));

  GURL filter_url_2 = GURL("https://www.abc.com/google");
  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_2);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url_2));

  GURL filter_url_3 = GURL("tel://12345678/");
  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_3);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_3));

  GURL filter_url_4 = GURL("https://www.google.com.au/");
  auto intent_filter_4 = apps_util::MakeIntentFilterForUrlScope(filter_url_4);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_4);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url_4));

  GURL filter_url_5 = GURL("https://www.example.com/google");
  auto intent_filter_5 = apps_util::MakeIntentFilterForUrlScope(filter_url_5);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_5);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(filter_url_5));

  apps::IntentFilters intent_filters;
  intent_filters.push_back(std::move(intent_filter_1));
  intent_filters.push_back(std::move(intent_filter_2));
  intent_filters.push_back(std::move(intent_filter_3));

  auto preferred_apps =
      preferred_apps_.FindPreferredAppsForFilters(intent_filters);

  EXPECT_EQ(preferred_apps.size(), 2u);
  EXPECT_TRUE(preferred_apps.contains(kAppId1));
  EXPECT_TRUE(preferred_apps.contains(kAppId2));
  EXPECT_FALSE(preferred_apps.contains(kAppId3));
}
