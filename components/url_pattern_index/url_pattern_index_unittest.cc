// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/url_pattern_index.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "components/url_pattern_index/url_pattern.h"
#include "components/url_pattern_index/url_rule_test_support.h"
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

class UrlPatternIndexTest : public ::testing::Test {
 public:
  UrlPatternIndexTest() { Reset(); }

 protected:
  bool AddUrlRule(const proto::UrlRule& rule) {
    auto offset = SerializeUrlRule(rule, flat_builder_.get(), &domain_map_);
    if (offset.o)
      index_builder_->IndexUrlRule(offset);
    return !!offset.o;
  }

  void AddSimpleUrlRule(std::string pattern,
                        uint32_t id,
                        uint32_t priority,
                        uint8_t options) {
    auto pattern_offset = flat_builder_->CreateString(pattern);

    flat::UrlRuleBuilder rule_builder(*flat_builder_);
    rule_builder.add_options(options);
    rule_builder.add_url_pattern(pattern_offset);
    rule_builder.add_id(id);
    rule_builder.add_priority(priority);
    auto rule_offset = rule_builder.Finish();

    index_builder_->IndexUrlRule(rule_offset);
  }

  void Finish() {
    const auto index_offset = index_builder_->Finish();
    flat_builder_->Finish(index_offset);

    const flat::UrlPatternIndex* flat_index =
        flat::GetUrlPatternIndex(flat_builder_->GetBufferPointer());
    index_matcher_.reset(new UrlPatternIndexMatcher(flat_index));
  }

  const flat::UrlRule* FindMatch(
      base::StringPiece url_string,
      base::StringPiece document_origin_string = base::StringPiece(),
      proto::ElementType element_type = testing::kOther,
      proto::ActivationType activation_type = kNoActivation,
      bool disable_generic_rules = false) const {
    const GURL url(url_string);
    const url::Origin document_origin =
        testing::GetOrigin(document_origin_string);
    return index_matcher_->FindMatch(
        url, document_origin, element_type, activation_type,
        testing::IsThirdParty(url, document_origin), disable_generic_rules,
        UrlPatternIndexMatcher::FindRuleStrategy::kAny);
  }

  const flat::UrlRule* FindHighestPriorityMatch(
      base::StringPiece url_string) const {
    return index_matcher_->FindMatch(
        GURL(url_string), url::Origin(), testing::kOther /*element_type*/,
        kNoActivation /*activation_type*/, true /*is_third_party*/,
        false /*disable_generic_rules*/,
        UrlPatternIndexMatcher::FindRuleStrategy::
            kHighestPriority /*strategy*/);
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
    flat_builder_.reset(new flatbuffers::FlatBufferBuilder());
    index_builder_.reset(new UrlPatternIndexBuilder(flat_builder_.get()));
    domain_map_.clear();
  }

 private:
  std::unique_ptr<flatbuffers::FlatBufferBuilder> flat_builder_;
  std::unique_ptr<UrlPatternIndexBuilder> index_builder_;
  std::unique_ptr<UrlPatternIndexMatcher> index_matcher_;

  FlatDomainMap domain_map_;

  DISALLOW_COPY_AND_ASSIGN(UrlPatternIndexTest);
};

TEST_F(UrlPatternIndexTest, EmptyIndex) {
  Finish();
  EXPECT_FALSE(FindMatch(base::StringPiece() /* url */));
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
                   common_options | flat::OptionFlag_IS_CASE_INSENSITIVE);
  AddSimpleUrlRule("case-sensitive", 0 /* id */, 0 /* priority */,
                   common_options);
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
      {"ex.com", kThirdParty, "http://ex.com", nullptr, true},

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
  constexpr const char* kUrl = "http://example.com";

  const struct {
    std::vector<std::string> domains;
    const char* document_origin;
    bool expect_match;
  } kTestCases[] = {
      {std::vector<std::string>(), nullptr, true},
      {std::vector<std::string>(), "http://domain.com", true},

      {{"domain.com"}, nullptr, false},
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

      {{"~domain.com"}, nullptr, true},
      {{"~domain.com"}, "http://domain.com", false},
      {{"~ddomain.com"}, "http://domain.com", true},
      {{"~domain.com"}, "http://ddomain.com", true},
      {{"~domain.com"}, "http://sub.domain.com", false},
      {{"~sub.domain.com"}, "http://domain.com", true},
      {{"~sub.domain.com"}, "http://sub.domain.com", false},
      {{"~sub.domain.com"}, "http://a.b.c.sub.domain.com", false},
      {{"~sub.domain.com"}, "http://sub.domain.com.com", true},

      {{"domain1.com", "domain2.com"}, nullptr, false},
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

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Domains: " << ::testing::PrintToString(test_case.domains)
                 << "; DocumentOrigin: " << test_case.document_origin);

    auto rule = MakeUrlRule(UrlPattern(kUrl, kSubstring));
    testing::AddDomains(test_case.domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(kUrl, test_case.document_origin));
    Reset();
  }
}

