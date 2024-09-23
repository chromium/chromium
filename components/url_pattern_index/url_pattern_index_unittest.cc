// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/url_pattern_index/url_pattern_index.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "components/url_pattern_index/url_pattern.h"
#include "components/url_pattern_index/url_rule_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace url_pattern_index {

using testing::kAnchorNone;
using testing::kAnyParty;
using testing::kBoundary;
using testing::kDocument;
using testing::kFont;
using testing::kImage;
using testing::kNoActivation;
using testing::kScript;
using testing::kSubdomain;
using testing::kSubstring;
using testing::kThirdParty;
using testing::MakeUrlRule;
using EmbedderConditionsMatcher =
    UrlPatternIndexMatcher::EmbedderConditionsMatcher;

class UrlPatternIndexTest : public ::testing::Test {
 public:
  UrlPatternIndexTest() { Reset(); }

  UrlPatternIndexTest(const UrlPatternIndexTest&) = delete;
  UrlPatternIndexTest& operator=(const UrlPatternIndexTest&) = delete;

 protected:
  bool AddUrlRule(const proto::UrlRule& rule) {
    auto offset = SerializeUrlRule(rule, flat_builder_.get(), &domain_map_);
    if (offset.o) {
      indexed_rules_count_++;
      index_builder_->IndexUrlRule(offset);
    }
    return !!offset.o;
  }

  void AddSimpleUrlRule(const std::string& pattern,
                        uint32_t id,
                        uint32_t priority,
                        uint8_t options,
                        uint16_t element_types,
                        uint16_t request_methods_mask,
                        const std::vector<uint8_t>& embedder_conditions = {}) {
    auto pattern_offset = flat_builder_->CreateString(pattern);
    auto embedder_conditions_offset =
        flat_builder_->CreateVector(embedder_conditions);

    flat::UrlRuleBuilder rule_builder(*flat_builder_);
    rule_builder.add_options(options);
    rule_builder.add_url_pattern(pattern_offset);
    rule_builder.add_id(id);
    rule_builder.add_priority(priority);
    rule_builder.add_element_types(element_types);
    rule_builder.add_request_methods(request_methods_mask);
    rule_builder.add_embedder_conditions(embedder_conditions_offset);

    auto rule_offset = rule_builder.Finish();

    index_builder_->IndexUrlRule(rule_offset);

    indexed_rules_count_++;
  }

  void Finish() {
    const auto index_offset = index_builder_->Finish();
    flat_builder_->Finish(index_offset);

    const flat::UrlPatternIndex* flat_index =
        flat::GetUrlPatternIndex(flat_builder_->GetBufferPointer());
    index_matcher_ = std::make_unique<UrlPatternIndexMatcher>(flat_index);

    ASSERT_EQ(indexed_rules_count_, index_matcher_->GetRulesCount());
  }

  const flat::UrlRule* FindMatch(
      std::string_view url_string,
      std::string_view document_origin_string = std::string_view(),
      proto::ElementType element_type = testing::kOther,
      proto::ActivationType activation_type = kNoActivation,
      bool disable_generic_rules = false,
      const base::flat_set<int>& disabled_rule_ids = {}) const {
    const GURL url(url_string);
    const url::Origin document_origin =
        testing::GetOrigin(document_origin_string);
    return index_matcher_->FindMatch(
        url, document_origin, element_type, activation_type,
        testing::IsThirdParty(url, document_origin), disable_generic_rules,
        UrlPatternIndexMatcher::EmbedderConditionsMatcher(),
        UrlPatternIndexMatcher::FindRuleStrategy::kAny, disabled_rule_ids);
  }

  const flat::UrlRule* FindMatch(
      std::string_view url_string,
      std::string_view document_origin_string,
      flat::ElementType element_type,
      flat::ActivationType activation_type,
      flat::RequestMethod request_method,
      bool disable_generic_rules,
      const EmbedderConditionsMatcher& embedder_conditions_matcher =
          EmbedderConditionsMatcher()) const {
    const GURL url(url_string);
    const url::Origin document_origin =
        testing::GetOrigin(document_origin_string);
    return index_matcher_->FindMatch(
        url, document_origin, element_type, activation_type, request_method,
        testing::IsThirdParty(url, document_origin), disable_generic_rules,
        embedder_conditions_matcher,
        UrlPatternIndexMatcher::FindRuleStrategy::kAny,
        {} /* disabled_rule_ids */);
  }

  std::vector<const flat::UrlRule*> FindAllMatches(
      std::string_view url_string,
      std::string_view document_origin_string,
      proto::ElementType element_type,
      proto::ActivationType activation_type,
      bool disable_generic_rules,
      const base::flat_set<int>& disabled_rule_ids = {}) const {
    const GURL url(url_string);
    const url::Origin document_origin =
        testing::GetOrigin(document_origin_string);
    return index_matcher_->FindAllMatches(
        url, document_origin, element_type, activation_type,
        testing::IsThirdParty(url, document_origin), disable_generic_rules,
        UrlPatternIndexMatcher::EmbedderConditionsMatcher(), disabled_rule_ids);
  }

  const flat::UrlRule* FindHighestPriorityMatch(
      std::string_view url_string,
      const base::flat_set<int>& disabled_rule_ids = {}) const {
    return index_matcher_->FindMatch(
        GURL(url_string), url::Origin(), testing::kOther /*element_type*/,
        kNoActivation /*activation_type*/, true /*is_third_party*/,
        false /*disable_generic_rules*/,
        UrlPatternIndexMatcher::EmbedderConditionsMatcher(),
        UrlPatternIndexMatcher::FindRuleStrategy::kHighestPriority /*strategy*/,
        disabled_rule_ids);
  }

  bool IsOutOfRange(const flat::UrlRule* rule) const {
    if (!rule)
      return false;
    const auto* data = reinterpret_cast<const uint8_t*>(rule);
    return data < flat_builder_->GetBufferPointer() ||
           data >= flat_builder_->GetBufferPointer() + flat_builder_->GetSize();
  }

  void Reset() {
    index_matcher_.reset();
    index_builder_.reset();
    flat_builder_ = std::make_unique<flatbuffers::FlatBufferBuilder>();
    index_builder_ =
        std::make_unique<UrlPatternIndexBuilder>(flat_builder_.get());
    domain_map_.clear();
    indexed_rules_count_ = 0;
  }

 private:
  size_t indexed_rules_count_ = 0;
  std::unique_ptr<flatbuffers::FlatBufferBuilder> flat_builder_;
  std::unique_ptr<UrlPatternIndexBuilder> index_builder_;
  std::unique_ptr<UrlPatternIndexMatcher> index_matcher_;

