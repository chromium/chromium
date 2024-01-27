// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include "base/values.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kFilterUrl[] = "https://www.google.com/";
}

class IntentUtilTest : public testing::Test {
 protected:
  apps::ConditionPtr CreateMultiConditionValuesCondition() {
    std::vector<apps::ConditionValuePtr> condition_values;
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        "https", apps::PatternMatchType::kLiteral));
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        "http", apps::PatternMatchType::kLiteral));
    auto condition = std::make_unique<apps::Condition>(
        apps::ConditionType::kScheme, std::move(condition_values));
    return condition;
  }

  apps::IntentPtr CreateShareIntent(const std::string& mime_type) {
    auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
    intent->mime_type = mime_type;
    return intent;
  }

  std::vector<apps::IntentFilePtr> CreateIntentFiles(
      const GURL& url,
      std::optional<std::string> mime_type,
      std::optional<bool> is_directory) {
    auto file = std::make_unique<apps::IntentFile>(url);
    file->mime_type = mime_type;
    file->is_directory = is_directory;
    std::vector<apps::IntentFilePtr> files;
    files.push_back(std::move(file));
    return files;
  }

  apps::IntentPtr CreateIntent(
      const std::string& action,
      const GURL& url,
      const std::string& mime_type,
      std::vector<apps::IntentFilePtr> files,
      const std::string& activity_name,
      const GURL& drive_share_url,
      const std::string& share_text,
      const std::string& share_title,
      const std::string& start_type,
      std::vector<std::string> categories,
      const std::string& data,
      bool ui_bypassed,
      const base::flat_map<std::string, std::string>& extras) {
    auto intent = std::make_unique<apps::Intent>(action);
    intent->url = url;
    intent->mime_type = mime_type;
    intent->files = std::move(files);
    intent->activity_name = activity_name;
    intent->drive_share_url = drive_share_url;
    intent->share_text = share_text;
    intent->share_title = share_title;
    intent->start_type = start_type;
    intent->categories = categories;
    intent->data = data;
    intent->ui_bypassed = ui_bypassed;
    intent->extras = extras;
    return intent;
  }
};

TEST_F(IntentUtilTest, AllConditionMatches) {
  GURL test_url("https://www.google.com/");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_TRUE(intent->MatchFilter(intent_filter));
}

TEST_F(IntentUtilTest, OneConditionDoesNotMatch) {
  GURL test_url("https://www.abc.com/");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_FALSE(intent->MatchFilter(intent_filter));
}

TEST_F(IntentUtilTest, IntentDoesNotHaveValueToMatch) {
  GURL test_url("www.abc.com/");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_FALSE(intent->MatchFilter(intent_filter));
}

// Test ConditionMatch with more then one condition values.
TEST_F(IntentUtilTest, OneConditionValueMatch) {
  auto condition = CreateMultiConditionValuesCondition();
  GURL test_url("https://www.google.com/");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  EXPECT_TRUE(intent->MatchCondition(condition));
}

TEST_F(IntentUtilTest, NoneConditionValueMatch) {
  auto condition = CreateMultiConditionValuesCondition();
  GURL test_url("tel://www.google.com/");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  EXPECT_FALSE(intent->MatchCondition(condition));
}

// Test Condition Value match with different pattern match type.
TEST_F(IntentUtilTest, NoneMatchType) {
  auto condition_value = std::make_unique<apps::ConditionValue>(
      "https", apps::PatternMatchType::kLiteral);
  EXPECT_TRUE(apps_util::ConditionValueMatches("https", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("http", condition_value));
}

TEST_F(IntentUtilTest, LiteralMatchType) {
  auto condition_value = std::make_unique<apps::ConditionValue>(
      "https", apps::PatternMatchType::kLiteral);
  EXPECT_TRUE(apps_util::ConditionValueMatches("https", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("http", condition_value));
}

TEST_F(IntentUtilTest, PrefixMatchType) {
  auto condition_value = std::make_unique<apps::ConditionValue>(
      "/ab", apps::PatternMatchType::kPrefix);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/abc", condition_value));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/ABC", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/d", condition_value));
}

TEST_F(IntentUtilTest, SuffixMatchType) {
  auto condition_value = std::make_unique<apps::ConditionValue>(
      ".google.com", apps::PatternMatchType::kSuffix);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("en.google.com", condition_value));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("es.google.com", condition_value));
  EXPECT_TRUE(apps_util::ConditionValueMatches(".google.com", condition_value));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("es.google.org", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("google.com", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("other", condition_value));
}

TEST_F(IntentUtilTest, GlobMatchType) {
  auto condition_value_star = std::make_unique<apps::ConditionValue>(
      "/a*b", apps::PatternMatchType::kGlob);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/b", condition_value_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/ab", condition_value_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aab", condition_value_star));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/aaaaaab", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/aabb", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/aabc", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/bb", condition_value_star));

  auto condition_value_dot = std::make_unique<apps::ConditionValue>(
      "/a.b", apps::PatternMatchType::kGlob);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aab", condition_value_dot));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/acb", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/ab", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abd", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abbd", condition_value_dot));

  auto condition_value_dot_and_star = std::make_unique<apps::ConditionValue>(
      "/a.*b", apps::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/aab", condition_value_dot_and_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aadsfadslkjb",
                                               condition_value_dot_and_star));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/ab", condition_value_dot_and_star));

  // This arguably should be true, however the algorithm is transcribed from the
  // upstream Android codebase, which behaves like this.
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abasdfab",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abasdfad",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/bbasdfab",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/a", condition_value_dot_and_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/b", condition_value_dot_and_star));

  auto condition_value_escape_dot = std::make_unique<apps::ConditionValue>(
      "/a\\.b", apps::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/a.b", condition_value_escape_dot));

  // This arguably should be false, however the transcribed is carried from the
  // upstream Android codebase, which behaves like this.
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/acb", condition_value_escape_dot));

  auto condition_value_escape_star = std::make_unique<apps::ConditionValue>(
      "/a\\*b", apps::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/a*b", condition_value_escape_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/acb", condition_value_escape_star));
}

