// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/keyword_provider.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using base::ASCIIToUTF16;

namespace {

class TestingSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override {
    DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
    if (scheme == url::kHttpScheme || scheme == url::kHttpsScheme)
      return metrics::OmniboxInputType::URL;
    return metrics::OmniboxInputType::EMPTY;
  }
};

}  // namespace

class KeywordProviderTest : public testing::Test {
 protected:
  template<class ResultType>
  struct MatchType {
    const ResultType member;
    bool allowed_to_be_default_match;
  };

  template<class ResultType>
  struct TestData {
    const base::string16 input;
    const size_t num_results;
    const MatchType<ResultType> output[3];
  };

  KeywordProviderTest() : kw_provider_(nullptr) {
    variations::testing::ClearAllVariationParams();
  }
  ~KeywordProviderTest() override {}

  // Should be called at least once during a test case.  This is a separate
  // function from SetUp() because the client may want to set parameters
  // (e.g., field trials) before initializing TemplateURLService and the
  // related internal variables here.
  void SetUpClientAndKeywordProvider();

  void TearDown() override;

  template<class ResultType>
  void RunTest(TestData<ResultType>* keyword_cases,
               int num_cases,
               ResultType AutocompleteMatch::* member);

 protected:
  static const TemplateURLService::Initializer kTestData[];

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<KeywordProvider> kw_provider_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
};

// static
const TemplateURLService::Initializer KeywordProviderTest::kTestData[] = {
    {"aa", "aa.com?foo={searchTerms}", "aa"},
    {"aaaa", "http://aaaa/?aaaa=1&b={searchTerms}&c", "aaaa"},
    {"aaaaa", "{searchTerms}", "aaaaa"},
    {"ab", "bogus URL {searchTerms}", "ab"},
    {"weasel", "weasel{searchTerms}weasel", "weasel"},
    {"www", " +%2B?={searchTerms}foo ", "www"},
    {"nonsub", "http://nonsubstituting-keyword.com/", "nonsub"},
    {"z", "{searchTerms}=z", "z"},
    {"host.site.com", "http://host.site.com/?q={searchTerms}", "host.site.com"},
    {"ignoremelong.domain.com",
     "http://ignoremelong.domain.com/?q={searchTerms}",
     "ignoremelong.domain.com"},
    {"ignoreme.domain2.com", "http://ignoreme.domain2.com/?q={searchTerms}",
     "ignoreme.domain2.com"},
    {"fooshort.com", "http://fooshort.com/?q={searchTerms}", "fooshort.com"},
    {"foolong.co.uk", "http://foolong.co.uk/?q={searchTerms}", "foolong.co.uk"},
    {"cleantestv1.com", "http://cleantestv1.com?q={searchTerms}", "clean v1"},
    {"cleantestv2.com", "http://cleantestv2.com?q={searchTerms}", "clean v2"},
    {"www.cleantestv2.com", "http://www.cleantestv2.com?q={searchTerms}",
     "www clean v2"},
    {"www.cleantestv3.com", "http://www.cleantestv3.com?q={searchTerms}",
     "www clean v3"},
    {"http://cleantestv4.com", "http://cleantestv4.com?q={searchTerms}",
     "http clean v4"},
    {"cleantestv5.com", "http://cleantestv5.com?q={searchTerms}", "clean v5"},
    {"http://cleantestv5.com", "http://cleantestv5.com?q={searchTerms}",
     "http clean v5"},
    {"cleantestv6:", "http://cleantestv6.com?q={searchTerms}", "clean v6"},
    {"cleantestv7/", "http://cleantestv7slash.com?q={searchTerms}",
     "clean v7 slash"},
    {"cleantestv8/", "http://cleantestv8.com?q={searchTerms}", "clean v8"},
    {"cleantestv8", "http://cleantestv8slash.com?q={searchTerms}",
     "clean v8 slash"},
};