  FlatDomainMap domain_map_;
};

TEST_F(UrlPatternIndexTest, EmptyIndex) {
  Finish();
  EXPECT_FALSE(FindMatch(std::string_view() /* url */));
  EXPECT_FALSE(FindMatch("http://example.com"));
  EXPECT_FALSE(FindMatch("http://another.example.com?param=val"));
}

TEST_F(UrlPatternIndexTest, OneSimpleRule) {
  ASSERT_TRUE(AddUrlRule(MakeUrlRule(UrlPattern("?param=", kSubstring))));
  Finish();

  EXPECT_FALSE(FindMatch("https://example.com"));
  EXPECT_TRUE(FindMatch("http://example.org?param=image1"));
}

TEST_F(UrlPatternIndexTest, NoRuleApplies) {
  ASSERT_TRUE(AddUrlRule(MakeUrlRule(UrlPattern("?filter_out=", kSubstring))));
  ASSERT_TRUE(AddUrlRule(MakeUrlRule(UrlPattern("&filter_out=", kSubstring))));
  Finish();

  EXPECT_FALSE(FindMatch("http://example.com"));
  EXPECT_FALSE(FindMatch("http://example.com?filter_not"));
  EXPECT_FALSE(FindMatch("http://example.com?k=v&filter_not"));
}

TEST_F(UrlPatternIndexTest, ProtoCaseSensitivity) {
  ASSERT_TRUE(
      AddUrlRule(MakeUrlRule(UrlPattern("case-sensitive", kSubstring))));
  proto::UrlRule rule = MakeUrlRule(UrlPattern("case-INSENsitive"));
  rule.set_match_case(false);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  // We don't currently read case sensitivity from proto rules.
  EXPECT_FALSE(FindMatch("http://abc.com/type=CASE-insEnsitIVe"));
  EXPECT_FALSE(FindMatch("http://abc.com/type=case-INSENSITIVE"));
  EXPECT_FALSE(FindMatch("http://abc.com?type=CASE-sensitive"));
  EXPECT_TRUE(FindMatch("http://abc.com?type=case-sensitive"));
}

TEST_F(UrlPatternIndexTest, CaseSensitivity) {
  uint8_t common_options = flat::OptionFlag_APPLIES_TO_FIRST_PARTY |
                           flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
  AddSimpleUrlRule("case-insensitive", 0 /* id */, 0 /* priority */,
                   common_options | flat::OptionFlag_IS_CASE_INSENSITIVE,
                   flat::ElementType_ANY, flat::RequestMethod_ANY);
  AddSimpleUrlRule("case-sensitive", 0 /* id */, 0 /* priority */,
                   common_options, flat::ElementType_ANY,
                   flat::RequestMethod_ANY);
  Finish();

  EXPECT_TRUE(FindMatch("http://abc.com/type=CASE-insEnsitIVe"));
  EXPECT_TRUE(FindMatch("http://abc.com/type=case-INSENSITIVE"));
  EXPECT_FALSE(FindMatch("http://abc.com?type=CASE-sensitive"));
  EXPECT_TRUE(FindMatch("http://abc.com?type=case-sensitive"));
}