TEST_F(IntentUtilTest, FilterMatchLevel) {
  auto filter_scheme_only = apps_util::MakeSchemeOnlyFilter("http");
  auto filter_scheme_and_host_only =
      apps_util::MakeSchemeAndHostOnlyFilter("https", "www.abc.com");
  auto filter_url =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.google.com/"));
  auto filter_empty = std::make_unique<apps::IntentFilter>();

  EXPECT_TRUE(filter_scheme_only->IsBrowserFilter());
  EXPECT_FALSE(filter_scheme_and_host_only->IsBrowserFilter());
  EXPECT_FALSE(filter_url->IsBrowserFilter());
  EXPECT_FALSE(filter_empty->IsBrowserFilter());

  EXPECT_EQ(filter_url->GetFilterMatchLevel(),
            static_cast<int>(apps::IntentFilterMatchLevel::kScheme) +
                static_cast<int>(apps::IntentFilterMatchLevel::kAuthority) +
                static_cast<int>(apps::IntentFilterMatchLevel::kPath));
  EXPECT_EQ(filter_scheme_and_host_only->GetFilterMatchLevel(),
            static_cast<int>(apps::IntentFilterMatchLevel::kScheme) +
                static_cast<int>(apps::IntentFilterMatchLevel::kAuthority));
  EXPECT_EQ(filter_scheme_only->GetFilterMatchLevel(),
            static_cast<int>(apps::IntentFilterMatchLevel::kScheme));
  EXPECT_EQ(filter_empty->GetFilterMatchLevel(),
            static_cast<int>(apps::IntentFilterMatchLevel::kNone));

  EXPECT_TRUE(filter_url->GetFilterMatchLevel() >
              filter_scheme_and_host_only->GetFilterMatchLevel());
  EXPECT_TRUE(filter_scheme_and_host_only->GetFilterMatchLevel() >
              filter_scheme_only->GetFilterMatchLevel());
  EXPECT_TRUE(filter_scheme_only->GetFilterMatchLevel() >
              filter_empty->GetFilterMatchLevel());
}

TEST_F(IntentUtilTest, ActionMatch) {
  GURL test_url("https://www.google.com/");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(GURL(kFilterUrl));
  EXPECT_TRUE(intent->MatchFilter(intent_filter));

  auto send_intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, test_url);
  send_intent->action = apps_util::kIntentActionSend;
  EXPECT_FALSE(send_intent->MatchFilter(intent_filter));

  auto send_intent_filter =
      apps_util::MakeIntentFilterForUrlScope(GURL(kFilterUrl));
  send_intent_filter->conditions[0]->condition_values[0]->value =
      apps_util::kIntentActionSend;
  EXPECT_FALSE(intent->MatchFilter(send_intent_filter));
}

TEST_F(IntentUtilTest, AuthorityMatch) {
  auto MakeAuthorityFilter = [](const std::string& authority,
                                apps::PatternMatchType match_type =
                                    apps::PatternMatchType::kLiteral) {
    auto intent_filter = std::make_unique<apps::IntentFilter>();
    intent_filter->AddSingleValueCondition(apps::ConditionType::kAuthority,
                                           authority, match_type);
    return intent_filter;
  };

  auto MakeViewIntent = [](std::string_view url_spec) {
    return std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                          GURL(url_spec));
  };

  std::vector<std::string> explicit_ports{
      apps_util::AuthorityView::Encode(GURL("https://example.com:1234")),
      apps_util::AuthorityView::Encode(
          url::Origin::CreateFromNormalizedTuple("https", "example.com", 1234)),
  };
  for (const auto& explicit_port : explicit_ports) {
    auto authority_filter = MakeAuthorityFilter(explicit_port);
    EXPECT_TRUE(MakeViewIntent("https://example.com:1234")
                    ->MatchFilter(authority_filter));
    EXPECT_FALSE(
        MakeViewIntent("https://example.com")->MatchFilter(authority_filter));
    EXPECT_FALSE(MakeViewIntent("https://example.com:5678")
                     ->MatchFilter(authority_filter));
    EXPECT_FALSE(MakeViewIntent("https://example.org:1234")
                     ->MatchFilter(authority_filter));
  }

  auto implicit_port = MakeAuthorityFilter(
      apps_util::AuthorityView::Encode(GURL("https://example.com")));
  EXPECT_TRUE(
      MakeViewIntent("https://example.com")->MatchFilter(implicit_port));
  EXPECT_TRUE(
      MakeViewIntent("https://example.com:443")->MatchFilter(implicit_port));
  EXPECT_FALSE(
      MakeViewIntent("https://example.com:80")->MatchFilter(implicit_port));
  EXPECT_FALSE(
      MakeViewIntent("https://example.com:1234")->MatchFilter(implicit_port));
  EXPECT_FALSE(
      MakeViewIntent("https://example.org")->MatchFilter(implicit_port));

  auto portless_scheme = MakeAuthorityFilter(
      apps_util::AuthorityView::Encode(GURL("file://test")));
  EXPECT_TRUE(MakeViewIntent("file://test")->MatchFilter(portless_scheme));
  EXPECT_FALSE(
      MakeViewIntent("file://test:1234")->MatchFilter(portless_scheme));
  EXPECT_FALSE(MakeViewIntent("file://other")->MatchFilter(portless_scheme));

  std::vector<std::string> host_onlys{
      "example.com",
      apps_util::AuthorityView::Encode(
          url::Origin::CreateFromNormalizedTuple("https", "example.com", 0)),
  };
  for (const auto& host_only : host_onlys) {
    auto authority_filter = MakeAuthorityFilter(host_only);
    EXPECT_TRUE(
        MakeViewIntent("https://example.com")->MatchFilter(authority_filter));
    EXPECT_TRUE(MakeViewIntent("https://example.com:80")
                    ->MatchFilter(authority_filter));
    EXPECT_TRUE(MakeViewIntent("https://example.com:1234")
                    ->MatchFilter(authority_filter));
    EXPECT_FALSE(
        MakeViewIntent("https://example.org")->MatchFilter(authority_filter));
  }

  auto host_suffix = MakeAuthorityFilter(
      apps_util::AuthorityView::Encode(GURL("https://example.com:1234")),
      apps::PatternMatchType::kSuffix);
  EXPECT_TRUE(
      MakeViewIntent("https://example.com:1234")->MatchFilter(host_suffix));
  EXPECT_TRUE(MakeViewIntent("https://test.example.com:1234")
                  ->MatchFilter(host_suffix));
  EXPECT_FALSE(
      MakeViewIntent("https://example.com.au:1234")->MatchFilter(host_suffix));
  EXPECT_FALSE(MakeViewIntent("https://test.example.com.au:1234")
                   ->MatchFilter(host_suffix));
  EXPECT_FALSE(MakeViewIntent("https://example.com")->MatchFilter(host_suffix));
}