TEST_F(UrlPatternIndexTest, OneRuleWithLongDomainList) {
  constexpr const char* kUrl = "http://example.com";
  constexpr size_t kDomains = 200;

  std::vector<std::string> domains;
  for (size_t i = 0; i < kDomains; ++i) {
    const std::string domain = "domain" + std::to_string(i) + ".com";
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
  testing::AddDomains(domains, &rule);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  for (size_t i = 0; i < kDomains; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration: " << i);
    const std::string domain = "domain" + std::to_string(i) + ".com";

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
              !!FindMatch(test_case.url, nullptr /* document_origin_string */,
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
    rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
    rule.clear_element_types();
    rule.set_activation_types(test_case.activation_types);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_match,
              !!FindMatch(test_case.document_url,
                          nullptr /* parent_document_origin */,
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
  rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
  rule.set_element_types(testing::kSubdocument);
  rule.set_activation_types(kDocument);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  EXPECT_FALSE(FindMatch("http://allow.ex.com"));
  EXPECT_TRUE(FindMatch("http://allow.ex.com",
                        nullptr /*document_origin_string */,
                        testing::kSubdocument));

  EXPECT_FALSE(FindMatch("http://allow.ex.com",
                         nullptr /* document_origin_string */,
                         testing::kNoElement, testing::kGenericBlock));
  EXPECT_TRUE(FindMatch("http://allow.ex.com",
                        nullptr /* document_origin_string */,
                        testing::kNoElement, kDocument));
}

TEST_F(UrlPatternIndexTest, MatchWithDisableGenericRules) {
  const struct {
    const char* url_pattern;
    std::vector<std::string> domains;
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
    testing::AddDomains(rule_data.domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule))
        << "UrlPattern: " << rule_data.url_pattern
        << "; Domains: " << ::testing::PrintToString(rule_data.domains);
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
    rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
    rule.set_element_types(rule_data.element_types);
    rule.set_activation_types(rule_data.activation_types);
    EXPECT_TRUE(AddUrlRule(rule))
        << "ElementTypes: " << static_cast<int>(rule_data.element_types)
        << "; ActivationTypes: "
        << static_cast<int>(rule_data.activation_types);
  }
  Finish();

  EXPECT_TRUE(FindMatch("http://example.com/", nullptr, kImage));
  EXPECT_TRUE(FindMatch("http://example.com/", nullptr, kScript));
  EXPECT_FALSE(FindMatch("http://example.com/", nullptr, testing::kPopup));
  EXPECT_FALSE(FindMatch("http://example.com/"));

  EXPECT_TRUE(
      FindMatch("http://example.com", nullptr, testing::kNoElement, kDocument));
  EXPECT_FALSE(FindMatch("http://example.com", nullptr, testing::kNoElement,
                         testing::kGenericBlock));
}

TEST_F(UrlPatternIndexTest, FindMatchReturnsCorrectRules) {
  constexpr size_t kNumOfPatterns = 1024;

  std::vector<std::string> url_patterns(kNumOfPatterns);
  for (size_t i = 0; i < kNumOfPatterns; ++i) {
    url_patterns[i] = "http://example." + std::to_string(i) + ".com";
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
              base::StringPiece(rule_pattern->data(), rule_pattern->size()));
  }

  EXPECT_FALSE(
      FindMatch("http://example." + std::to_string(kNumOfPatterns) + ".com"));
}

// Tests UrlPatternIndexMatcher::FindMatch works with the kHighestPriority match
// strategy.
TEST_F(UrlPatternIndexTest, FindMatchHighestPriority) {
  const size_t kNumPatternTypes = 15;

  int id = 1;
  auto pattern_for_number = [](size_t num) {
    return "http://" + std::to_string(num) + ".com";
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
                           flat::OptionFlag_APPLIES_TO_THIRD_PARTY);
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

}  // namespace url_pattern_index