TEST_F(UrlPatternIndexTest, OneRuleWithoutMetaInfo) {
  const struct {
    UrlPattern url_pattern;
    const char* url;
    bool expect_match;
  } kTestCases[] = {
      // SUBSTRING
      {{"abcd", kSubstring}, "http://ex.com/abcd", true},
      {{"abcd", kSubstring}, "http://ex.com/dcab", false},
      {{"42", kSubstring}, "http://ex.com/adcd/picture42.png", true},
      {{"&test", kSubstring},
       "http://ex.com/params?param1=false&test=true",
       true},
      {{"-test-42.", kSubstring}, "http://ex.com/unit-test-42.1", true},
      {{"/abcdtest160x600.", kSubstring},
       "http://ex.com/abcdtest160x600.png",
       true},

      // WILDCARDED
      {{"http://ex.com/abcd/picture*.png"},
       "http://ex.com/abcd/picture42.png",
       true},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://ex.com", true},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://test.ex.com", true},
      {{"ex.com", kSubdomain, kAnchorNone}, "https://test.ex.com.com", true},
      {{"ex.com", kSubdomain, kAnchorNone}, "https://test.rest.ex.com", true},
      {{"ex.com", kSubdomain, kAnchorNone}, "https://test_ex.com", false},
      {{"abcd.ex.com/", kSubdomain, kAnchorNone},
       "http://abcd.ex.com?xyz=1",
       true},
      {{"abcd.ex.com/", kSubdomain, kAnchorNone},
       "http://abcd.ex.com#xyz",
       true},
      {{"ex.co/", kSubdomain, kAnchorNone}, "https://test.ex.co", true},
      {{"abcd.ex.com/", kSubdomain, kAnchorNone},
       "https://abcd.ex.com.",
       false},

      {{"http://ex.com", kBoundary, kAnchorNone}, "http://ex.com/", true},
      {{"http://ex.com", kBoundary, kAnchorNone}, "http://ex.com/42", true},
      {{"http://ex.com", kBoundary, kAnchorNone},
       "http://ex.com/42/http://ex.com/",
       true},
      {{"http://ex.com", kBoundary, kAnchorNone},
       "http://ex.com/42/http://ex.info/",
       true},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com", true},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com/42", false},
      {{"http://ex.com/", kBoundary, kBoundary},
       "http://ex.info/42/http://ex.com/",
       false},
      {{"http://ex.com/", kBoundary, kBoundary},
       "http://ex.info/42/http://ex.com/",
       false},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com/", true},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com/42.swf", false},
      {{"http://ex.com/", kBoundary, kBoundary},
       "http://ex.info/redirect/http://ex.com/",
       false},
      {{"pdf", kAnchorNone, kBoundary}, "http://ex.com/abcd.pdf", true},
      {{"pdf", kAnchorNone, kBoundary}, "http://ex.com/pdfium", false},
      {{"http://ex.com^"}, "http://ex.com/", true},
      {{"http://ex.com^"}, "http://ex.com:8000/", true},
      {{"http://ex.com^"}, "http://ex.com.ru", false},
      {{"^ex.com^"},
       "http://ex.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       true},
      {{"^42.loss^"},
       "http://ex.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       true},

      // TODO(pkalinnikov): The '^' at the end should match end-of-string.
      //
      // {"^%D1%82%D0%B5%D1%81%D1%82^",
      //  "http://ex.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
      //  true},
      // {"/abcd/*/picture^", "http://ex.com/abcd/42/picture", true},

      {{"/abcd/*/picture^"}, "http://ex.com/abcd/42/loss/picture?param", true},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd//picture/42", true},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd/picture", false},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd/42/pictureraph", false},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd/42/picture.swf", false},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://test.ex.com/42.swf",
       true},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://server1.test.ex.com/42.swf",
       true},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "https://test.ex.com:8000/",
       true},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://test.ex.com.ua/42.swf",
       false},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://ex.com/redirect/http://test.ex.com/",
       false},

      {{"/abcd/*"}, "https://ex.com/abcd/", true},
      {{"/abcd/*"}, "http://ex.com/abcd/picture.jpeg", true},
      {{"/abcd/*"}, "https://ex.com/abcd", false},
      {{"/abcd/*"}, "http://abcd.ex.com", false},
      {{"*/abcd/"}, "https://ex.com/abcd/", true},
      {{"*/abcd/"}, "http://ex.com/abcd/picture.jpeg", true},
      {{"*/abcd/"}, "https://ex.com/test-abcd/", false},
      {{"*/abcd/"}, "http://abcd.ex.com", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << "UrlPattern: " << test_case.url_pattern
                                      << "; URL: " << test_case.url);

    ASSERT_TRUE(AddUrlRule(MakeUrlRule(test_case.url_pattern)));
    Finish();

    EXPECT_EQ(test_case.expect_match, !!FindMatch(test_case.url));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithThirdParty) {
  const struct {
    const char* url_pattern;
    proto::SourceType source_type;

    const char* url;
    const char* document_origin;
    bool expect_match;
  } kTestCases[] = {
      {"ex.com", kThirdParty, "http://ex.com", "http://exmpl.org", true},
      {"ex.com", kThirdParty, "http://ex.com", "http://ex.com", false},
      {"ex.com", kThirdParty, "http://ex.com/path?k=v", "http://exmpl.org",
       true},
      {"ex.com", kThirdParty, "http://ex.com/path?k=v", "http://ex.com", false},
      {"ex.com", testing::kFirstParty, "http://ex.com/path?k=v",
       "http://ex.com", true},
      {"ex.com", testing::kFirstParty, "http://ex.com/path?k=v",
       "http://exmpl.com", false},
      {"ex.com", kAnyParty, "http://ex.com/path?k=v", "http://ex.com", true},
      {"ex.com", kAnyParty, "http://ex.com/path?k=v", "http://exmpl.com", true},
      {"ex.com", kThirdParty, "http://subdomain.ex.com", "http://ex.com",
       false},
      {"ex.com", kThirdParty, "http://ex.com", "", true},

      // Public Suffix List tests.
      {"ex.com", kThirdParty, "http://two.ex.com", "http://one.ex.com", false},
      {"ex.com", kThirdParty, "http://ex.com", "http://one.ex.com", false},
      {"ex.com", kThirdParty, "http://two.ex.com", "http://ex.com", false},
      {"ex.com", kThirdParty, "http://ex.com", "http://example.org", true},
      {"appspot.com", kThirdParty, "http://two.appspot.org",
       "http://one.appspot.com", false},
  };

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "UrlPattern: " << test_case.url_pattern
                 << "; SourceType: " << static_cast<int>(test_case.source_type)
                 << "; URL: " << test_case.url
                 << "; DocumentOrigin: " << test_case.document_origin);

    auto rule = MakeUrlRule(UrlPattern(test_case.url_pattern, kSubstring));
    rule.set_source_type(test_case.source_type);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(test_case.url, test_case.document_origin));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithDomainList) {
  const struct {
    std::vector<std::string> domains;
    std::string_view url_or_origin;
    bool expect_match;
  } kTestCases[] = {
      {std::vector<std::string>(), "", true},
      {std::vector<std::string>(), "http://domain.com", true},

      {{"domain.com"}, "", false},
      {{"domain.com"}, "http://domain.com", true},
      {{"ddomain.com"}, "http://domain.com", false},
      {{"domain.com"}, "http://ddomain.com", false},
      {{"domain.com"}, "http://sub.domain.com", true},
      {{"sub.domain.com"}, "http://domain.com", false},
      {{"sub.domain.com"}, "http://sub.domain.com", true},
      {{"sub.domain.com"}, "http://a.b.c.sub.domain.com", true},
      {{"sub.domain.com"}, "http://sub.domain.com.com", false},

      // TODO(pkalinnikov): Probably need to canonicalize domain patterns to
      // avoid subtleties like below.
      {{"domain.com"}, "http://domain.com.", true},
      {{"domain.com"}, "http://.domain.com", true},
      {{"domain.com"}, "http://.domain.com.", true},
      {{".domain.com"}, "http://.domain.com", true},
      {{"domain.com."}, "http://domain.com", false},
      {{"domain.com."}, "http://domain.com.", true},

      {{"domain..com"}, "http://domain.com", false},
      {{"domain.com"}, "http://domain..com", false},
      {{"domain..com"}, "http://domain..com", true},

      {{"~domain.com"}, "", true},
      {{"~domain.com"}, "http://domain.com", false},
      {{"~ddomain.com"}, "http://domain.com", true},
      {{"~domain.com"}, "http://ddomain.com", true},
      {{"~domain.com"}, "http://sub.domain.com", false},
      {{"~sub.domain.com"}, "http://domain.com", true},
      {{"~sub.domain.com"}, "http://sub.domain.com", false},
      {{"~sub.domain.com"}, "http://a.b.c.sub.domain.com", false},
      {{"~sub.domain.com"}, "http://sub.domain.com.com", true},

      {{"domain1.com", "domain2.com"}, "", false},
      {{"domain1.com", "domain2.com"}, "http://domain1.com", true},
      {{"domain1.com", "domain2.com"}, "http://domain2.com", true},
      {{"domain1.com", "domain2.com"}, "http://domain3.com", false},
      {{"domain1.com", "domain2.com"}, "http://not_domain1.com", false},
      {{"domain1.com", "domain2.com"}, "http://sub.domain1.com", true},
      {{"domain1.com", "domain2.com"}, "http://a.b.c.sub.domain2.com", true},

      {{"~domain1.com", "~domain2.com"}, "http://domain1.com", false},
      {{"~domain1.com", "~domain2.com"}, "http://domain2.com", false},
      {{"~domain1.com", "~domain2.com"}, "http://domain3.com", true},

      {{"domain.com", "~sub.domain.com"}, "http://domain.com", true},
      {{"domain.com", "~sub.domain.com"}, "http://sub.domain.com", false},
      {{"domain.com", "~sub.domain.com"}, "http://a.b.sub.domain.com", false},
      {{"domain.com", "~sub.domain.com"}, "http://ssub.domain.com", true},

      {{"domain.com", "~a.domain.com", "~b.domain.com"},
       "http://domain.com",
       true},
      {{"domain.com", "~a.domain.com", "~b.domain.com"},
       "http://a.domain.com",
       false},
      {{"domain.com", "~a.domain.com", "~b.domain.com"},
       "http://b.domain.com",
       false},

      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://domain.com",
       true},
      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://a.domain.com",
       false},
      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://b.a.domain.com",
       true},
      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://c.b.a.domain.com",
       true},

      // The following test addresses a former bug in domain list matcher. When
      // "domain.com" was matched, the positive filters lookup stopped, and the
      // next domain was considered as a negative. The initial character was
      // skipped (supposing it's a '~') and the remainder was considered a
      // domain. So "ddomain.com" would be matched and thus the whole rule would
      // be classified as non-matching, which is not correct.
      {{"domain.com", "ddomain.com", "~sub.domain.com"},
       "http://domain.com",
       true},
  };

  // Test initiator domain conditions.
  constexpr const char* kUrl = "http://example.com";
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Initiator Domains: "
                 << ::testing::PrintToString(test_case.domains)
                 << "; DocumentOrigin: " << test_case.url_or_origin);

    auto rule = MakeUrlRule(UrlPattern(kUrl, kSubstring));
    testing::AddInitiatorDomains(test_case.domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(kUrl, test_case.url_or_origin));
    Reset();
  }

  // Test request domain conditions.
  for (const auto& test_case : kTestCases) {
    if (test_case.url_or_origin.empty())
      continue;

    SCOPED_TRACE(::testing::Message()
                 << "Request Domains: "
                 << ::testing::PrintToString(test_case.domains)
                 << "; Request URL: " << test_case.url_or_origin);

    auto rule = MakeUrlRule(UrlPattern(test_case.url_or_origin, kSubstring));
    testing::AddRequestDomains(test_case.domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match, !!FindMatch(test_case.url_or_origin));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithInitiatorAndRequestDomainLists) {
  const struct {
    std::vector<std::string> initiator_domains;
    std::vector<std::string> request_domains;
    const char* request_url;
    const char* document_origin;
    bool expect_match;
  } kTestCases[] = {
      {{"initiator.com"},
       {"request.com"},
       "http://request.com/path",
       "http://initiator.com",
       true},
      {{"initiator.com"},
       {"other-request.com"},
       "http://request.com/path",
       "http://initiator.com",
       false},
      {{"other-initiator.com"},
       {"request.com"},
       "http://request.com/path",
       "http://initiator.com",
       false},
      {{"~initiator.com"},
       {"request.com"},
       "http://request.com/path",
       "http://initiator.com",
       false},
      {{"initiator.com"},
       {"~request.com"},
       "http://request.com/path",
       "http://initiator.com",
       false},
      {{"~initiator.com"},
       {"~request.com"},
       "http://request.com/path",
       "http://initiator.com",
       false},
      {{"~other-initiator.com"},
       {"request.com"},
       "http://request.com/path",
       "http://initiator.com",
       true},
      {{"initiator.com"},
       {"~other-request.com"},
       "http://request.com/path",
       "http://initiator.com",
       true},
      {{"~other-initiator.com"},
       {"~other-request.com"},
       "http://request.com/path",
       "http://initiator.com",
       true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Initiator Domains: "
                 << ::testing::PrintToString(test_case.initiator_domains)
                 << "; Request Domains: "
                 << ::testing::PrintToString(test_case.request_domains)
                 << "; Request URL: " << test_case.request_url
                 << "; Request Origin: " << test_case.document_origin);

    auto rule = MakeUrlRule(UrlPattern(test_case.request_url, kSubstring));
    testing::AddInitiatorDomains(test_case.initiator_domains, &rule);
    testing::AddRequestDomains(test_case.request_domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(test_case.request_url, test_case.document_origin));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithLongDomainList) {
  constexpr const char* kUrl = "http://example.com";
  constexpr size_t kDomains = 200;

  std::vector<std::string> domains;
  for (size_t i = 0; i < kDomains; ++i) {
    const std::string domain = "domain" + base::NumberToString(i) + ".com";
    domains.push_back(domain);
    domains.push_back("~sub." + domain);
    domains.push_back("a.sub." + domain);
    domains.push_back("b.sub." + domain);
    domains.push_back("c.sub." + domain);
    domains.push_back("~aa.sub." + domain);
    domains.push_back("~ab.sub." + domain);
    domains.push_back("~ba.sub." + domain);
    domains.push_back("~bb.sub." + domain);
    domains.push_back("~sub.sub.c.sub." + domain);
  }

  auto rule = MakeUrlRule(UrlPattern(kUrl, kSubstring));
  testing::AddInitiatorDomains(domains, &rule);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  for (size_t i = 0; i < kDomains; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration: " << i);
    const std::string domain = "domain" + base::NumberToString(i) + ".com";

    EXPECT_TRUE(FindMatch(kUrl, "http://" + domain));
    EXPECT_FALSE(FindMatch(kUrl, "http://sub." + domain));
    EXPECT_TRUE(FindMatch(kUrl, "http://a.sub." + domain));
    EXPECT_TRUE(FindMatch(kUrl, "http://b.sub." + domain));
    EXPECT_TRUE(FindMatch(kUrl, "http://c.sub." + domain));
    EXPECT_FALSE(FindMatch(kUrl, "http://aa.sub." + domain));
    EXPECT_FALSE(FindMatch(kUrl, "http://ab.sub." + domain));
    EXPECT_FALSE(FindMatch(kUrl, "http://ba.sub." + domain));
    EXPECT_FALSE(FindMatch(kUrl, "http://bb.sub." + domain));
    EXPECT_TRUE(FindMatch(kUrl, "http://sub.c.sub." + domain));
    EXPECT_FALSE(FindMatch(kUrl, "http://sub.sub.c.sub." + domain));
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithElementTypes) {
  constexpr auto kAll = testing::kAllElementTypes;
  const struct {
    const char* url_pattern;
    int32_t element_types;

    const char* url;
    proto::ElementType element_type;
    bool expect_match;
  } kTestCases[] = {
      {"ex.com", kAll, "http://ex.com/img.jpg", kImage, true},
      {"ex.com", kAll & ~testing::kPopup, "http://ex.com/img", testing::kPopup,
       false},

      {"ex.com", kImage, "http://ex.com/img.jpg", kImage, true},
      {"ex.com", kAll & ~kImage, "http://ex.com/img.jpg", kImage, false},
      {"ex.com", kScript, "http://ex.com/img.jpg", kImage, false},
      {"ex.com", kAll & ~kScript, "http://ex.com/img.jpg", kImage, true},

      {"ex.com", kImage | kFont, "http://ex.com/font", kFont, true},
      {"ex.com", kImage | kFont, "http://ex.com/image", kImage, true},
      {"ex.com", kImage | kFont, "http://ex.com/video",
       proto::ELEMENT_TYPE_MEDIA, false},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/font", kFont, false},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/scr", kScript, false},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/img", kImage, true},

      {"ex.com", kAll, "http://ex.com", proto::ELEMENT_TYPE_OTHER, true},
      {"ex.com", kAll, "http://ex.com", proto::ELEMENT_TYPE_UNSPECIFIED, false},
      {"ex.com", testing::kWebSocket, "ws://ex.com",
       proto::ELEMENT_TYPE_WEBSOCKET, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        ::testing::Message()
        << "UrlPattern: " << test_case.url_pattern
        << "; ElementTypes: " << static_cast<int>(test_case.element_types)
        << "; URL: " << test_case.url
        << "; ElementType: " << static_cast<int>(test_case.element_type));

    auto rule = MakeUrlRule(UrlPattern(test_case.url_pattern, kSubstring));
    rule.set_element_types(test_case.element_types);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(test_case.url, "" /* document_origin_string */,
                          test_case.element_type));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithActivationTypes) {
  const struct {
    const char* url_pattern;
    int32_t activation_types;

    const char* document_url;
    proto::ActivationType activation_type;
    bool expect_match;
  } kTestCases[] = {
      {"example.com", kDocument, "http://example.com", kDocument, true},
      {"xample.com", kDocument, "http://example.com", kDocument, true},
      {"exampl.com", kDocument, "http://example.com", kDocument, false},

      {"example.com", testing::kGenericBlock, "http://example.com", kDocument,
       false},
      {"example.com", kDocument, "http://example.com", kNoActivation, false},
      {"example.com", testing::kGenericBlock, "http://example.com",
       kNoActivation, false},

      // Invalid GURL.
      {"example.com", kDocument, "http;//example.com", kDocument, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        ::testing::Message()
        << "UrlPattern: " << test_case.url_pattern
        << "; ActivationTypes: " << static_cast<int>(test_case.activation_types)
        << "; DocumentURL: " << test_case.document_url
        << "; ActivationType: " << static_cast<int>(test_case.activation_type));

    auto rule = MakeUrlRule(UrlPattern(test_case.url_pattern, kSubstring));
    rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
    rule.clear_element_types();
    rule.set_activation_types(test_case.activation_types);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(
        test_case.expect_match,
        !!FindMatch(test_case.document_url, "" /* parent_document_origin */,
                    testing::kNoElement, test_case.activation_type));
    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(test_case.document_url, "http://example.com/",
                          testing::kNoElement, test_case.activation_type));
    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(test_case.document_url, "http://xmpl.com/",
                          testing::kNoElement, test_case.activation_type));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithElementAndActivationTypes) {
  auto rule = MakeUrlRule(UrlPattern("allow.ex.com", kSubstring));
  rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
  rule.set_element_types(testing::kSubdocument);
  rule.set_activation_types(kDocument);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  EXPECT_FALSE(FindMatch("http://allow.ex.com"));
  EXPECT_TRUE(FindMatch("http://allow.ex.com", "" /*document_origin_string */,
                        testing::kSubdocument));

  EXPECT_FALSE(FindMatch("http://allow.ex.com", "" /* document_origin_string */,
                         testing::kNoElement, testing::kGenericBlock));
  EXPECT_TRUE(FindMatch("http://allow.ex.com", "" /* document_origin_string */,
                        testing::kNoElement, kDocument));
}