TEST_F(IntentUtilTest, MimeTypeMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";
  std::string mime_type_only_main_type = "text";
  std::string mime_type_only_star = "*";

  auto intent1 = CreateShareIntent(mime_type1);
  auto intent2 = CreateShareIntent(mime_type2);
  auto intent_sub_wildcard = CreateShareIntent(mime_type_sub_wildcard);
  auto intent_all_wildcard = CreateShareIntent(mime_type_all_wildcard);
  auto intent_only_main_type = CreateShareIntent(mime_type_only_main_type);
  auto intent_only_star = CreateShareIntent(mime_type_only_star);

  auto filter1 = apps_util::MakeIntentFilterForMimeType(mime_type1);

  EXPECT_TRUE(intent1->MatchFilter(filter1));
  EXPECT_FALSE(intent2->MatchFilter(filter1));
  EXPECT_FALSE(intent_sub_wildcard->MatchFilter(filter1));
  EXPECT_FALSE(intent_all_wildcard->MatchFilter(filter1));
  EXPECT_FALSE(intent_only_main_type->MatchFilter(filter1));
  EXPECT_FALSE(intent_only_star->MatchFilter(filter1));

  auto filter2 = apps_util::MakeIntentFilterForMimeType(mime_type2);

  EXPECT_FALSE(intent1->MatchFilter(filter2));
  EXPECT_TRUE(intent2->MatchFilter(filter2));
  EXPECT_FALSE(intent_sub_wildcard->MatchFilter(filter2));
  EXPECT_FALSE(intent_all_wildcard->MatchFilter(filter2));
  EXPECT_FALSE(intent_only_main_type->MatchFilter(filter2));
  EXPECT_FALSE(intent_only_star->MatchFilter(filter2));

  auto filter_sub_wildcard =
      apps_util::MakeIntentFilterForMimeType(mime_type_sub_wildcard);

  EXPECT_TRUE(intent1->MatchFilter(filter_sub_wildcard));
  EXPECT_FALSE(intent2->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent_sub_wildcard->MatchFilter(filter_sub_wildcard));
  EXPECT_FALSE(intent_all_wildcard->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent_only_main_type->MatchFilter(filter_sub_wildcard));
  EXPECT_FALSE(intent_only_star->MatchFilter(filter_sub_wildcard));

  auto filter_all_wildcard =
      apps_util::MakeIntentFilterForMimeType(mime_type_all_wildcard);

  EXPECT_TRUE(intent1->MatchFilter(filter_all_wildcard));
  EXPECT_TRUE(intent2->MatchFilter(filter_all_wildcard));
  EXPECT_TRUE(intent_sub_wildcard->MatchFilter(filter_all_wildcard));
  EXPECT_TRUE(intent_all_wildcard->MatchFilter(filter_all_wildcard));
  EXPECT_TRUE(intent_only_main_type->MatchFilter(filter_all_wildcard));
  EXPECT_TRUE(intent_only_star->MatchFilter(filter_all_wildcard));

  auto filter_only_main_type =
      apps_util::MakeIntentFilterForMimeType(mime_type_only_main_type);

  EXPECT_TRUE(intent1->MatchFilter(filter_only_main_type));
  EXPECT_FALSE(intent2->MatchFilter(filter_only_main_type));
  EXPECT_TRUE(intent_sub_wildcard->MatchFilter(filter_only_main_type));
  EXPECT_FALSE(intent_all_wildcard->MatchFilter(filter_only_main_type));
  EXPECT_TRUE(intent_only_main_type->MatchFilter(filter_only_main_type));
  EXPECT_FALSE(intent_only_star->MatchFilter(filter_only_main_type));

  auto filter_only_star =
      apps_util::MakeIntentFilterForMimeType(mime_type_only_star);

  EXPECT_TRUE(intent1->MatchFilter(filter_only_star));
  EXPECT_TRUE(intent2->MatchFilter(filter_only_star));
  EXPECT_TRUE(intent_sub_wildcard->MatchFilter(filter_only_star));
  EXPECT_TRUE(intent_all_wildcard->MatchFilter(filter_only_star));
  EXPECT_TRUE(intent_only_main_type->MatchFilter(filter_only_star));
  EXPECT_TRUE(intent_only_star->MatchFilter(filter_only_star));
}

