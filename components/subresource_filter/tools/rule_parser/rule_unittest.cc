// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/subresource_filter/tools/rule_parser/rule.h"

#include <stddef.h>
#include <memory>

#include "components/subresource_filter/tools/rule_parser/rule_options.h"
#include "components/subresource_filter/tools/rule_parser/rule_parser.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

constexpr auto kScript = url_pattern_index::proto::ELEMENT_TYPE_SCRIPT;
constexpr auto kImage = url_pattern_index::proto::ELEMENT_TYPE_IMAGE;
constexpr auto kPopup = url_pattern_index::proto::ELEMENT_TYPE_POPUP;
constexpr auto kWebsocket = url_pattern_index::proto::ELEMENT_TYPE_WEBSOCKET;

constexpr auto kAnchorNone = url_pattern_index::proto::ANCHOR_TYPE_NONE;

}  // namespace

TEST(RuleTest, DefaultUrlRule) {
  UrlRule rule;
  EXPECT_TRUE(rule.url_pattern.empty());
  EXPECT_EQ(url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING,
            rule.url_pattern_type);
  EXPECT_FALSE(rule.match_case);
  EXPECT_EQ(kAnchorNone, rule.anchor_left);
  EXPECT_EQ(kAnchorNone, rule.anchor_right);
  EXPECT_TRUE(rule.domains.empty());

  EXPECT_TRUE(rule.type_mask &
              type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_OTHER));
  EXPECT_TRUE(rule.type_mask & type_mask_for(kScript));
  EXPECT_TRUE(rule.type_mask & type_mask_for(kImage));
  EXPECT_FALSE(rule.type_mask & type_mask_for(kPopup));
  EXPECT_TRUE(rule.type_mask & type_mask_for(kWebsocket));

  EXPECT_FALSE(
      rule.type_mask &
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT));
  EXPECT_FALSE(
      rule.type_mask &
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_ELEMHIDE));
  EXPECT_FALSE(
      rule.type_mask &
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_GENERICBLOCK));
  EXPECT_FALSE(
      rule.type_mask &
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_GENERICHIDE));
}

TEST(RuleTest, CanonicalizeUrlPattern) {
  const struct {
    const char* rule;
    const char* expected_url_pattern;
    AnchorType expected_anchor_left;
    AnchorType expected_anchor_right;
  } kTestCases[] = {
      {"*/text/*", "/text/", kAnchorNone, kAnchorNone},
      {"*/text", "/text", kAnchorNone, kAnchorNone},
      {"text/*", "text/", kAnchorNone, kAnchorNone},
      {"*te*xt*", "te*xt", kAnchorNone, kAnchorNone},
      {"|*te*xt*", "te*xt", kAnchorNone,
       url_pattern_index::proto::ANCHOR_TYPE_NONE},
      {"*te*xt*|", "te*xt", kAnchorNone, kAnchorNone},
      {"|*te*xt*|", "te*xt", kAnchorNone, kAnchorNone},
      {"||*te*xt*", "*te*xt", url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN,
       kAnchorNone},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test_case.rule);

    RuleParser parser;
    ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_URL,
              parser.Parse(test_case.rule));

    const UrlRule& rule = parser.url_rule();
    EXPECT_EQ(test_case.expected_url_pattern, rule.url_pattern);
    EXPECT_EQ(test_case.expected_anchor_left, rule.anchor_left);
    EXPECT_EQ(test_case.expected_anchor_right, rule.anchor_right);
  }
}

