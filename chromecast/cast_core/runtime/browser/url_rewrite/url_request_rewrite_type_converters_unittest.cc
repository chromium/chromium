// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"

#include <optional>

#include "base/run_loop.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_validation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cast_core/public/src/proto/v2/url_rewrite.pb.h"

namespace chromecast {
namespace {

using ::testing::SizeIs;

cast::v2::UrlRequestRewrite CreateRewriteAddHeaders(std::string header_name,
                                                    std::string header_value) {
  cast::v2::UrlRequestRewrite rewrite;
  cast::v2::UrlHeader* url_header =
      rewrite.mutable_add_headers()->mutable_headers()->Add();
  url_header->set_name(std::move(header_name));
  url_header->set_value(std::move(header_value));
  return rewrite;
}

cast::v2::UrlRequestRewrite CreateRewriteRemoveHeader(
    std::optional<std::string> query_pattern,
    std::string header_name) {
  cast::v2::UrlRequestRewrite rewrite;
  auto* remove_header = rewrite.mutable_remove_header();
  if (query_pattern)
    remove_header->set_query_pattern(std::move(query_pattern).value());
  remove_header->set_header_name(std::move(header_name));
  return rewrite;
}

cast::v2::UrlRequestRewrite CreateRewriteSubstituteQueryPattern(
    std::string pattern,
    std::string substitution) {
  cast::v2::UrlRequestRewrite rewrite;
  auto* substitute_query_pattern = rewrite.mutable_substitute_query_pattern();
  substitute_query_pattern->set_pattern(std::move(pattern));
  substitute_query_pattern->set_substitution(std::move(substitution));
  return rewrite;
}

cast::v2::UrlRequestRewrite CreateRewriteReplaceUrl(std::string url_ends_with,
                                                    std::string new_url) {
  cast::v2::UrlRequestRewrite rewrite;
  auto* replace_url = rewrite.mutable_replace_url();
  replace_url->set_url_ends_with(std::move(url_ends_with));
  replace_url->set_new_url(std::move(new_url));
  return rewrite;
}

cast::v2::UrlRequestRewrite CreateRewriteAppendToQuery(std::string query) {
  cast::v2::UrlRequestRewrite rewrite;
  auto* append_to_query = rewrite.mutable_append_to_query();
  append_to_query->set_query(std::move(query));
  return rewrite;
}

class UrlRequestRewriteTypeConvertersTest : public testing::Test {
 public:
  UrlRequestRewriteTypeConvertersTest() = default;

  UrlRequestRewriteTypeConvertersTest(
      const UrlRequestRewriteTypeConvertersTest&) = delete;
  UrlRequestRewriteTypeConvertersTest& operator=(
      const UrlRequestRewriteTypeConvertersTest&) = delete;

  ~UrlRequestRewriteTypeConvertersTest() override = default;

 protected:
  bool UpdateRulesFromRewrite(cast::v2::UrlRequestRewrite rewrite) {
    cast::v2::UrlRequestRewriteRules rules;
    rules.mutable_rules()->Add()->mutable_rewrites()->Add(std::move(rewrite));
    cached_rules_ =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
            std::move(rules));
    return url_rewrite::ValidateRules(cached_rules_.get());
  }

  url_rewrite::mojom::UrlRequestRewriteRulesPtr cached_rules_;
};

// Tests AddHeaders rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteTypeConvertersTest, ConvertAddHeader) {
  EXPECT_TRUE(UpdateRulesFromRewrite(CreateRewriteAddHeaders("Test", "Value")));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules_->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules_->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_add_headers());

  const std::vector<url_rewrite::mojom::UrlHeaderPtr>& headers =
      cached_rules_->rules[0]->actions[0]->get_add_headers()->headers;
  ASSERT_THAT(headers, SizeIs(1));
  ASSERT_EQ(headers[0]->name, "Test");
  ASSERT_EQ(headers[0]->value, "Value");
}

// Tests RemoveHeader rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteTypeConvertersTest, ConvertRemoveHeader) {
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteRemoveHeader(std::make_optional("Test"), "Header")));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules_->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules_->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_remove_header());

  const url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header1 =
      cached_rules_->rules[0]->actions[0]->get_remove_header();
  ASSERT_TRUE(remove_header1->query_pattern);
  ASSERT_EQ(remove_header1->query_pattern.value().compare("Test"), 0);
  ASSERT_EQ(remove_header1->header_name.compare("Header"), 0);

  // Create a RemoveHeader rewrite with no pattern.
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteRemoveHeader(std::nullopt, "Header")));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules_->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules_->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_remove_header());

  const url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header2 =
      cached_rules_->rules[0]->actions[0]->get_remove_header();
  ASSERT_FALSE(remove_header2->query_pattern);
  ASSERT_EQ(remove_header2->header_name.compare("Header"), 0);
}