TEST_F(IntentUtilTest, CommonMimeTypeMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type3 = "text/html";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";

  auto filter1 = apps_util::MakeIntentFilterForSend(mime_type1);
  auto filter2 = apps_util::MakeIntentFilterForSend(mime_type2);
  auto filter3 = apps_util::MakeIntentFilterForSend(mime_type3);
  auto filter_sub_wildcard =
      apps_util::MakeIntentFilterForSend(mime_type_sub_wildcard);
  auto filter_all_wildcard =
      apps_util::MakeIntentFilterForSend(mime_type_all_wildcard);
  auto filter1_and_2 = apps_util::MakeIntentFilterForSend(mime_type1);
  filter1_and_2->conditions[1]->condition_values.push_back(
      std::make_unique<apps::ConditionValue>(
          mime_type2, apps::PatternMatchType::kMimeType));

  std::vector<GURL> urls;
  std::vector<std::string> mime_types;

  urls.emplace_back("abc");

  // Test match with text/plain type.
  mime_types.push_back(mime_type1);
  auto intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_TRUE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));
  EXPECT_TRUE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter3));
  EXPECT_TRUE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));

  // Test match with image/jpeg type.
  mime_types.clear();
  mime_types.push_back(mime_type2);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_TRUE(intent->MatchFilter(filter2));
  EXPECT_TRUE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter3));
  EXPECT_FALSE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));

  // Test match with text/html type.
  mime_types.clear();
  mime_types.push_back(mime_type3);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));
  EXPECT_FALSE(intent->MatchFilter(filter1_and_2));
  EXPECT_TRUE(intent->MatchFilter(filter3));
  EXPECT_TRUE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));

  // Test match with text/* types.
  mime_types.clear();
  mime_types.push_back(mime_type_sub_wildcard);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));
  EXPECT_FALSE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter3));
  EXPECT_TRUE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));

  // Test match with */* type.
  mime_types.clear();
  mime_types.push_back(mime_type_all_wildcard);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));
  EXPECT_FALSE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter3));
  EXPECT_FALSE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));
}

TEST_F(IntentUtilTest, CommonMimeTypeMatchMultiple) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type3 = "text/html";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";

  auto filter1 = apps_util::MakeIntentFilterForSendMultiple(mime_type1);
  auto filter2 = apps_util::MakeIntentFilterForSendMultiple(mime_type2);
  auto filter3 = apps_util::MakeIntentFilterForSendMultiple(mime_type3);
  auto filter_sub_wildcard =
      apps_util::MakeIntentFilterForSendMultiple(mime_type_sub_wildcard);
  auto filter_all_wildcard =
      apps_util::MakeIntentFilterForSendMultiple(mime_type_all_wildcard);
  auto filter1_and_2 = apps_util::MakeIntentFilterForSendMultiple(mime_type1);
  filter1_and_2->conditions[1]->condition_values.push_back(
      std::make_unique<apps::ConditionValue>(
          mime_type2, apps::PatternMatchType::kMimeType));

  std::vector<GURL> urls;
  std::vector<std::string> mime_types;

  urls.emplace_back("abc");
  urls.emplace_back("def");

  // Test match with same mime types.
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type1);
  auto intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_TRUE(intent->MatchFilter(filter1));
  EXPECT_TRUE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter3));

  // Test match with same main type and different sub type.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type3);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter3));
  EXPECT_TRUE(intent->MatchFilter(filter_sub_wildcard));

  // Test match with explicit type and wildcard sub type.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type_sub_wildcard);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter1_and_2));
  EXPECT_TRUE(intent->MatchFilter(filter_sub_wildcard));

  // Test match with different mime types.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type2);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));
  EXPECT_FALSE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter1_and_2));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));

  // Test match with explicit type and general wildcard type.
  mime_types.clear();
  mime_types.push_back(mime_type1);
  mime_types.push_back(mime_type_all_wildcard);
  intent = apps_util::MakeShareIntent(urls, mime_types);
  EXPECT_FALSE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter1_and_2));
  EXPECT_FALSE(intent->MatchFilter(filter_sub_wildcard));
  EXPECT_TRUE(intent->MatchFilter(filter_all_wildcard));
}