// Test that FindAllMatches will return the correct number of UrlRule matches
// for incoming requests.
TEST_F(UrlPatternIndexTest, MultipleRuleMatches) {
  const struct {
    uint32_t id;
    const char* url_pattern;
    uint16_t element_types;
  } kRules[] = {{0, "ex1", flat::ElementType_ANY},
                {1, "ex1", flat::ElementType_IMAGE},
                {2, "ex1", flat::ElementType_IMAGE | flat::ElementType_FONT},
                {3, "ex12", flat::ElementType_ANY},
                {4, "google", flat::ElementType_ANY},
                {5, "google", flat::ElementType_IMAGE}};

  for (const auto& rule_data : kRules) {
    AddSimpleUrlRule(rule_data.url_pattern, rule_data.id, 0 /* priority */,
                     flat::OptionFlag_APPLIES_TO_FIRST_PARTY |
                         flat::OptionFlag_APPLIES_TO_THIRD_PARTY,
                     rule_data.element_types, flat::RequestMethod_ANY);
  }
  Finish();

  const struct {
    const char* url;
    proto::ElementType element_type;
    std::vector<uint32_t> expected_matched_ids;
  } kTestCases[] = {{"http://ex1.com", proto::ELEMENT_TYPE_OTHER, {0}},
                    {"http://ex1.com/font", kFont, {0, 2}},
                    {"http://ex1.com/img", kImage, {0, 1, 2}},
                    {"http://ex12.com", proto::ELEMENT_TYPE_OTHER, {0, 3}},
                    {"http://ex12.com/img", kImage, {0, 1, 2, 3}},
                    {"http://google.com", proto::ELEMENT_TYPE_OTHER, {4}},
                    {"http://google.com/img", kImage, {4, 5}},
                    {"http://ex12google.com/img", kImage, {0, 1, 2, 3, 4, 5}},
                    {"http://nomatch.com/img", kImage, {}}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "UrlPattern: " << test_case.url << "; ElementTypes: "
                 << static_cast<int>(test_case.element_type));

    std::vector<uint32_t> actual_matched_ids;
    std::vector<const flat::UrlRule*> matched_rules = FindAllMatches(
        test_case.url, "" /* document_origin_string */, test_case.element_type,
        kNoActivation, false /* disable_generic_rules */);

    for (const auto* rule : matched_rules)
      actual_matched_ids.push_back(rule->id());

    EXPECT_THAT(actual_matched_ids, ::testing::UnorderedElementsAreArray(
                                        test_case.expected_matched_ids));
  }
}

