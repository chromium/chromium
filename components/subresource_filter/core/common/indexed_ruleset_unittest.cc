// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/url_pattern_index/url_pattern.h"
#include "components/url_pattern_index/url_rule_test_support.h"
#include "components/url_pattern_index/url_rule_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

using ::subresource_filter::testing::StyleRuleParams;

namespace proto = url_pattern_index::proto;
using url_pattern_index::UrlPattern;
using url_pattern_index::testing::MakeUrlRule;

class SubresourceFilterIndexedRulesetTest : public ::testing::Test {
 public:
  SubresourceFilterIndexedRulesetTest() { Reset(); }

  SubresourceFilterIndexedRulesetTest(
      const SubresourceFilterIndexedRulesetTest&) = delete;
  SubresourceFilterIndexedRulesetTest& operator=(
      const SubresourceFilterIndexedRulesetTest&) = delete;

 protected:
  LoadPolicy GetLoadPolicy(
      std::string_view url,
      std::string_view document_origin = "",
      proto::ElementType element_type = url_pattern_index::testing::kOther,
      bool disable_generic_rules = false,
      const url_pattern_index::flat::UrlRule** out_rule = nullptr) const {
    CHECK(matcher_);
    return matcher_->GetLoadPolicyForResourceLoad(
        GURL(url),
        FirstPartyOrigin(
            url_pattern_index::testing::GetOrigin(document_origin)),
        element_type, disable_generic_rules, out_rule);
  }

  bool MatchingRule(
      std::string_view url,
      std::string_view document_origin = "",
      proto::ElementType element_type = url_pattern_index::testing::kOther,
      bool disable_generic_rules = false) const {
    CHECK(matcher_);
    return matcher_->MatchedUrlRule(
               GURL(url),
               FirstPartyOrigin(
                   url_pattern_index::testing::GetOrigin(document_origin)),
               element_type, disable_generic_rules) != nullptr;
  }

  bool ShouldDeactivate(std::string_view document_url,
                        std::string_view parent_document_origin = "",
                        proto::ActivationType activation_type =
                            url_pattern_index::testing::kNoActivation) const {
    CHECK(matcher_);
    return matcher_->ShouldDisableFilteringForDocument(
        GURL(document_url),
        url_pattern_index::testing::GetOrigin(parent_document_origin),
        activation_type);
  }

  bool AddUrlRule(const proto::UrlRule& rule) {
    return indexer_->AddUrlRule(rule);
  }

  bool AddSimpleRule(std::string_view url_pattern) {
    return AddUrlRule(MakeUrlRule(
        UrlPattern(url_pattern, url_pattern_index::testing::kSubstring)));
  }

  bool AddSimpleAllowlistRule(std::string_view url_pattern) {
    auto rule = MakeUrlRule(
        UrlPattern(url_pattern, url_pattern_index::testing::kSubstring));
    rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
    return AddUrlRule(rule);
  }

  bool AddSimpleAllowlistRule(std::string_view url_pattern,
                              int32_t activation_types) {
    auto rule = MakeUrlRule(
        UrlPattern(url_pattern, url_pattern_index::testing::kSubstring));
    rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
    rule.clear_element_types();
    rule.set_activation_types(activation_types);
    return AddUrlRule(rule);
  }

  bool AddStyleRule(const StyleRuleParams& params) {
    return indexer_->AddStyleRuleFromProto(testing::CreateStyleRule(params));
  }

  void Finish() {
    indexer_->Finish();
    matcher_ = std::make_unique<IndexedRulesetMatcher>(indexer_->data());
  }

  void Reset() {
    matcher_.reset(nullptr);
    indexer_ = std::make_unique<RulesetIndexer>(0);
  }

  std::unique_ptr<RulesetIndexer> indexer_;
  std::unique_ptr<IndexedRulesetMatcher> matcher_;
};