GURL test_url(const std::string& file_name) {
  GURL url = GURL("filesystem:https://site.com/isolated/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

GURL ext_test_url(const std::string& file_name) {
  GURL url =
      GURL("filesystem:chrome-extension://extensionid/external/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

GURL system_web_app_test_url(const std::string& file_name) {
  GURL url = GURL("filesystem:chrome://file-manager/external/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

TEST_F(IntentUtilTest, FileExtensionMatch) {
  std::string mime_type_mp3 = "audio/mp3";
  std::string file_ext_mp3 = "mp3";
  std::string mime_type_mpeg = "audio/mpeg";

  auto file_filter =
      apps_util::MakeFileFilterForView(mime_type_mp3, file_ext_mp3, "label");

  // Test match with the same mime type and the same file extension.
  auto intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.mp3"), mime_type_mp3, false));
  EXPECT_TRUE(intent->MatchFilter(file_filter));

  // Test match with different mime types and the same file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.mp3"), mime_type_mp3, false));
  EXPECT_TRUE(intent->MatchFilter(file_filter));

  // Test match with the same mime type and a different file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.png"), mime_type_mp3, false));
  EXPECT_TRUE(intent->MatchFilter(file_filter));

  // Test match with different mime types and a different file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.png"), mime_type_mpeg, false));
  EXPECT_FALSE(intent->MatchFilter(file_filter));

  std::string file_ext_dot_mp3 = ".mp3";
  auto file_filter_dot = apps_util::MakeFileFilterForView(
      mime_type_mp3, file_ext_dot_mp3, "label");

  // The whole extension must match, not just the end.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.extramp3"), mime_type_mpeg, false));
  EXPECT_FALSE(intent->MatchFilter(file_filter));
  EXPECT_FALSE(intent->MatchFilter(file_filter_dot));

  // Check that the filter behaves the same with and without a leading ".".
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.mp3"), mime_type_mpeg, false));
  EXPECT_TRUE(intent->MatchFilter(file_filter_dot));
}

TEST_F(IntentUtilTest, FileExtensionMatchCaseInsensitive) {
  auto lowercase_filter =
      apps_util::MakeFileFilterForView("text/csv", "csv", "label");
  auto uppercase_filter =
      apps_util::MakeFileFilterForView("text/csv", "CSV", "label");

  auto lowercase_intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.csv"), std::nullopt, false));
  EXPECT_TRUE(lowercase_intent->MatchFilter(lowercase_filter));
  EXPECT_TRUE(lowercase_intent->MatchFilter(uppercase_filter));

  auto uppercase_intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.CSV"), std::nullopt, false));
  EXPECT_TRUE(uppercase_intent->MatchFilter(lowercase_filter));
  EXPECT_TRUE(uppercase_intent->MatchFilter(uppercase_filter));

  auto mixcase_intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(test_url("abc.CsV"), std::nullopt, false));
  EXPECT_TRUE(mixcase_intent->MatchFilter(lowercase_filter));
  EXPECT_TRUE(mixcase_intent->MatchFilter(uppercase_filter));
}

TEST_F(IntentUtilTest, FileURLMatch) {
  std::string mp3_url_pattern = R"(filesystem:chrome-extension://.*/.*\.mp3)";

  auto url_filter = apps_util::MakeURLFilterForView(mp3_url_pattern, "label");

  // Test match with mp3 file extension.
  auto intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc.mp3"), "", false));
  EXPECT_TRUE(intent->MatchFilter(url_filter));

  // Test non-match with mp4 file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc.mp4"), "", false));
  EXPECT_FALSE(intent->MatchFilter(url_filter));

  // Test non-match with just the end of a file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc.testmp3"), "", false));
  EXPECT_FALSE(intent->MatchFilter(url_filter));

  std::string single_wild_url_pattern = "filesystem:chrome-extension://.*/.*";
  auto wild_filter =
      apps_util::MakeURLFilterForView(single_wild_url_pattern, "label");

  // Test that mp3 matches with *
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc.mp3"), "", false));
  EXPECT_TRUE(intent->MatchFilter(wild_filter));

  // Test that no file extension matches with *
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc"), "", false));
  EXPECT_TRUE(intent->MatchFilter(wild_filter));

  std::string ext_wild_url_pattern =
      R"(filesystem:chrome-extension://.*/.*\..*)";
  auto ext_wild_filter =
      apps_util::MakeURLFilterForView(ext_wild_url_pattern, "label");

  // Test that mp3 matches with *.*
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc.mp3"), "", false));
  EXPECT_TRUE(intent->MatchFilter(ext_wild_filter));

  // Test that no file extension does not match with *.*
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(ext_test_url("abc"), "", false));
  EXPECT_FALSE(intent->MatchFilter(ext_wild_filter));
}

TEST_F(IntentUtilTest, FileSystemWebAppURLMatch) {
  std::string mp3_url_pattern = R"(filesystem:chrome://.*/.*\.mp3)";

  auto url_filter = apps_util::MakeURLFilterForView(mp3_url_pattern, "label");

  // Test match with mp3 file extension.
  auto intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(system_web_app_test_url("abc.mp3"), "", false));
  EXPECT_TRUE(intent->MatchFilter(url_filter));

  // Test non-match with mp4 file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(system_web_app_test_url("abc.mp4"), "", false));
  EXPECT_FALSE(intent->MatchFilter(url_filter));

  // Test non-match with just the end of a file extension.
  intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView,
      CreateIntentFiles(system_web_app_test_url("abc.testmp3"), "", false));
  EXPECT_FALSE(intent->MatchFilter(url_filter));
}

TEST_F(IntentUtilTest, FileWithTitleText) {
  const std::string mime_type = "image/jpeg";
  auto filter = apps_util::MakeIntentFilterForSend(mime_type);

  const std::vector<GURL> urls{GURL("abc")};
  const std::vector<std::string> mime_types{mime_type};

  auto intent = apps_util::MakeShareIntent(urls, mime_types, "text", "title");
  EXPECT_TRUE(intent->share_text.has_value());
  EXPECT_EQ(intent->share_text.value(), "text");
  EXPECT_TRUE(intent->share_title.has_value());
  EXPECT_EQ(intent->share_title.value(), "title");
  EXPECT_TRUE(intent->MatchFilter(filter));

  intent = apps_util::MakeShareIntent(urls, mime_types, "text", "");
  EXPECT_TRUE(intent->share_text.has_value());
  EXPECT_EQ(intent->share_text.value(), "text");
  EXPECT_FALSE(intent->share_title.has_value());
  EXPECT_TRUE(intent->MatchFilter(filter));

  intent = apps_util::MakeShareIntent(urls, mime_types, "", "title");
  EXPECT_FALSE(intent->share_text.has_value());
  EXPECT_TRUE(intent->share_title.has_value());
  EXPECT_EQ(intent->share_title.value(), "title");
  EXPECT_TRUE(intent->MatchFilter(filter));

  intent = apps_util::MakeShareIntent(urls, mime_types, "", "");
  EXPECT_FALSE(intent->share_text.has_value());
  EXPECT_FALSE(intent->share_title.has_value());
  EXPECT_TRUE(intent->MatchFilter(filter));
}

TEST_F(IntentUtilTest, FileWithDlpSourceUrls) {
  const std::string mime_type = "image/jpeg";
  const GURL file_url = GURL("https://www.google.com/");
  const std::string dlp_source_url = "https://www.example.com/";

  auto filter = apps_util::MakeIntentFilterForSend(mime_type);
  const std::vector<GURL> urls{file_url};
  const std::vector<std::string> mime_types{mime_type};
  const std::vector<std::string> dlp_source_urls{dlp_source_url};

  auto intent = apps_util::MakeShareIntent(urls, mime_types, dlp_source_urls);
  ASSERT_EQ(1u, intent->files.size());
  EXPECT_EQ(file_url, intent->files[0]->url);
  EXPECT_EQ(dlp_source_url, intent->files[0]->dlp_source_url);
  EXPECT_TRUE(intent->MatchFilter(filter));
}

/*
 * Tests that the MakeShareIntent function overload for making intent for a
 * single files creates an intent with consistent mime types.
 */
TEST_F(IntentUtilTest, ShareSingleIntent) {
  const std::string mime_type = "image/jpeg";
  const GURL file_url = GURL("https://www.google.com/");
  const GURL drive_share_url = GURL("https://drive.google.com/");

  auto intent = apps_util::MakeShareIntent(file_url, mime_type, drive_share_url,
                                           /* is_directory: */ false);

  ASSERT_EQ(1u, intent->files.size());
  ASSERT_TRUE(intent->mime_type);
  EXPECT_EQ(mime_type, intent->mime_type.value());
  ASSERT_TRUE(intent->files[0]->mime_type);
  EXPECT_EQ(mime_type, intent->files[0]->mime_type.value());
}

TEST_F(IntentUtilTest, TextMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  auto filter1 = apps_util::MakeIntentFilterForMimeType(mime_type1);
  auto filter2 = apps_util::MakeIntentFilterForMimeType(mime_type2);

  auto intent = apps_util::MakeShareIntent("text", "");
  EXPECT_TRUE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));

  intent = apps_util::MakeShareIntent("", "title");
  EXPECT_TRUE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));

  intent = apps_util::MakeShareIntent("text", "title");
  EXPECT_TRUE(intent->MatchFilter(filter1));
  EXPECT_FALSE(intent->MatchFilter(filter2));
}