// Tests SubstituteQueryPattern rewrites are properly converted to their Mojo
// equivalent.
TEST_F(UrlRequestRewriteTypeConvertersTest, ConvertSubstituteQueryPattern) {
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteSubstituteQueryPattern("Pattern", "Substitution")));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules_->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules_->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(
      cached_rules_->rules[0]->actions[0]->is_substitute_query_pattern());

  const url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr&
      substitute_query_pattern =
          cached_rules_->rules[0]->actions[0]->get_substitute_query_pattern();
  ASSERT_EQ(substitute_query_pattern->pattern.compare("Pattern"), 0);
  ASSERT_EQ(substitute_query_pattern->substitution.compare("Substitution"), 0);
}

// Tests ReplaceUrl rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteTypeConvertersTest, ConvertReplaceUrl) {
  GURL url("http://site.xyz");
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("/something", url.spec())));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules_->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules_->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_replace_url());

  const url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr& replace_url =
      cached_rules_->rules[0]->actions[0]->get_replace_url();
  ASSERT_EQ(replace_url->url_ends_with.compare("/something"), 0);
  ASSERT_EQ(replace_url->new_url.spec().compare(url.spec()), 0);
}

// Tests AppendToQuery rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteTypeConvertersTest, ConvertAppendToQuery) {
  EXPECT_TRUE(
      UpdateRulesFromRewrite(CreateRewriteAppendToQuery("foo=bar&foo")));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules_->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules_->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_append_to_query());

  const url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr& append_to_query =
      cached_rules_->rules[0]->actions[0]->get_append_to_query();
  ASSERT_EQ(append_to_query->query.compare("foo=bar&foo"), 0);
}

// Tests validation is working as expected.
TEST_F(UrlRequestRewriteTypeConvertersTest, Validation) {
  // Empty rewrite.
  EXPECT_FALSE(UpdateRulesFromRewrite(cast::v2::UrlRequestRewrite()));

  // Invalid AddHeaders header name.
  EXPECT_FALSE(
      UpdateRulesFromRewrite(CreateRewriteAddHeaders("Te\nst1", "Value")));

  // Invalid AddHeaders header value.
  EXPECT_FALSE(
      UpdateRulesFromRewrite(CreateRewriteAddHeaders("Test1", "Val\nue")));

  // Empty AddHeaders.
  {
    cast::v2::UrlRequestRewrite rewrite;
    rewrite.mutable_add_headers();
    EXPECT_FALSE(UpdateRulesFromRewrite(std::move(rewrite)));
  }

  // Invalid RemoveHeader header name.
  EXPECT_FALSE(
      UpdateRulesFromRewrite(CreateRewriteRemoveHeader("Query", "Head\ner")));

  // Empty RemoveHeader.
  {
    cast::v2::UrlRequestRewrite rewrite;
    rewrite.mutable_remove_header();
    EXPECT_FALSE(UpdateRulesFromRewrite(std::move(rewrite)));
  }

  // Empty SubstituteQueryPattern.
  {
    cast::v2::UrlRequestRewrite rewrite;
    rewrite.mutable_substitute_query_pattern();
    EXPECT_FALSE(UpdateRulesFromRewrite(std::move(rewrite)));
  }

  // ReplaceURL with valid new_url.
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("/something", "http://site.xyz")));
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("some%00thing", "http://site.xyz")));

  // ReplaceURL with valid new_url including "%00" in its path.
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("/something", "http://site.xyz/%00")));
  EXPECT_TRUE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("some%00thing", "http://site.xyz/%00")));

  // ReplaceURL with invalid new_url.
  EXPECT_FALSE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("/something", "http:site:xyz")));
  EXPECT_FALSE(UpdateRulesFromRewrite(
      CreateRewriteReplaceUrl("some%00thing", "http:site:xyz")));

  // Empty ReplaceUrl.
  {
    cast::v2::UrlRequestRewrite rewrite;
    rewrite.mutable_replace_url();
    EXPECT_FALSE(UpdateRulesFromRewrite(std::move(rewrite)));
  }

  // Empty AppendToQuery.
  {
    cast::v2::UrlRequestRewrite rewrite;
    rewrite.mutable_append_to_query();
    EXPECT_FALSE(UpdateRulesFromRewrite(std::move(rewrite)));
  }
}

// Tests rules are properly renewed after new rules are sent.
TEST_F(UrlRequestRewriteTypeConvertersTest, RuleRenewal) {
  EXPECT_TRUE(
      UpdateRulesFromRewrite(CreateRewriteAddHeaders("Test1", "Value")));
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_add_headers());
  ASSERT_THAT(cached_rules_->rules[0]->actions[0]->get_add_headers()->headers,
              SizeIs(1));
  ASSERT_EQ(
      cached_rules_->rules[0]->actions[0]->get_add_headers()->headers[0]->name,
      "Test1");

  EXPECT_TRUE(
      UpdateRulesFromRewrite(CreateRewriteAddHeaders("Test2", "Value")));

  // We should have the new rules.
  ASSERT_THAT(cached_rules_->rules, SizeIs(1));
  ASSERT_THAT(cached_rules_->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules_->rules[0]->actions[0]->is_add_headers());
  ASSERT_THAT(cached_rules_->rules[0]->actions[0]->get_add_headers()->headers,
              SizeIs(1));
  ASSERT_EQ(
      cached_rules_->rules[0]->actions[0]->get_add_headers()->headers[0]->name,
      "Test2");
}

}  // namespace
}  // namespace chromecast