TEST_F(SubresourceFilterIndexedRulesetTest, EmptyRuleset) {
  Finish();
  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy(""));
  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("http://example.com"));
  EXPECT_EQ(LoadPolicy::ALLOW,
            GetLoadPolicy("http://another.example.com?param=val"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, NoRuleApplies) {
  ASSERT_TRUE(AddSimpleRule("?filter_out="));
  ASSERT_TRUE(AddSimpleRule("&filter_out="));
  Finish();

  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("http://example.com"));
  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("http://example.com?filter_not"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, NoRuleApplies_OutRuleParameter) {
  ASSERT_TRUE(AddSimpleRule("?filter_out="));
  Finish();

  const url_pattern_index::flat::UrlRule* rule = nullptr;
  EXPECT_EQ(LoadPolicy::ALLOW,
            GetLoadPolicy("http://example.com", /*document_origin=*/"",
                          /*=element_type=*/url_pattern_index::testing::kOther,
                          /*disable_generic_rules=*/false, /*out_rule=*/&rule));
  EXPECT_FALSE(rule);
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlocklist) {
  ASSERT_TRUE(AddSimpleRule("?param="));
  Finish();

  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("https://example.com"));
  EXPECT_EQ(LoadPolicy::DISALLOW,
            GetLoadPolicy("http://example.org?param=image1"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlocklist_OutRuleParameter) {
  ASSERT_TRUE(AddSimpleRule("?param="));
  Finish();

  const url_pattern_index::flat::UrlRule* rule = nullptr;
  EXPECT_EQ(
      LoadPolicy::DISALLOW,
      GetLoadPolicy("http://example.com?param=image1", /*document_origin=*/"",
                    /*=element_type=*/url_pattern_index::testing::kOther,
                    /*disable_generic_rules=*/false, /*out_rule=*/&rule));
  EXPECT_TRUE(rule);
  EXPECT_EQ(url_pattern_index::FlatUrlRuleToFilterlistString(rule), "?param=");
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlocklistSubdocument) {
  ASSERT_TRUE(AddSimpleRule("?param="));
  Finish();

  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("https://example.com"));
  EXPECT_EQ(LoadPolicy::DISALLOW,
            GetLoadPolicy("http://example.org?param=image1",
                          /*document_origin=*/"",
                          url_pattern_index::testing::kSubdocument));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleAllowlist) {
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  // This should not return EXPLICITLY_ALLOW because there is no corresponding
  // blocklist rule for the allowlist rule. To optimize speed, allowlist rules
  // are only checked if a rule was matched with a blocklist rule unless it
  // is a subdocument resource.
  EXPECT_EQ(LoadPolicy::ALLOW,
            GetLoadPolicy("https://example.com?filter_out=true"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleAllowlist_OutRuleParameter) {
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  const url_pattern_index::flat::UrlRule* rule = nullptr;
  EXPECT_EQ(LoadPolicy::ALLOW,
            GetLoadPolicy("https://example.com?filter_out=true",
                          /*document_origin=*/"",
                          /*=element_type=*/url_pattern_index::testing::kOther,
                          /*disable_generic_rules=*/false, /*out_rule=*/&rule));
  EXPECT_FALSE(rule);
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleAllowlistSubdocument) {
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  // Verify allowlist rules are always checked for subdocument element types.
  EXPECT_EQ(LoadPolicy::EXPLICITLY_ALLOW,
            GetLoadPolicy("https://example.com?filter_out=true",
                          /*document_origin=*/"",
                          url_pattern_index::testing::kSubdocument));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       SimpleAllowlistWithMatchingBlocklist) {
  ASSERT_TRUE(AddSimpleRule("example.com/?filter_out="));
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  EXPECT_EQ(LoadPolicy::EXPLICITLY_ALLOW,
            GetLoadPolicy("https://example.com?filter_out=true"));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       SimpleAllowlistWithMatchingBlocklist_OutRuleParameter) {
  ASSERT_TRUE(AddSimpleRule("example.com/?filter_out="));
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  const url_pattern_index::flat::UrlRule* rule = nullptr;
  EXPECT_EQ(LoadPolicy::EXPLICITLY_ALLOW,
            GetLoadPolicy("https://example.com?filter_out=true",
                          /*document_origin=*/"",
                          /*=element_type=*/url_pattern_index::testing::kOther,
                          /*disable_generic_rules=*/false, /*out_rule=*/&rule));
  EXPECT_FALSE(rule);
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       SimpleAllowlistWithMatchingBlocklistSubdocument) {
  ASSERT_TRUE(AddSimpleRule("example.com/?filter_out="));
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  EXPECT_EQ(LoadPolicy::EXPLICITLY_ALLOW,
            GetLoadPolicy("https://example.com?filter_out=true",
                          /*document_origin=*/"",
                          url_pattern_index::testing::kSubdocument));
}

// Ensure patterns containing non-ascii characters are disallowed.
TEST_F(SubresourceFilterIndexedRulesetTest, NonAsciiPatterns) {
  // non-ascii character é.
  std::string non_ascii = base::WideToUTF8(L"\u00E9");
  ASSERT_FALSE(AddSimpleRule(non_ascii));
  Finish();

  EXPECT_EQ(LoadPolicy::ALLOW,
            GetLoadPolicy("https://example.com/q=" + non_ascii));
}

// Ensure that specifying non-ascii characters in percent encoded form in
// patterns works.
TEST_F(SubresourceFilterIndexedRulesetTest, PercentEncodedPatterns) {
  // Percent encoded form of é.
  ASSERT_TRUE(AddSimpleRule("%C3%A9"));
  Finish();

  EXPECT_EQ(LoadPolicy::DISALLOW, GetLoadPolicy("https://example.com/q=" +
                                                base::WideToUTF8(L"\u00E9")));
}

// Ensures that specifying patterns in punycode works for matching IDN domains.
TEST_F(SubresourceFilterIndexedRulesetTest, IDNHosts) {
  // ҏӊԟҭв.com
  const std::string punycode = "xn--b1a9p8c1e8r.com";
  ASSERT_TRUE(AddSimpleRule(punycode));
  Finish();

  EXPECT_EQ(LoadPolicy::DISALLOW, GetLoadPolicy("https://" + punycode));
  EXPECT_EQ(LoadPolicy::DISALLOW,
            GetLoadPolicy(base::WideToUTF8(
                L"https://\x048f\x04ca\x051f\x04ad\x0432.com")));
}

// Ensure patterns containing non-ascii domains are disallowed.
TEST_F(SubresourceFilterIndexedRulesetTest, NonAsciiDomain) {
  const char* kUrl = "http://example.com";

  // ґғ.com
  std::string non_ascii_domain = base::WideToUTF8(L"\x0491\x0493.com");

  auto rule =
      MakeUrlRule(UrlPattern(kUrl, url_pattern_index::testing::kSubstring));
  url_pattern_index::testing::AddInitiatorDomains({non_ascii_domain}, &rule);
  ASSERT_FALSE(AddUrlRule(rule));

  rule = MakeUrlRule(UrlPattern(kUrl, url_pattern_index::testing::kSubstring));
  std::string non_ascii_excluded_domain = "~" + non_ascii_domain;
  url_pattern_index::testing::AddInitiatorDomains({non_ascii_excluded_domain},
                                                  &rule);
  ASSERT_FALSE(AddUrlRule(rule));

  Finish();
}

// Ensure patterns with percent encoded hosts match correctly.
//
// Warning: This test depends on the standard non-compliant URL behavior in
// Chrome. Currently, Chrome escapes '*' (%2A) character in URL host, but this
// behavior is non-compliant. See https://crbug.com/1416013 for details. We
// probably no longer need this test once https://crbug.com/1416013 is fixed.
TEST_F(SubresourceFilterIndexedRulesetTest, PercentEncodedHostPattern) {
  const char* kPercentEncodedHost = "http://%2A.com/";
  ASSERT_TRUE(AddSimpleRule(kPercentEncodedHost));
  Finish();

  EXPECT_EQ(LoadPolicy::DISALLOW, GetLoadPolicy("http://*.com/"));
  EXPECT_EQ(LoadPolicy::DISALLOW, GetLoadPolicy(kPercentEncodedHost));
}

// Verifies the behavior for rules having percent encoded domains.
TEST_F(SubresourceFilterIndexedRulesetTest, PercentEncodedDomain) {
  const char* kUrl = "http://example.com";
  std::string percent_encoded_host = "%2C.com";

  auto rule =
      MakeUrlRule(UrlPattern(kUrl, url_pattern_index::testing::kSubstring));
  url_pattern_index::testing::AddInitiatorDomains({percent_encoded_host},
                                                  &rule);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  // Note: This should actually fail. However url_pattern_index lower cases all
  // domains. Hence it doesn't correctly deal with domains having escape
  // characters which are percent-encoded in upper case by Chrome's url parser.
  EXPECT_EQ(LoadPolicy::ALLOW,
            GetLoadPolicy(kUrl, "http://" + percent_encoded_host));
  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy(kUrl, "http://,.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlocklistAndAllowlist) {
  ASSERT_TRUE(AddSimpleRule("?filter="));
  ASSERT_TRUE(AddSimpleAllowlistRule("allowlisted.com/?filter="));
  Finish();

  EXPECT_EQ(LoadPolicy::DISALLOW,
            GetLoadPolicy("http://blocklisted.com?filter=on"));
  EXPECT_EQ(LoadPolicy::EXPLICITLY_ALLOW,
            GetLoadPolicy("https://allowlisted.com/?filter=on"));
  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("https://notblocklisted.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       OneBlocklistAndOneDeactivationRule) {
  ASSERT_TRUE(AddSimpleRule("example.com"));
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com",
                                     url_pattern_index::testing::kDocument));
  Finish();

  EXPECT_TRUE(ShouldDeactivate("https://example.com", "",
                               url_pattern_index::testing::kDocument));
  EXPECT_FALSE(ShouldDeactivate("https://xample.com", "",
                                url_pattern_index::testing::kDocument));
  EXPECT_EQ(LoadPolicy::DISALLOW, GetLoadPolicy("https://example.com"));
  EXPECT_EQ(LoadPolicy::ALLOW, GetLoadPolicy("https://xample.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, MatchingEmptyRuleset) {
  Finish();
  EXPECT_FALSE(MatchingRule(""));
  EXPECT_FALSE(MatchingRule("http://example.com"));
  EXPECT_FALSE(MatchingRule("http://another.example.com?param=val"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, MatchingNoRuleApplies) {
  ASSERT_TRUE(AddSimpleRule("?filter_out="));
  ASSERT_TRUE(AddSimpleRule("&filter_out="));
  Finish();

  EXPECT_FALSE(MatchingRule("http://example.com"));
  EXPECT_FALSE(MatchingRule("http://example.com?filter_not"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, MatchingSimpleBlocklist) {
  ASSERT_TRUE(AddSimpleRule("?param="));
  Finish();

  EXPECT_FALSE(MatchingRule("https://example.com"));
  EXPECT_TRUE(MatchingRule("http://example.org?param=image1"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, MatchingSimpleAllowlist) {
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com/?filter_out="));
  Finish();

  EXPECT_FALSE(MatchingRule("https://example.com?filter_out=true"));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       MatchingSimpleBlocklistAndAllowlist) {
  ASSERT_TRUE(AddSimpleRule("?filter="));
  ASSERT_TRUE(AddSimpleAllowlistRule("allowlisted.com/?filter="));
  Finish();

  EXPECT_TRUE(MatchingRule("http://blocklisted.com?filter=on"));
  EXPECT_TRUE(MatchingRule("https://allowlisted.com?filter=on"));
  EXPECT_FALSE(MatchingRule("https://notblocklisted.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       MatchingOneBlocklistAndOneDeactivationRule) {
  ASSERT_TRUE(AddSimpleRule("example.com"));
  ASSERT_TRUE(AddSimpleAllowlistRule("example.com",
                                     url_pattern_index::testing::kDocument));
  Finish();
  EXPECT_TRUE(MatchingRule("https://example.com"));
  EXPECT_FALSE(MatchingRule("https://xample.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, RulesetId) {
  indexer_ = std::make_unique<RulesetIndexer>(0x12345678);
  ASSERT_TRUE(AddSimpleRule("example.com"));
  Finish();
  EXPECT_EQ(0x12345678u, matcher_->ruleset_id());
}

TEST_F(SubresourceFilterIndexedRulesetTest, StyleRulesSmokeTest) {
  AddStyleRule(StyleRuleParams().SetSelector("#ad-1").SetIds({"ad-1"}));
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-2")
                   .SetDomains({"example.com"})
                   .SetIds({"ad-2"}));
  AddStyleRule(StyleRuleParams().SetSelector(".class1").SetClasses({"class1"}));
  Finish();

  std::vector<std::string_view> rules;

  // Verify domain-specific selector.
  matcher_->GetDomainSelectors(url::Origin::Create(GURL("http://example.com")),
                               rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-2", rules[0]);

  // Verify lookup by ID.
  rules.clear();
  matcher_->GetSelectorsById(url::Origin(), "ad-1", GetStyleRuleHash("ad-1"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-1", rules[0]);

  // Verify lookup by class.
  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "class1",
                                GetStyleRuleHash("class1"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class1", rules[0]);

  // Verify MaybeHasStyleRule.
  EXPECT_TRUE(matcher_->MaybeHasStyleRule(GetStyleRuleHash("ad-1")));
  EXPECT_TRUE(matcher_->MaybeHasStyleRule(GetStyleRuleHash("class1")));
  EXPECT_FALSE(matcher_->MaybeHasStyleRule(GetStyleRuleHash("non-existent")));
}

}  // namespace subresource_filter