TEST_F(IntentUtilTest, Convert) {
  const std::string action = apps_util::kIntentActionSend;
  GURL test_url1 = GURL("https://www.google.com/");
  GURL test_url2 = GURL("https://www.abc.com/");
  GURL test_url3 = GURL("https://www.foo.com/");
  const std::string mime_type = "image/jpeg";
  const std::string activity_name = "test";
  const std::string share_text = "share text";
  const std::string share_title = "share title";
  const std::string start_type = "start type";
  const std::string category1 = "category1";
  const std::string data = "data";
  const bool ui_bypassed = true;
  base::flat_map<std::string, std::string> extras = {
      {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

  auto files = std::vector<apps::IntentFilePtr>();
  files.push_back(std::make_unique<apps::IntentFile>(test_url1));
  files.push_back(std::make_unique<apps::IntentFile>(test_url2));

  auto src_intent =
      CreateIntent(action, test_url1, mime_type, std::move(files),
                   activity_name, test_url3, share_text, share_title,
                   start_type, {category1}, data, ui_bypassed, extras);
  base::Value value = apps_util::ConvertIntentToValue(src_intent);
  auto dst_intent = apps_util::ConvertValueToIntent(std::move(value));

  EXPECT_EQ(action, dst_intent->action);
  EXPECT_EQ(test_url1, dst_intent->url.value());
  EXPECT_EQ(mime_type, dst_intent->mime_type.value());
  EXPECT_EQ(2u, dst_intent->files.size());
  EXPECT_EQ(test_url1, dst_intent->files[0]->url);
  EXPECT_EQ(test_url2, dst_intent->files[1]->url);
  EXPECT_EQ(activity_name, dst_intent->activity_name.value());
  EXPECT_EQ(test_url3, dst_intent->drive_share_url.value());
  EXPECT_EQ(share_text, dst_intent->share_text.value());
  EXPECT_EQ(share_title, dst_intent->share_title.value());
  EXPECT_EQ(start_type, dst_intent->start_type.value());
  EXPECT_EQ(1u, dst_intent->categories.size());
  EXPECT_EQ(category1, dst_intent->categories[0]);
  EXPECT_EQ(data, dst_intent->data.value());
  EXPECT_EQ(ui_bypassed, dst_intent->ui_bypassed);
  EXPECT_FALSE(dst_intent->extras.empty());
  EXPECT_EQ(3u, dst_intent->extras.size());
  EXPECT_EQ(extras, dst_intent->extras);
}

TEST_F(IntentUtilTest, ConvertEmptyIntent) {
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
  base::Value value = apps_util::ConvertIntentToValue(intent);
  auto dst_intent = apps_util::ConvertValueToIntent(std::move(value));

  EXPECT_FALSE(dst_intent->url.has_value());
  EXPECT_FALSE(dst_intent->mime_type.has_value());
  EXPECT_TRUE(dst_intent->files.empty());
  EXPECT_FALSE(dst_intent->activity_name.has_value());
  EXPECT_FALSE(dst_intent->drive_share_url.has_value());
  EXPECT_FALSE(dst_intent->share_text.has_value());
  EXPECT_FALSE(dst_intent->share_title.has_value());
  EXPECT_FALSE(dst_intent->start_type.has_value());
  EXPECT_TRUE(dst_intent->categories.empty());
  EXPECT_FALSE(dst_intent->data.has_value());
  EXPECT_FALSE(dst_intent->ui_bypassed.has_value());
  EXPECT_TRUE(dst_intent->extras.empty());
}

TEST_F(IntentUtilTest, CalculateCommonMimeType) {
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({}));

  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({""}));
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({"not_a_valid_type"}));
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType({"not_a_valid_type/"}));
  EXPECT_EQ("*/*",
            apps_util::CalculateCommonMimeType({"not_a_valid_type/foo/bar"}));

  EXPECT_EQ("image/png", apps_util::CalculateCommonMimeType({"image/png"}));
  EXPECT_EQ("image/png",
            apps_util::CalculateCommonMimeType({"image/png", "image/png"}));
  EXPECT_EQ("image/*",
            apps_util::CalculateCommonMimeType({"image/png", "image/jpeg"}));
  EXPECT_EQ("*/*",
            apps_util::CalculateCommonMimeType({"image/png", "text/plain"}));
  EXPECT_EQ("*/*", apps_util::CalculateCommonMimeType(
                       {"image/png", "image/jpeg", "text/plain"}));
}

