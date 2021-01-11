// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/url_rule_util.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/url_pattern_index/url_pattern.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "components/url_pattern_index/url_rule_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace url_pattern_index {

namespace {

proto::UrlRule MakeProtoRule(proto::RuleSemantics semantics,
                             const UrlPattern& url_pattern,
                             proto::SourceType source_type,
                             proto::ElementType types,
                             const std::vector<std::string>& domains) {
  proto::UrlRule rule;

  rule.set_semantics(semantics);
  rule.set_source_type(source_type);
  rule.set_element_types(types);

  rule.set_url_pattern_type(url_pattern.type());
  rule.set_anchor_left(url_pattern.anchor_left());
  rule.set_anchor_right(url_pattern.anchor_right());
  rule.set_match_case(url_pattern.match_case());
  rule.set_url_pattern(url_pattern.url_pattern().as_string());

  testing::AddDomains(domains, &rule);

  return rule;
}

struct RuleTest {
  const char* rule_string;
  const char* match_string;
};

class UrlRuleUtilTest : public ::testing::Test {
 protected:
  UrlRuleUtilTest() = default;

  const flat::UrlRule* MakeFlatRule(const proto::UrlRule& rule) {
    auto offset =
        url_pattern_index::SerializeUrlRule(rule, &flat_builder_, &domain_map_);
    return flatbuffers::GetTemporaryPointer(flat_builder_, offset);
  }

  const flat::UrlRule* MakeFlatRule(const std::string& pattern,
                                    uint16_t flat_element_types_mask) {
    auto pattern_offset = flat_builder_.CreateString(pattern);

    flat::UrlRuleBuilder rule_builder(flat_builder_);
    rule_builder.add_url_pattern(pattern_offset);
    rule_builder.add_element_types(flat_element_types_mask);
    auto offset = rule_builder.Finish();

    return flatbuffers::GetTemporaryPointer(flat_builder_, offset);
  }

  flatbuffers::FlatBufferBuilder flat_builder_;

  FlatDomainMap domain_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlRuleUtilTest);
};

TEST_F(UrlRuleUtilTest, Blacklist) {
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/", FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, Whitelist) {
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_WHITELIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("@@example.com/", FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, LeftAnchor) {
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST,
                    UrlPattern("example.com/", proto::ANCHOR_TYPE_NONE,
                               proto::ANCHOR_TYPE_NONE),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/", FlatUrlRuleToFilterlistString(flat_rule));

  flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST,
                    UrlPattern("example.com/", proto::ANCHOR_TYPE_BOUNDARY,
                               proto::ANCHOR_TYPE_NONE),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("|example.com/", FlatUrlRuleToFilterlistString(flat_rule));

  flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST,
                    UrlPattern("example.com/", proto::ANCHOR_TYPE_SUBDOMAIN,
                               proto::ANCHOR_TYPE_NONE),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("||example.com/", FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, RightAnchor) {
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST,
                    UrlPattern("example.com", proto::ANCHOR_TYPE_NONE,
                               proto::ANCHOR_TYPE_NONE),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com", FlatUrlRuleToFilterlistString(flat_rule));

  flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST,
                    UrlPattern("example.com", proto::ANCHOR_TYPE_NONE,
                               proto::ANCHOR_TYPE_BOUNDARY),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com|", FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, BothSidesAnchored) {
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST,
                    UrlPattern("example.com", proto::ANCHOR_TYPE_SUBDOMAIN,
                               proto::ANCHOR_TYPE_BOUNDARY),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("||example.com|", FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, NonRegex) {
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("/foo/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("/foo/*", FlatUrlRuleToFilterlistString(flat_rule));

  // Show that whitelist rules work too.
  flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_WHITELIST, UrlPattern("/foo/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("@@/foo/*", FlatUrlRuleToFilterlistString(flat_rule));

  // TODO(jkarlin): If regex support is added to UrlRule, verify that regex
  // rules don't get the '*' appended.
}

TEST_F(UrlRuleUtilTest, Party) {
  const flat::UrlRule* flat_rule = MakeFlatRule(MakeProtoRule(
      proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
      proto::SOURCE_TYPE_THIRD_PARTY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/$third-party",
            FlatUrlRuleToFilterlistString(flat_rule));

  flat_rule = MakeFlatRule(MakeProtoRule(
      proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
      proto::SOURCE_TYPE_FIRST_PARTY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/$~third-party",
            FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, MultipleOptions) {
  const flat::UrlRule* flat_rule = MakeFlatRule(MakeProtoRule(
      proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
      proto::SOURCE_TYPE_THIRD_PARTY, proto::ELEMENT_TYPE_SCRIPT, {}));
  EXPECT_EQ("example.com/$third-party,script",
            FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, ElementType) {
  // Test a single type.
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_SCRIPT, {}));
  EXPECT_EQ("example.com/$script", FlatUrlRuleToFilterlistString(flat_rule));

  // Test blocking every type.
  flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/", FlatUrlRuleToFilterlistString(flat_rule));

  // Block everything except other. This test will need to be updated as
  // proto::ElementType is changed.
  flat_rule = MakeFlatRule(MakeProtoRule(
      proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
      proto::SOURCE_TYPE_ANY,
      static_cast<proto::ElementType>(proto::ELEMENT_TYPE_ALL - 1), {}));
  std::string expected =
      "example.com/"
      "$script,image,stylesheet,object,xmlhttprequest,object-subrequest,"
      "subdocument,ping,media,font,websocket";

  EXPECT_EQ(expected, FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, ActivationType) {
  // Test with no activiation type.
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/", FlatUrlRuleToFilterlistString(flat_rule));

  // Test with a document activation type.
  auto proto_rule =
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {});
  proto_rule.set_activation_types(proto::ACTIVATION_TYPE_DOCUMENT);
  flat_rule = MakeFlatRule(proto_rule);
  EXPECT_EQ("example.com/$document", FlatUrlRuleToFilterlistString(flat_rule));

  // Test with Document & Generic block types.
  proto_rule.set_activation_types(proto::ACTIVATION_TYPE_DOCUMENT |
                                  proto::ACTIVATION_TYPE_GENERICBLOCK);
  flat_rule = MakeFlatRule(proto_rule);
  EXPECT_EQ("example.com/$document,genericblock",
            FlatUrlRuleToFilterlistString(flat_rule));
}

TEST_F(UrlRuleUtilTest, DomainList) {
  //  Test with no domains set.
  const flat::UrlRule* flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL, {}));
  EXPECT_EQ("example.com/", FlatUrlRuleToFilterlistString(flat_rule));

  // Test with domains set.
  flat_rule = MakeFlatRule(
      MakeProtoRule(proto::RULE_SEMANTICS_BLACKLIST, UrlPattern("example.com/"),
                    proto::SOURCE_TYPE_ANY, proto::ELEMENT_TYPE_ALL,
                    {"foo.example.com", "~bar.example.com"}));
  EXPECT_EQ("example.com/$domain=foo.example.com|~bar.example.com",
            FlatUrlRuleToFilterlistString(flat_rule));
}

// Ensures that MAIN_FRAME and CSP_REPORT types are ignored since Filterlist
// does not support these.
TEST_F(UrlRuleUtilTest, IgnoredTypes) {
  const flat::UrlRule* flat_rule =
      MakeFlatRule("example.com/", flat::ElementType_MAIN_FRAME |
                                       flat::ElementType_CSP_REPORT |
                                       flat::ElementType_SCRIPT);

  EXPECT_EQ("example.com/$script", FlatUrlRuleToFilterlistString(flat_rule));
}

}  // namespace

}  // namespace url_pattern_index
