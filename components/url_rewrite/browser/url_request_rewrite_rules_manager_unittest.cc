// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_rewrite {
namespace {

using ::testing::ContainerEq;
using ::testing::IsNull;
using ::testing::SizeIs;

TEST(UrlRequestRewriteRulesManagerTest, OnRulesUpdatedSucceeds) {
  url_rewrite::UrlRequestRewriteRulesManager url_request_rewrite_rules_manager;
  ASSERT_THAT(url_request_rewrite_rules_manager.GetCachedRules(), IsNull());

  auto add_headers = mojom::UrlRequestRewriteAddHeaders::New();
  auto header = mojom::UrlHeader::New("Test", "Value");
  add_headers->headers.push_back(std::move(header));
  auto rule = mojom::UrlRequestRule::New();
  rule->hosts_filter.emplace(std::vector<std::string>{"foo.bar"});
  rule->schemes_filter.emplace(std::vector<std::string>{"https"});
  rule->actions.push_back(
      mojom::UrlRequestAction::NewAddHeaders(std::move(add_headers)));
  auto rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));
  ASSERT_TRUE(
      url_request_rewrite_rules_manager.OnRulesUpdated(std::move(rules)));

  // Verify the rules got updated.
  mojom::UrlRequestRewriteRules* cached_rules =
      url_request_rewrite_rules_manager.GetCachedRules()->data.get();
  ASSERT_THAT(cached_rules->rules, SizeIs(1));
  ASSERT_TRUE(cached_rules->rules[0]->hosts_filter);
  ASSERT_THAT(cached_rules->rules[0]->hosts_filter.value(),
              ContainerEq(std::vector<std::string>{"foo.bar"}));
  ASSERT_TRUE(cached_rules->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules->rules[0]->schemes_filter.value(),
              ContainerEq(std::vector<std::string>{"https"}));
  ASSERT_THAT(cached_rules->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules->rules[0]->actions[0]->is_add_headers());

  const std::vector<url_rewrite::mojom::UrlHeaderPtr>& headers =
      cached_rules->rules[0]->actions[0]->get_add_headers()->headers;
  ASSERT_THAT(headers, SizeIs(1));
  ASSERT_EQ(headers[0]->name, "Test");
  ASSERT_EQ(headers[0]->value, "Value");
}

TEST(UrlRequestRewriteRulesManagerTest, OnRulesUpdatedFailsWithInvalidRules) {
  url_rewrite::UrlRequestRewriteRulesManager url_request_rewrite_rules_manager;
  ASSERT_THAT(url_request_rewrite_rules_manager.GetCachedRules(), IsNull());

  auto remove_header =
      mojom::UrlRequestRewriteRemoveHeader::New("Query", "TestHeader");
  auto rule = mojom::UrlRequestRule::New();
  rule->actions.push_back(
      mojom::UrlRequestAction::NewRemoveHeader(std::move(remove_header)));
  auto rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));
  ASSERT_TRUE(
      url_request_rewrite_rules_manager.OnRulesUpdated(std::move(rules)));

  // Verify the rule got updated.
  mojom::UrlRequestRewriteRules* cached_rules =
      url_request_rewrite_rules_manager.GetCachedRules()->data.get();
  ASSERT_THAT(cached_rules->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules->rules[0]->actions[0]->is_remove_header());
  EXPECT_EQ(
      cached_rules->rules[0]->actions[0]->get_remove_header()->header_name,
      "TestHeader");
  ASSERT_TRUE(
      cached_rules->rules[0]->actions[0]->get_remove_header()->query_pattern);
  EXPECT_EQ(
      *cached_rules->rules[0]->actions[0]->get_remove_header()->query_pattern,
      "Query");

  // Verify the rules are not reset if validation fails.
  rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(mojom::UrlRequestRule::New());
  ASSERT_FALSE(
      url_request_rewrite_rules_manager.OnRulesUpdated(std::move(rules)));
  mojom::UrlRequestRewriteRules* updated_rules =
      url_request_rewrite_rules_manager.GetCachedRules()->data.get();
  EXPECT_EQ(updated_rules, cached_rules);
}

TEST(UrlRequestRewriteRulesManagerTest, OnRulesRenewal) {
  url_rewrite::UrlRequestRewriteRulesManager url_request_rewrite_rules_manager;
  ASSERT_THAT(url_request_rewrite_rules_manager.GetCachedRules(), IsNull());

  auto append_to_query =
      mojom::UrlRequestRewriteAppendToQuery::New("TestQuery");
  auto rule = mojom::UrlRequestRule::New();
  rule->actions.push_back(
      mojom::UrlRequestAction::NewAppendToQuery(std::move(append_to_query)));
  auto rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));
  ASSERT_TRUE(
      url_request_rewrite_rules_manager.OnRulesUpdated(std::move(rules)));

  // Verify the rule got updated.
  mojom::UrlRequestRewriteRules* cached_rules =
      url_request_rewrite_rules_manager.GetCachedRules()->data.get();
  ASSERT_THAT(cached_rules->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(cached_rules->rules[0]->actions[0]->is_append_to_query());
  EXPECT_EQ(cached_rules->rules[0]->actions[0]->get_append_to_query()->query,
            "TestQuery");

  // Verify the rules gets updated.
  auto substitute =
      mojom::UrlRequestRewriteSubstituteQueryPattern::New("Pattern", "Sub");
  rule = mojom::UrlRequestRule::New();
  rule->actions.push_back(mojom::UrlRequestAction::NewSubstituteQueryPattern(
      std::move(substitute)));
  rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));
  ASSERT_TRUE(
      url_request_rewrite_rules_manager.OnRulesUpdated(std::move(rules)));

  cached_rules = url_request_rewrite_rules_manager.GetCachedRules()->data.get();
  ASSERT_THAT(cached_rules->rules, SizeIs(1));
  ASSERT_FALSE(cached_rules->rules[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->rules[0]->schemes_filter);
  ASSERT_THAT(cached_rules->rules[0]->actions, SizeIs(1));
  ASSERT_TRUE(
      cached_rules->rules[0]->actions[0]->is_substitute_query_pattern());
  EXPECT_EQ(cached_rules->rules[0]
                ->actions[0]
                ->get_substitute_query_pattern()
                ->pattern,
            "Pattern");
  EXPECT_EQ(cached_rules->rules[0]
                ->actions[0]
                ->get_substitute_query_pattern()
                ->substitution,
            "Sub");
}

}  // namespace
}  // namespace url_rewrite