TEST_F(IntentUtilTest, IsGenericFileHandler) {
  using apps::Intent;
  using apps::IntentFile;
  using apps::IntentFilePtr;
  using apps::IntentFilterPtr;
  using apps::IntentPtr;

  std::vector<IntentFilePtr> intent_files;
  IntentFilePtr foo = std::make_unique<IntentFile>(test_url("foo.jpg"));
  foo->mime_type = "image/jpeg";
  foo->is_directory = false;
  intent_files.push_back(std::move(foo));

  IntentFilePtr bar = std::make_unique<IntentFile>(test_url("bar.txt"));
  bar->mime_type = "text/plain";
  bar->is_directory = false;
  intent_files.push_back(std::move(bar));

  std::vector<IntentFilePtr> intent_files2;
  IntentFilePtr foo2 = std::make_unique<IntentFile>(test_url("foo.ics"));
  foo2->mime_type = "text/calendar";
  foo2->is_directory = false;
  intent_files2.push_back(std::move(foo2));

  std::vector<IntentFilePtr> intent_files3;
  IntentFilePtr foo_dir = std::make_unique<IntentFile>(test_url("foo/"));
  foo_dir->mime_type = "";
  foo_dir->is_directory = true;
  intent_files3.push_back(std::move(foo_dir));

  IntentPtr intent = std::make_unique<Intent>(apps_util::kIntentActionView,
                                              std::move(intent_files));
  IntentPtr intent2 = std::make_unique<Intent>(apps_util::kIntentActionView,
                                               std::move(intent_files2));
  IntentPtr intent3 = std::make_unique<Intent>(apps_util::kIntentActionView,
                                               std::move(intent_files3));

  const std::string kLabel = "";

  // extensions: ["*"]
  IntentFilterPtr filter1 = apps_util::MakeFileFilterForView("", "*", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter1));
  EXPECT_TRUE(filter1->IsFileExtensionsFilter());

  // extensions: ["*", "jpg"]
  IntentFilterPtr filter2 = apps_util::MakeFileFilterForView("", "*", kLabel);
  apps_util::AddConditionValue(apps::ConditionType::kFile, "jpg",
                               apps::PatternMatchType::kFileExtension, filter2);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter2));
  EXPECT_TRUE(filter2->IsFileExtensionsFilter());

  // extensions: ["jpg"]
  IntentFilterPtr filter3 = apps_util::MakeFileFilterForView("", "jpg", kLabel);
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent, filter3));
  EXPECT_TRUE(filter3->IsFileExtensionsFilter());

  // types: ["*"]
  IntentFilterPtr filter4 = apps_util::MakeFileFilterForView("*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter4));
  EXPECT_FALSE(filter4->IsFileExtensionsFilter());

  // types: ["*/*"]
  IntentFilterPtr filter5 = apps_util::MakeFileFilterForView("*/*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter5));
  EXPECT_FALSE(filter5->IsFileExtensionsFilter());

  // types: ["image/*"]
  IntentFilterPtr filter6 =
      apps_util::MakeFileFilterForView("image/*", "", kLabel);
  // Partial wild card is not generic.
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent, filter6));
  EXPECT_FALSE(filter6->IsFileExtensionsFilter());

  // types: ["*", "image/*"]
  IntentFilterPtr filter7 = apps_util::MakeFileFilterForView("*", "", kLabel);
  apps_util::AddConditionValue(apps::ConditionType::kFile, "image/*",
                               apps::PatternMatchType::kMimeType, filter7);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter7));
  EXPECT_FALSE(filter7->IsFileExtensionsFilter());

  // extensions: ["*"], types: ["image/*"]
  IntentFilterPtr filter8 =
      apps_util::MakeFileFilterForView("image/*", "*", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent, filter8));
  EXPECT_FALSE(filter8->IsFileExtensionsFilter());

  // types: ["text/*"] and target files contain unsupported text mime type, e.g.
  // text/calendar.
  IntentFilterPtr filter9 =
      apps_util::MakeFileFilterForView("text/*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent2, filter9));
  EXPECT_FALSE(filter9->IsFileExtensionsFilter());

  // types: ["text/*"] and target files don't contain unsupported text mime
  // type.
  IntentFilterPtr filter10 =
      apps_util::MakeFileFilterForView("text/*", "", kLabel);
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent, filter10));
  EXPECT_FALSE(filter10->IsFileExtensionsFilter());

  // File is a directory.
  IntentFilterPtr filter11 =
      apps_util::MakeFileFilterForView("text/*", "", kLabel);
  EXPECT_TRUE(apps_util::IsGenericFileHandler(intent3, filter11));
  EXPECT_FALSE(filter11->IsFileExtensionsFilter());

  // File is a directory, but filter is inode/directory.
  IntentFilterPtr filter12 =
      apps_util::MakeFileFilterForView("inode/directory", "", kLabel);
  EXPECT_FALSE(apps_util::IsGenericFileHandler(intent3, filter12));
  EXPECT_FALSE(filter12->IsFileExtensionsFilter());
}