TEST_F(UrlPatternIndexTest, MatchWithDisableGenericRules) {
  const struct {
    const char* url_pattern;
    std::vector<std::string> initiator_domains;
  } kRules[] = {
      // Generic rules.
      {"some_text", std::vector<std::string>()},
      {"another_text", {"~example.com"}},
      {"final_text", {"~example1.com", "~example2.com"}},
      // Domain specific rules.
      {"some_text", {"example1.com"}},
      {"more_text", {"example.com", "~exclude.example.com"}},
      {"last_text", {"example1.com", "sub.example2.com"}},
  };

  for (const auto& rule_data : kRules) {
    auto rule = MakeUrlRule(UrlPattern(rule_data.url_pattern, kSubstring));
    testing::AddInitiatorDomains(rule_data.initiator_domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule))
        << "UrlPattern: " << rule_data.url_pattern << "; Initiator Domains: "
        << ::testing::PrintToString(rule_data.initiator_domains);
  }

  // Note: Some of the rules have common domains (e.g., example1.com), which are
  // ultimately shared by FlatBuffers' CreateSharedString. The test also makes
  // sure that the data structure works properly with such optimization.
  Finish();

  const struct {
    const char* url;
    const char* document_origin;
    bool expect_match_with_enable_all_rules;
    bool expect_match_with_disable_generic_rules;
  } kTestCases[] = {
      {"http://ex.com/some_text", "http://example.com", true, false},
      {"http://ex.com/some_text", "http://example1.com", true, true},

      {"http://ex.com/another_text", "http://example.com", false, false},
      {"http://ex.com/another_text", "http://example1.com", true, false},

      {"http://ex.com/final_text", "http://example.com", true, false},
      {"http://ex.com/final_text", "http://example1.com", false, false},
      {"http://ex.com/final_text", "http://example2.com", false, false},

      {"http://ex.com/more_text", "http://example.com", true, true},
      {"http://ex.com/more_text", "http://exclude.example.com", false, false},
      {"http://ex.com/more_text", "http://example1.com", false, false},

      {"http://ex.com/last_text", "http://example.com", false, false},
      {"http://ex.com/last_text", "http://example1.com", true, true},
      {"http://ex.com/last_text", "http://example2.com", false, false},
      {"http://ex.com/last_text", "http://sub.example2.com", true, true},
  };

  constexpr bool kDisableGenericRules = true;
  constexpr bool kEnableAllRules = false;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "UrlPattern: " << test_case.url
                 << "; DocumentOrigin: " << test_case.document_origin);

    EXPECT_EQ(
        test_case.expect_match_with_disable_generic_rules,
        !!FindMatch(test_case.url, test_case.document_origin, testing::kOther,
                    kNoActivation, kDisableGenericRules));
    EXPECT_EQ(test_case.expect_match_with_enable_all_rules,
              !!FindMatch(test_case.url, test_case.document_origin,
                          testing::kOther, kNoActivation, kEnableAllRules));
  }
}