TEST(RuleTest, CanonicalizeDomainList) {
  static const size_t kMaxDomainsCount = 3;
  static const char* const kTestCases[][kMaxDomainsCount] = {
      {"a.com", "c.com", "b.com"},
      {"a.com", "aa.aa.com", "long-example.com"},
      {"~sub.ex1.com", "ex2.com", "ex1.com"},
      {"example.com", "b.exmpl.com", "~a.b.example.com"},
      {"~example.com", "b.example.com", "~a.b.example.com"},
  };

  for (const auto& test_case : kTestCases) {
    std::vector<std::string> domains;
    size_t count = 0;
    for (; count < kMaxDomainsCount && test_case[count]; ++count)
      domains.push_back(test_case[count]);

    CanonicalizeDomainList(&domains);
    EXPECT_EQ(count, domains.size());
    for (size_t i = 1; i < domains.size(); ++i) {
      EXPECT_GE(domains[i - 1].size(), domains[i].size());
      if (domains[i - 1].size() == domains[i].size())
        EXPECT_LE(domains[i - 1], domains[i]);
    }
  }
}

TEST(RuleTest, UrlRuleToString) {
  UrlRule rule;
  rule.url_pattern = "domain*.example.com^";
  rule.anchor_left = url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN;
  rule.url_pattern_type = url_pattern_index::proto::URL_PATTERN_TYPE_WILDCARDED;
  rule.is_allowlist = true;
  rule.is_third_party = TriState::NO;
  rule.type_mask = kScript | kImage;
  rule.domains = {"example.com", "~exception.example.com"};

  EXPECT_EQ(
      "@@||domain*.example.com^$script,image,~third-party,"
      "domain=example.com|~exception.example.com",
      ToString(rule.ToProtobuf()));

  rule = UrlRule();
  rule.url_pattern = "example.com";
  rule.type_mask =
      url_pattern_index::proto::ELEMENT_TYPE_ALL & ~kImage & ~kPopup;
  EXPECT_EQ("example.com$~image", ToString(rule.ToProtobuf()));

  rule.type_mask = url_pattern_index::proto::ELEMENT_TYPE_ALL & ~kPopup;
  EXPECT_EQ("example.com", ToString(rule.ToProtobuf()));

  rule.type_mask = url_pattern_index::proto::ELEMENT_TYPE_ALL & ~kImage;
  EXPECT_EQ("example.com$~image,popup", ToString(rule.ToProtobuf()));

  rule.type_mask = url_pattern_index::proto::ELEMENT_TYPE_ALL & ~kWebsocket;
  EXPECT_EQ("example.com$~websocket,popup", ToString(rule.ToProtobuf()));

  rule.type_mask = kPopup;
  EXPECT_EQ("example.com$popup", ToString(rule.ToProtobuf()));

  rule.type_mask = kPopup | kImage;
  EXPECT_EQ("example.com$image,popup", ToString(rule.ToProtobuf()));

  rule.type_mask = kPopup | kWebsocket;
  EXPECT_EQ("example.com$websocket,popup", ToString(rule.ToProtobuf()));

  rule.type_mask =
      url_pattern_index::proto::ELEMENT_TYPE_SUBDOCUMENT |
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT);
  EXPECT_EQ("example.com$subdocument,document", ToString(rule.ToProtobuf()));

  rule.type_mask =
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT);
  EXPECT_EQ("example.com$document", ToString(rule.ToProtobuf()));

  rule.type_mask =
      (url_pattern_index::proto::ELEMENT_TYPE_ALL & ~kImage & ~kPopup) |
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT);
  EXPECT_EQ("example.com$~image,document", ToString(rule.ToProtobuf()));

  // A workaround for when no type is specified. This is to avoid ambiguity with
  // the pure "example.com" rule which targets a bunch of default element types.
  rule.type_mask = 0;
  EXPECT_EQ("example.com$image,~image", ToString(rule.ToProtobuf()));
}

TEST(RuleTest, CssRuleToString) {
  CssRule rule;
  rule.is_allowlist = true;
  rule.domains = {"example.com", "~exception.example.com"};
  rule.css_selector = "#example-id";

  EXPECT_EQ("example.com,~exception.example.com#@##example-id",
            ToString(rule.ToProtobuf()));
}

}  // namespace subresource_filter