void KeywordProviderTest::SetUpClientAndKeywordProvider() {
  client_.reset(new MockAutocompleteProviderClient());
  client_->set_template_url_service(
      std::make_unique<TemplateURLService>(kTestData, base::size(kTestData)));
  kw_provider_ = new KeywordProvider(client_.get(), nullptr);
}

void KeywordProviderTest::TearDown() {
  client_.reset();
  kw_provider_ = nullptr;
}

template<class ResultType>
void KeywordProviderTest::RunTest(TestData<ResultType>* keyword_cases,
                                  int num_cases,
                                  ResultType AutocompleteMatch::* member) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    SCOPED_TRACE(keyword_cases[i].input);
    AutocompleteInput input(keyword_cases[i].input,
                            metrics::OmniboxEventProto::OTHER,
                            TestingSchemeClassifier());
    kw_provider_->Start(input, false);
    EXPECT_TRUE(kw_provider_->done());
    matches = kw_provider_->matches();
    ASSERT_EQ(keyword_cases[i].num_results, matches.size());
    for (size_t j = 0; j < matches.size(); ++j) {
      EXPECT_EQ(keyword_cases[i].output[j].member, matches[j].*member);
      EXPECT_EQ(keyword_cases[i].output[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
  }
}

TEST_F(KeywordProviderTest, Edit) {
  const MatchType<base::string16> kEmptyMatch = { base::string16(), false };
  TestData<base::string16> edit_cases[] = {
      // Searching for a nonexistent prefix should give nothing.
      {ASCIIToUTF16("Not Found"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("aaaaaNot Found"),
       0,
       {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Check that tokenization only collapses whitespace between first tokens,
      // no-query-input cases have a space appended, and action is not escaped.
      {ASCIIToUTF16("z"),
       1,
       {{ASCIIToUTF16("z "), true}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("z    \t"),
       1,
       {{ASCIIToUTF16("z "), true}, kEmptyMatch, kEmptyMatch}},

      // Check that exact, substituting keywords with a verbatim search term
      // don't generate a result.  (These are handled by SearchProvider.)
      {ASCIIToUTF16("z foo"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("z   a   b   c++"),
       0,
       {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Matches should be limited to three, and sorted in quality order, not
      // alphabetical.
      {ASCIIToUTF16("aaa"),
       2,
       {{ASCIIToUTF16("aaaa "), false},
        {ASCIIToUTF16("aaaaa "), false},
        kEmptyMatch}},
      {ASCIIToUTF16("a 1 2 3"),
       3,
       {{ASCIIToUTF16("aa 1 2 3"), false},
        {ASCIIToUTF16("ab 1 2 3"), false},
        {ASCIIToUTF16("aaaa 1 2 3"), false}}},
      {ASCIIToUTF16("www.a"),
       3,
       {{ASCIIToUTF16("aa "), false},
        {ASCIIToUTF16("ab "), false},
        {ASCIIToUTF16("aaaa "), false}}},
      {ASCIIToUTF16("foo hello"),
       2,
       {{ASCIIToUTF16("fooshort.com hello"), false},
        {ASCIIToUTF16("foolong.co.uk hello"), false},
        kEmptyMatch}},
      // Exact matches should prevent returning inexact matches.  Also, the
      // verbatim query for this keyword match should not be returned.  (It's
      // returned by SearchProvider.)
      {ASCIIToUTF16("aaaa foo"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("www.aaaa foo"),
       0,
       {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Matches should be retrieved by typing the prefix of the keyword, not
      // the
      // domain name.
      {ASCIIToUTF16("host foo"),
       1,
       {{ASCIIToUTF16("host.site.com foo"), false}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("host.site foo"),
       1,
       {{ASCIIToUTF16("host.site.com foo"), false}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("site foo"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Clean up keyword input properly.  "http" and "https" are the only
      // allowed schemes.
      {ASCIIToUTF16("www"),
       1,
       {{ASCIIToUTF16("www "), true}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("www."), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      // In this particular example, stripping the "www." from "www.FOO" means
      // we can allow matching against keywords that explicitly start with
      // "FOO", even if "FOO" happens to be "www".  It's a little odd yet it
      // seems reasonable.
      {ASCIIToUTF16("www.w w"),
       3,
       {{ASCIIToUTF16("www w"), false},
        {ASCIIToUTF16("weasel w"), false},
        {ASCIIToUTF16("www.cleantestv2.com w"), false}}},
      {ASCIIToUTF16("http://www"),
       1,
       {{ASCIIToUTF16("www "), true}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("http://www."), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("ftp: blah"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("mailto:z"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("ftp://z"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("https://z"),
       1,
       {{ASCIIToUTF16("z "), true}, kEmptyMatch, kEmptyMatch}},

      // Non-substituting keywords, whether typed fully or not
      // should not add a space.
      {ASCIIToUTF16("nonsu"),
       1,
       {{ASCIIToUTF16("nonsub"), false}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("nonsub"),
       1,
       {{ASCIIToUTF16("nonsub"), true}, kEmptyMatch, kEmptyMatch}},
  };

  SetUpClientAndKeywordProvider();
  RunTest<base::string16>(edit_cases, base::size(edit_cases),
                          &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, DomainMatches) {
  const MatchType<base::string16> kEmptyMatch = { base::string16(), false };
  TestData<base::string16> edit_cases[] = {
    // Searching for a nonexistent prefix should give nothing.
    { ASCIIToUTF16("Not Found"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("aaaaaNot Found"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },

    // Matches should be limited to three and sorted in quality order.
    // This order depends on whether we're using the pre-domain-name text
    // for matching--when matching the domain, we sort by the length of the
    // domain, not the length of the whole keyword.
    { ASCIIToUTF16("ignore foo"), 2,
      { { ASCIIToUTF16("ignoreme.domain2.com foo"), false },
        { ASCIIToUTF16("ignoremelong.domain.com foo"), false },
        kEmptyMatch } },
    { ASCIIToUTF16("dom foo"), 2,
      { { ASCIIToUTF16("ignoremelong.domain.com foo"), false },
        { ASCIIToUTF16("ignoreme.domain2.com foo"), false },
        kEmptyMatch } },

    // Matches should be retrieved by typing the domain name, not only
    // a prefix to the keyword.
    { ASCIIToUTF16("host foo"), 1,
      { { ASCIIToUTF16("host.site.com foo"), false },
        kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("host.site foo"), 1,
      { { ASCIIToUTF16("host.site.com foo"), false },
        kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("site foo"), 1,
      { { ASCIIToUTF16("host.site.com foo"), false },
        kEmptyMatch, kEmptyMatch } },
  };

  // Add a rule enabling matching in the domain name of keywords (i.e.,
  // non-prefix matching).
  {
    std::map<std::string, std::string> params;
    params[OmniboxFieldTrial::kKeywordRequiresPrefixMatchRule] = "false";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  SetUpClientAndKeywordProvider();
  RunTest<base::string16>(edit_cases, base::size(edit_cases),
                          &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, IgnoreRegistryForScoring) {
  const MatchType<base::string16> kEmptyMatch = { base::string16(), false };
  TestData<base::string16> edit_cases[] = {
    // Matches should be limited to three and sorted in quality order.
    // When ignoring the registry length, this order of suggestions should
    // result (sorted by keyword length sans registry).  The "Edit" test case
    // has this exact test for when not ignoring the registry to check that
    // the other order (shorter full keyword) results there.
    { ASCIIToUTF16("foo hello"), 2,
      { { ASCIIToUTF16("foolong.co.uk hello"), false },
        { ASCIIToUTF16("fooshort.com hello"), false },
        kEmptyMatch } },

    // Keywords that don't have full hostnames should keep the same order
    // as normal.
    { ASCIIToUTF16("aaa"), 2,
      { { ASCIIToUTF16("aaaa "), false },
        { ASCIIToUTF16("aaaaa "), false },
        kEmptyMatch } },
    { ASCIIToUTF16("a 1 2 3"), 3,
     { { ASCIIToUTF16("aa 1 2 3"), false },
       { ASCIIToUTF16("ab 1 2 3"), false },
       { ASCIIToUTF16("aaaa 1 2 3"), false } } },
    { ASCIIToUTF16("www.a"), 3,
      { { ASCIIToUTF16("aa "), false },
        { ASCIIToUTF16("ab "), false },
        { ASCIIToUTF16("aaaa "), false } } },
  };

  // Add a rule to make matching in the registry portion of a keyword
  // unimportant.
  {
    std::map<std::string, std::string> params;
    params[OmniboxFieldTrial::kKeywordRequiresRegistryRule] = "false";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  SetUpClientAndKeywordProvider();
  RunTest<base::string16>(edit_cases, base::size(edit_cases),
                          &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, DISABLED_URL) {
  const MatchType<GURL> kEmptyMatch = { GURL(), false };
  TestData<GURL> url_cases[] = {
      // No query input -> empty destination URL.
      {ASCIIToUTF16("z"), 1, {{GURL(), true}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("z    \t"), 1, {{GURL(), true}, kEmptyMatch, kEmptyMatch}},

      // Check that tokenization only collapses whitespace between first tokens
      // and query input, but not rest of URL, is escaped.
      {ASCIIToUTF16("w  bar +baz"),
       3,
       {{GURL(" +%2B?=bar+%2Bbazfoo "), false},
        {GURL("bar+%2Bbaz=z"), false},
        {GURL("http://www.cleantestv2.com/?q=bar+%2Bbaz"), false}}},

      // Substitution should work with various locations of the "%s".
      {ASCIIToUTF16("aaa 1a2b"),
       2,
       {{GURL("http://aaaa/?aaaa=1&b=1a2b&c"), false},
        {GURL("1a2b"), false},
        kEmptyMatch}},
      {ASCIIToUTF16("a 1 2 3"),
       3,
       {{GURL("aa.com?foo=1+2+3"), false},
        {GURL("bogus URL 1+2+3"), false},
        {GURL("http://aaaa/?aaaa=1&b=1+2+3&c"), false}}},
      {ASCIIToUTF16("www.w w"),
       3,
       {{GURL(" +%2B?=wfoo "), false},
        {GURL("weaselwweasel"), false},
        {GURL("http://www.cleantestv2.com/?q=w"), false}}},
  };

  SetUpClientAndKeywordProvider();
  RunTest<GURL>(url_cases, base::size(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, Contents) {
  const MatchType<base::string16> kEmptyMatch = { base::string16(), false };
  TestData<base::string16> contents_cases[] = {
      // No query input -> substitute "<Type search term>" into contents.
      {ASCIIToUTF16("z"),
       1,
       {{ASCIIToUTF16("<Type search term>"), true}, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("z    \t"),
       1,
       {{ASCIIToUTF16("<Type search term>"), true}, kEmptyMatch, kEmptyMatch}},

      // Exact keyword matches with remaining text should return nothing.
      {ASCIIToUTF16("www.www www"), 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {ASCIIToUTF16("z   a   b   c++"),
       0,
       {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Exact keyword matches with remaining text when the keyword is an
      // extension keyword should return something.  This is tested in
      // chrome/browser/extensions/api/omnibox/omnibox_apitest.cc's
      // in OmniboxApiTest's Basic test.

      // There are two keywords that start with "aaa".  Suggestions will be
      // disambiguated by the description.  We do not test the description value
      // here because KeywordProvider doesn't set descriptions; these are
      // populated later by AutocompleteController.
      {ASCIIToUTF16("aaa"),
       2,
       {{ASCIIToUTF16("<Type search term>"), false},
        {ASCIIToUTF16("<Type search term>"), false},
        kEmptyMatch}},
      // When there is a search string, simply display it.
      {ASCIIToUTF16("www.w w"),
       3,
       {{ASCIIToUTF16("w"), false},
        {ASCIIToUTF16("w"), false},
        {ASCIIToUTF16("w"), false}}},
      // Also, check that tokenization only collapses whitespace between first
      // tokens and contents are not escaped or unescaped.
      {ASCIIToUTF16("a   1 2+ 3"),
       3,
       {{ASCIIToUTF16("1 2+ 3"), false},
        {ASCIIToUTF16("1 2+ 3"), false},
        {ASCIIToUTF16("1 2+ 3"), false}}},
  };

  SetUpClientAndKeywordProvider();
  RunTest<base::string16>(contents_cases, base::size(contents_cases),
                          &AutocompleteMatch::contents);
}

TEST_F(KeywordProviderTest, AddKeyword) {
  SetUpClientAndKeywordProvider();
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("Test"));
  base::string16 keyword(ASCIIToUTF16("foo"));
  data.SetKeyword(keyword);
  data.SetURL("http://www.google.com/foo?q={searchTerms}");
  TemplateURL* template_url = client_->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(
      template_url ==
      client_->GetTemplateURLService()->GetTemplateURLForKeyword(keyword));
}

TEST_F(KeywordProviderTest, RemoveKeyword) {
  SetUpClientAndKeywordProvider();
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  base::string16 url(ASCIIToUTF16("http://aaaa/?aaaa=1&b={searchTerms}&c"));
  template_url_service->Remove(
      template_url_service->GetTemplateURLForKeyword(ASCIIToUTF16("aaaa")));
  ASSERT_TRUE(template_url_service->GetTemplateURLForKeyword(
                  ASCIIToUTF16("aaaa")) == nullptr);
}

TEST_F(KeywordProviderTest, GetKeywordForInput) {
  SetUpClientAndKeywordProvider();
  EXPECT_EQ(ASCIIToUTF16("aa"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("aa")));
  EXPECT_EQ(base::string16(),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("aafoo")));
  EXPECT_EQ(base::string16(),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("aa foo")));
  EXPECT_EQ(
      ASCIIToUTF16("cleantestv1.com"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("http://cleantestv1.com")));
  EXPECT_EQ(
      ASCIIToUTF16("cleantestv1.com"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("www.cleantestv1.com")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv1.com"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv1.com/")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv1.com"),
            kw_provider_->GetKeywordForText(
                ASCIIToUTF16("https://www.cleantestv1.com/")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv2.com"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv2.com")));
  EXPECT_EQ(
      ASCIIToUTF16("www.cleantestv2.com"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("www.cleantestv2.com")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv2.com"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv2.com/")));
  EXPECT_EQ(
      ASCIIToUTF16("www.cleantestv3.com"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("www.cleantestv3.com")));
  EXPECT_EQ(base::string16(),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv3.com")));
  EXPECT_EQ(
      ASCIIToUTF16("http://cleantestv4.com"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("http://cleantestv4.com")));
  EXPECT_EQ(base::string16(),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv4.com")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv5.com"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv5.com")));
  EXPECT_EQ(
      ASCIIToUTF16("http://cleantestv5.com"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("http://cleantestv5.com")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv6:"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv6:")));
  EXPECT_EQ(base::string16(),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv6")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv7/"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv7/")));
  EXPECT_EQ(base::string16(),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv7")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv8/"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv8/")));
  EXPECT_EQ(ASCIIToUTF16("cleantestv8"),
            kw_provider_->GetKeywordForText(ASCIIToUTF16("cleantestv8")));
}

TEST_F(KeywordProviderTest, GetSubstitutingTemplateURLForInput) {
  struct {
    const std::string text;
    const size_t cursor_position;
    const bool allow_exact_keyword_match;
    const std::string expected_url;
    const std::string updated_text;
    const size_t updated_cursor_position;
  } cases[] = {
    { "foo", base::string16::npos, true, "", "foo", base::string16::npos },
    { "aa foo", base::string16::npos, true, "aa.com?foo={searchTerms}", "foo",
      base::string16::npos },

    // Cursor adjustment.
    { "aa foo", base::string16::npos, true, "aa.com?foo={searchTerms}", "foo",
      base::string16::npos },
    { "aa foo", 4u, true, "aa.com?foo={searchTerms}", "foo", 1u },
    // Cursor at the end.
    { "aa foo", 6u, true, "aa.com?foo={searchTerms}", "foo", 3u },
    // Cursor before the first character of the remaining text.
    { "aa foo", 3u, true, "aa.com?foo={searchTerms}", "foo", 0u },

    // Trailing space.
    { "aa foo ", 7u, true, "aa.com?foo={searchTerms}", "foo ", 4u },
    // Trailing space without remaining text, cursor in the middle.
    { "aa  ", 3u, true, "aa.com?foo={searchTerms}", "", base::string16::npos },
    // Trailing space without remaining text, cursor at the end.
    { "aa  ", 4u, true, "aa.com?foo={searchTerms}", "", base::string16::npos },
    // Extra space after keyword, cursor at the end.
    { "aa  foo ", 8u, true, "aa.com?foo={searchTerms}", "foo ", 4u },
    // Extra space after keyword, cursor in the middle.
    { "aa  foo ", 3u, true, "aa.com?foo={searchTerms}", "foo ", 0 },
    // Extra space after keyword, no trailing space, cursor at the end.
    { "aa  foo", 7u, true, "aa.com?foo={searchTerms}", "foo", 3u },
    // Extra space after keyword, no trailing space, cursor in the middle.
    { "aa  foo", 5u, true, "aa.com?foo={searchTerms}", "foo", 1u },

    // Disallow exact keyword match.
    { "aa foo", base::string16::npos, false, "", "aa foo",
      base::string16::npos },
  };
  SetUpClientAndKeywordProvider();
  for (size_t i = 0; i < base::size(cases); i++) {
    AutocompleteInput input(
        ASCIIToUTF16(cases[i].text), cases[i].cursor_position,
        metrics::OmniboxEventProto::OTHER, TestingSchemeClassifier());
    input.set_allow_exact_keyword_match(cases[i].allow_exact_keyword_match);
    const TemplateURL* url =
        KeywordProvider::GetSubstitutingTemplateURLForInput(
            client_->GetTemplateURLService(), &input);
    if (cases[i].expected_url.empty())
      EXPECT_FALSE(url);
    else
      EXPECT_EQ(cases[i].expected_url, url->url());
    EXPECT_EQ(ASCIIToUTF16(cases[i].updated_text), input.text());
    EXPECT_EQ(cases[i].updated_cursor_position, input.cursor_position());
  }
}

// If extra query params are specified on the command line, they should be
// reflected (only) in the default search provider's destination URL.
TEST_F(KeywordProviderTest, ExtraQueryParams) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");

  TestData<GURL> url_cases[] = {
    { ASCIIToUTF16("a 1 2 3"), 3,
      { { GURL("aa.com?a=b&foo=1+2+3"), false },
        { GURL("bogus URL 1+2+3"), false },
        { GURL("http://aaaa/?aaaa=1&b=1+2+3&c"), false } } },
  };

  SetUpClientAndKeywordProvider();
  RunTest<GURL>(url_cases, base::size(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, DoesNotProvideMatchesOnFocus) {
  SetUpClientAndKeywordProvider();
  AutocompleteInput input(ASCIIToUTF16("aaa"),
                          metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  input.set_from_omnibox_focus(true);
  kw_provider_->Start(input, false);
  ASSERT_TRUE(kw_provider_->matches().empty());
}