TEST_F(UrlPatternIndexTest, RulesWithUnsupportedTypes) {
  const struct {
    int element_types;
    int activation_types;
  } kRules[] = {
      {proto::ELEMENT_TYPE_MAX << 1, 0},
      {0, proto::ACTIVATION_TYPE_MAX << 1},
      {proto::ELEMENT_TYPE_MAX << 1, proto::ACTIVATION_TYPE_MAX << 1},

      {testing::kPopup, 0},
      {0, proto::ACTIVATION_TYPE_ELEMHIDE},
      {0, proto::ACTIVATION_TYPE_GENERICHIDE},
      {0, proto::ACTIVATION_TYPE_ELEMHIDE | proto::ACTIVATION_TYPE_GENERICHIDE},
      {proto::ELEMENT_TYPE_POPUP, proto::ACTIVATION_TYPE_ELEMHIDE},
  };

  for (const auto& rule_data : kRules) {
    auto rule = MakeUrlRule(UrlPattern("example.com", kSubstring));
    rule.set_element_types(rule_data.element_types);
    rule.set_activation_types(rule_data.activation_types);
    EXPECT_FALSE(AddUrlRule(rule))
        << "ElementTypes: " << static_cast<int>(rule_data.element_types)
        << "; ActivationTypes: "
        << static_cast<int>(rule_data.activation_types);
  }
  ASSERT_TRUE(AddUrlRule(MakeUrlRule(UrlPattern("exmpl.com", kSubstring))));
  Finish();

  EXPECT_FALSE(FindMatch("http://example.com/"));
  EXPECT_TRUE(FindMatch("https://exmpl.com/"));
}

TEST_F(UrlPatternIndexTest, RulesWithSupportedAndUnsupportedTypes) {
  const struct {
    int element_types;
    int activation_types;
  } kRules[] = {
      {kImage | (proto::ELEMENT_TYPE_MAX << 1), 0},
      {kScript | testing::kPopup, 0},
      {0, kDocument | (proto::ACTIVATION_TYPE_MAX << 1)},
  };

  for (const auto& rule_data : kRules) {
    auto rule = MakeUrlRule(UrlPattern("example.com", kSubstring));
    rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
    rule.set_element_types(rule_data.element_types);
    rule.set_activation_types(rule_data.activation_types);
    EXPECT_TRUE(AddUrlRule(rule))
        << "ElementTypes: " << static_cast<int>(rule_data.element_types)
        << "; ActivationTypes: "
        << static_cast<int>(rule_data.activation_types);
  }
  Finish();

  EXPECT_TRUE(FindMatch("http://example.com/", "", kImage));
  EXPECT_TRUE(FindMatch("http://example.com/", "", kScript));
  EXPECT_FALSE(FindMatch("http://example.com/", "", testing::kPopup));
  EXPECT_FALSE(FindMatch("http://example.com/"));

  EXPECT_TRUE(
      FindMatch("http://example.com", "", testing::kNoElement, kDocument));
  EXPECT_FALSE(FindMatch("http://example.com", "", testing::kNoElement,
                         testing::kGenericBlock));
}

TEST_F(UrlPatternIndexTest, FindMatchReturnsCorrectRules) {
  constexpr size_t kNumOfPatterns = 1024;

  std::vector<std::string> url_patterns(kNumOfPatterns);
  for (size_t i = 0; i < kNumOfPatterns; ++i) {
    url_patterns[i] = "http://example." + base::NumberToString(i) + ".com";
    ASSERT_TRUE(
        AddUrlRule(MakeUrlRule(UrlPattern(url_patterns[i], kSubstring))))
        << "Rule #" << i;
  }
  Finish();

  std::reverse(url_patterns.begin() + kNumOfPatterns / 2, url_patterns.end());
  for (const std::string& url_pattern : url_patterns) {
    SCOPED_TRACE(::testing::Message() << "UrlPattern: " << url_pattern);

    const flat::UrlRule* rule = FindMatch(url_pattern);
    ASSERT_TRUE(rule);
    ASSERT_FALSE(IsOutOfRange(rule));

    const flatbuffers::String* rule_pattern = rule->url_pattern();
    ASSERT_TRUE(rule_pattern);
    EXPECT_EQ(url_pattern,
              std::string_view(rule_pattern->data(), rule_pattern->size()));
  }

  EXPECT_FALSE(FindMatch("http://example." +
                         base::NumberToString(kNumOfPatterns) + ".com"));
}