TEST_F(IntentUtilTest, CloneIntent) {
  const std::string action = apps_util::kIntentActionSend;
  auto src_intent = std::make_unique<apps::Intent>(action);
  auto dst_intent = src_intent->Clone();
  EXPECT_EQ(action, dst_intent->action);
  EXPECT_FALSE(dst_intent->ui_bypassed.has_value());
  EXPECT_EQ(*src_intent, *dst_intent);

  GURL test_url1 = GURL("https://www.google.com/");
  GURL test_url2 = GURL("https://www.abc.com/");
  GURL test_url3 = GURL("https://www.foo.com/");
  const std::string mime_type = "image/jpeg";
  const std::string activity_name = "test";
  const std::string share_text = "share text";
  const std::string share_title = "share title";
  const std::string start_type = "start type";
  const std::string category1 = "category1";
  const std::string data = "data";
  const bool ui_bypassed = true;
  base::flat_map<std::string, std::string> extras = {
      {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

  auto file1 = std::make_unique<apps::IntentFile>(test_url1);
  auto file2 = std::make_unique<apps::IntentFile>(test_url2);
  std::vector<apps::IntentFilePtr> files;
  files.push_back(std::move(file1));
  files.push_back(std::move(file2));

  src_intent = CreateIntent(action, test_url1, mime_type, std::move(files),
                            activity_name, test_url3, share_text, share_title,
                            start_type, {category1}, data, ui_bypassed, extras);
  dst_intent = src_intent->Clone();

  EXPECT_EQ(action, dst_intent->action);
  EXPECT_EQ(test_url1, dst_intent->url.value());
  EXPECT_EQ(mime_type, dst_intent->mime_type.value());
  EXPECT_EQ(2u, dst_intent->files.size());
  EXPECT_EQ(test_url1, dst_intent->files[0]->url);
  EXPECT_EQ(test_url2, dst_intent->files[1]->url);
  EXPECT_EQ(activity_name, dst_intent->activity_name.value());
  EXPECT_EQ(test_url3, dst_intent->drive_share_url.value());
  EXPECT_EQ(share_text, dst_intent->share_text.value());
  EXPECT_EQ(share_title, dst_intent->share_title.value());
  EXPECT_EQ(start_type, dst_intent->start_type.value());
  EXPECT_EQ(1u, dst_intent->categories.size());
  EXPECT_EQ(category1, dst_intent->categories[0]);
  EXPECT_EQ(data, dst_intent->data.value());
  EXPECT_EQ(ui_bypassed, dst_intent->ui_bypassed);
  EXPECT_EQ(3u, dst_intent->extras.size());
  EXPECT_EQ(extras, dst_intent->extras);
  EXPECT_EQ(*src_intent, *dst_intent);
}

TEST_F(IntentUtilTest, IntentEqual) {
  auto intent1 = std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
  auto intent2 = std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
  EXPECT_EQ(*intent1, *intent2);

  GURL test_url1 = GURL("https://www.google.com/");
  GURL test_url2 = GURL("https://www.abc.com/");

  {
    // Verify whether intents are not equal when IntentFile is different.
    auto file1 = std::make_unique<apps::IntentFile>(test_url1);
    auto file2 = std::make_unique<apps::IntentFile>(test_url2);
    std::vector<apps::IntentFilePtr> files1;
    std::vector<apps::IntentFilePtr> files2;
    files1.push_back(std::move(file1));
    files2.push_back(std::move(file2));
    intent1->files = std::move(files1);
    intent2->files = std::move(files2);
    EXPECT_NE(*intent1, *intent2);
  }

  {
    // Verify whether intents are not equal when IntentFile is equal.
    auto file1 = std::make_unique<apps::IntentFile>(test_url1);
    auto file2 = std::make_unique<apps::IntentFile>(test_url1);
    std::vector<apps::IntentFilePtr> files1;
    std::vector<apps::IntentFilePtr> files2;
    files1.push_back(std::move(file1));
    files2.push_back(std::move(file2));
    intent1->files = std::move(files1);
    intent2->files = std::move(files2);
    EXPECT_EQ(*intent1, *intent2);
  }

  {
    // Verify whether intents are not equal when extras is different.
    base::flat_map<std::string, std::string> extras1 = {{"key1", "value1"}};
    base::flat_map<std::string, std::string> extras2 = {{"key2", "value2"}};
    intent1->extras = std::move(extras1);
    intent2->extras = std::move(extras2);
    EXPECT_NE(*intent1, *intent2);
  }

  {
    // Verify whether intents are not equal when extras is equal.
    base::flat_map<std::string, std::string> extras1 = {{"key1", "value1"}};
    base::flat_map<std::string, std::string> extras2 = {{"key1", "value1"}};
    intent1->extras = std::move(extras1);
    intent2->extras = std::move(extras2);
    EXPECT_EQ(*intent1, *intent2);
  }

  {
    // Verify whether intents are not equal when mime_type is different.
    intent1->mime_type = "image/jpeg";
    intent2->mime_type = "image/png";
    EXPECT_NE(*intent1, *intent2);
  }

  {
    // Verify whether intents are not equal when mime_type is different.
    intent1->mime_type = "image/jpeg";
    intent2->mime_type = "image/jpeg";
    EXPECT_EQ(*intent1, *intent2);
  }
}