// Tests UrlPatternIndexMatcher::FindMatch works with the kHighestPriority match
// strategy.
TEST_F(UrlPatternIndexTest, FindMatchHighestPriority) {
  const size_t kNumPatternTypes = 15;

  int id = 1;
  auto pattern_for_number = [](size_t num) {
    return "http://" + base::NumberToString(num) + ".com";
  };

  for (size_t i = 1; i <= kNumPatternTypes; i++) {
    // For pattern type |i|, add |i| rules with priority from 1 to |i|.
    std::string pattern = pattern_for_number(i);

    // Create a shuffled vector of priorities from 1 to |i|.
    std::vector<uint32_t> priorities(i);
    std::iota(priorities.begin(), priorities.end(), 1);
    base::RandomShuffle(priorities.begin(), priorities.end());

    for (size_t j = 0; j < i; j++) {
      AddSimpleUrlRule(pattern, id, priorities[j],
                       flat::OptionFlag_APPLIES_TO_FIRST_PARTY |
                           flat::OptionFlag_APPLIES_TO_THIRD_PARTY,
                       flat::ElementType_ANY, flat::RequestMethod_ANY);
      id++;
    }
  }
  Finish();

  for (size_t i = 1; i <= kNumPatternTypes; i++) {
    std::string pattern = pattern_for_number(i);
    SCOPED_TRACE(::testing::Message() << "UrlPattern: " << pattern);

    const flat::UrlRule* rule = FindHighestPriorityMatch(pattern);
    ASSERT_TRUE(rule);

    EXPECT_EQ(i, rule->priority());
  }

  EXPECT_FALSE(FindHighestPriorityMatch(pattern_for_number(0)));
  EXPECT_FALSE(
      FindHighestPriorityMatch(pattern_for_number(kNumPatternTypes + 1)));
}

TEST_F(UrlPatternIndexTest, LongUrl_NoMatch) {
  std::string pattern = "http://example.com";
  ASSERT_TRUE(AddUrlRule(MakeUrlRule(UrlPattern(pattern, kSubstring))));
  Finish();

  std::string url = "http://example.com/";
  url.append(url::kMaxURLChars - url.size(), 'x');
  EXPECT_EQ(url::kMaxURLChars, url.size());
  EXPECT_TRUE(FindMatch(url));

  // Add a single extra character, which should push this over the max URL
  // limit. At this point, URL pattern matching should just give up since the
  // URL load will be disallowed elsewhere in the stack.
  url += "x";
  EXPECT_GT(url.size(), url::kMaxURLChars);
  EXPECT_FALSE(FindMatch(url));
}

TEST_F(UrlPatternIndexTest, RequestMethod) {
  const flat::ElementType other_element = flat::ElementType_OTHER;
  const flat::ActivationType no_activation = flat::ActivationType_NONE;
  const std::string origin = "http://foo.com";

  const struct {
    std::string name;
    flat::RequestMethod request_method;
  } request_methods[] = {{"delete", flat::RequestMethod_DELETE},
                         {"get", flat::RequestMethod_GET},
                         {"head", flat::RequestMethod_HEAD},
                         {"options", flat::RequestMethod_OPTIONS},
                         {"patch", flat::RequestMethod_PATCH},
                         {"post", flat::RequestMethod_POST},
                         {"put", flat::RequestMethod_PUT}};

  int next_rule_id = 0;
  for (auto request_method : request_methods) {
    AddSimpleUrlRule(request_method.name /* pattern */, next_rule_id++,
                     0 /* priority */,
                     flat::OptionFlag_APPLIES_TO_FIRST_PARTY |
                         flat::OptionFlag_APPLIES_TO_THIRD_PARTY,
                     flat::ElementType_ANY, request_method.request_method);
  }

  Finish();

  for (size_t i = 0; i < std::size(request_methods); i++) {
    SCOPED_TRACE(::testing::Message()
                 << "RequestMethod: " << request_methods[i].name);
    std::string url = origin + "/" + request_methods[i].name;
    EXPECT_TRUE(FindMatch(url, origin, other_element, no_activation,
                          flat::RequestMethod_ANY, false));
    EXPECT_TRUE(FindMatch(url, origin, other_element, no_activation,
                          flat::RequestMethod_NONE, false));
    for (size_t j = 0; j < std::size(request_methods); j++) {
      EXPECT_EQ(i == j, !!FindMatch(url, origin, other_element, no_activation,
                                    request_methods[j].request_method, false));
    }
  }
}

TEST_F(UrlPatternIndexTest, EmbedderConditions) {
  const std::vector<uint8_t> embedder_data_1 = {1, 2, 3};
  const std::string url_1 = "http://foo.com";
  AddSimpleUrlRule("foo", 1 /* id */, 0 /* priority */, flat::OptionFlag_ANY,
                   flat::ElementType_ANY, flat::RequestMethod_ANY,
                   embedder_data_1);
  const std::vector<uint8_t> embedder_data_2 = {4, 5};
  const std::string url_2 = "http://bar.com";
  AddSimpleUrlRule("bar", 2 /* id */, 0 /* priority */, flat::OptionFlag_ANY,
                   flat::ElementType_ANY, flat::RequestMethod_ANY,
                   embedder_data_2);
  Finish();

  EmbedderConditionsMatcher match_first_element_one =
      base::BindRepeating([](const flatbuffers::Vector<uint8_t>& conditions) {
        return conditions.size() >= 1 && conditions[0] == 1;
      });
  EmbedderConditionsMatcher match_first_element_three =
      base::BindRepeating([](const flatbuffers::Vector<uint8_t>& conditions) {
        return conditions.size() >= 1 && conditions[0] == 3;
      });
  EmbedderConditionsMatcher match_has_evens =
      base::BindRepeating([](const flatbuffers::Vector<uint8_t>& conditions) {
        return base::ranges::any_of(conditions,
                                    [](int i) { return i % 2 == 0; });
      });

  struct {
    const std::string url;
    const EmbedderConditionsMatcher matcher;
    const bool expect_match;
    // Fields below are valid iff `expect_match` is true.
    const uint32_t expected_id = 0;
    const std::optional<std::vector<uint8_t>> expected_embedder_data;
  } cases[] = {{url_1, match_first_element_one, true, 1, embedder_data_1},
               {url_1, match_has_evens, true, 1, embedder_data_1},
               {url_1, match_first_element_three, false},
               {url_2, match_first_element_one, false},
               {url_2, match_has_evens, true, 2, embedder_data_2},
               {url_2, match_first_element_three, false},
               {"http://abc.com", match_first_element_one, false}};

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(::testing::Message() << "Testing case " << i);
    bool disable_generic_rules = false;
    const flat::UrlRule* rule = FindMatch(
        cases[i].url, "", flat::ElementType_OTHER, flat::ActivationType_NONE,
        flat::RequestMethod_GET, disable_generic_rules, cases[i].matcher);
    EXPECT_EQ(cases[i].expect_match, !!rule);
    if (cases[i].expect_match) {
      EXPECT_EQ(cases[i].expected_id, rule->id());
      EXPECT_EQ(cases[i].expected_embedder_data,
                std::vector<uint8_t>(rule->embedder_conditions()->begin(),
                                     rule->embedder_conditions()->end()));
    }
  }
}

TEST_F(UrlPatternIndexTest, FindMatchWithDisabledRuleIds) {
  const struct {
    uint32_t id;
    const char* url_pattern;
    uint32_t priority;
  } kRules[] = {{0, "ex1", 0}, {1, "ex11", 1}, {2, "ex111", 2}};

  for (const auto& rule_data : kRules) {
    AddSimpleUrlRule(rule_data.url_pattern, rule_data.id, rule_data.priority,
                     flat::OptionFlag_ANY, flat::ElementType_ANY,
                     flat::RequestMethod_ANY);
  }
  Finish();

  const struct {
    const char* url;
    base::flat_set<int> disabled_rule_ids;
    const bool expected_match;
    // Fields below are valid only if `expected_match` is true.
    const uint32_t expected_highest_priority_id = UINT32_MAX;
  } kTestCases[] = {{"http://ex1.com", {}, true, 0},
                    {"http://ex1.com", {}, true, 0},
                    {"http://ex1.com", {0}, false},
                    {"http://ex1.com", {1}, true, 0},
                    {"http://ex1.com", {0, 1}, false},
                    {"http://ex11.com", {}, true, 1},
                    {"http://ex11.com", {}, true, 1},
                    {"http://ex11.com", {0}, true, 1},
                    {"http://ex11.com", {1}, true, 0},
                    {"http://ex11.com", {0, 1}, false},
                    {"http://ex11.com", {2}, true, 1},
                    {"http://ex11.com", {0, 2}, true, 1},
                    {"http://ex11.com", {1, 2}, true, 0},
                    {"http://ex111.com", {}, true, 2},
                    {"http://ex111.com", {}, true, 2},
                    {"http://ex111.com", {0}, true, 2},
                    {"http://ex111.com", {0, 1}, true, 2},
                    {"http://ex111.com", {2}, true, 1},
                    {"http://ex111.com", {0, 2}, true, 1},
                    {"http://ex111.com", {1, 2}, true, 0},
                    {"http://ex111.com", {0, 1, 2}, false},
                    {"http://ex111.com", {3}, true, 2},
                    {"http://ex111.com", {2, 3}, true, 1}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "UrlPattern: " << test_case.url << "; DisabledRuleIds: "
                 << ::testing::PrintToString(test_case.disabled_rule_ids));

    const flat::UrlRule* rule = FindMatch(
        test_case.url, std::string_view(), testing::kOther, kNoActivation,
        false /* disable_generic_rules */, test_case.disabled_rule_ids);

    EXPECT_EQ(test_case.expected_match, !!rule);

    rule = FindHighestPriorityMatch(test_case.url, test_case.disabled_rule_ids);
    EXPECT_EQ(test_case.expected_match, !!rule);
    if (test_case.expected_match && rule)
      EXPECT_EQ(test_case.expected_highest_priority_id, rule->id());
  }
}

TEST_F(UrlPatternIndexTest, FindAllMatchesWithDisabledRuleIds) {
  const struct {
    uint32_t id;
    const char* url_pattern;
  } kRules[] = {{0, "ex1"}, {1, "ex11"}, {2, "ex111"}};

  for (const auto& rule_data : kRules) {
    AddSimpleUrlRule(rule_data.url_pattern, rule_data.id, 0 /* priority */,
                     flat::OptionFlag_ANY, flat::ElementType_ANY,
                     flat::RequestMethod_ANY);
  }
  Finish();

  const struct {
    const char* url;
    base::flat_set<int> disabled_rule_ids;
    std::vector<uint32_t> expected_matched_ids;
  } kTestCases[] = {{"http://ex1.com", {}, {0}},
                    {"http://ex1.com", {}, {0}},
                    {"http://ex1.com", {0}, {}},
                    {"http://ex1.com", {1}, {0}},
                    {"http://ex1.com", {0, 1}, {}},
                    {"http://ex11.com", {}, {0, 1}},
                    {"http://ex11.com", {0}, {1}},
                    {"http://ex11.com", {1}, {0}},
                    {"http://ex11.com", {0, 1}, {}},
                    {"http://ex11.com", {2}, {0, 1}},
                    {"http://ex11.com", {0, 2}, {1}},
                    {"http://ex111.com", {}, {0, 1, 2}},
                    {"http://ex111.com", {}, {0, 1, 2}},
                    {"http://ex111.com", {0}, {1, 2}},
                    {"http://ex111.com", {1}, {0, 2}},
                    {"http://ex111.com", {2}, {0, 1}},
                    {"http://ex111.com", {0, 2}, {1}},
                    {"http://ex111.com", {0, 1, 2}, {}},
                    {"http://ex111.com", {3}, {0, 1, 2}},
                    {"http://ex111.com", {1, 3}, {0, 2}}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "UrlPattern: " << test_case.url << "; DisabledRuleIds: "
                 << ::testing::PrintToString(test_case.disabled_rule_ids));

    std::vector<const flat::UrlRule*> matched_rules = FindAllMatches(
        test_case.url, "" /* document_origin_string */,
        proto::ELEMENT_TYPE_OTHER, kNoActivation,
        false /* disable_generic_rules */, test_case.disabled_rule_ids);

    std::vector<uint32_t> actual_matched_ids;
    for (const auto* rule : matched_rules)
      actual_matched_ids.push_back(rule->id());

    EXPECT_THAT(actual_matched_ids, ::testing::UnorderedElementsAreArray(
                                        test_case.expected_matched_ids));
  }
}

}  // namespace url_pattern_index
