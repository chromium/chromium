// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/keyword_provider.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
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

const TemplateURLService::Initializer kTestData[] = {
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

}  // namespace

class KeywordProviderTest : public testing::Test {
 protected:
  template <class ResultType>
  struct MatchType {
    const ResultType member;
    bool allowed_to_be_default_match;
  };

  template <class ResultType>
  struct TestData {
    const std::u16string input;
    const size_t num_results;
    const MatchType<ResultType> output[3];
  };

  KeywordProviderTest() : kw_provider_(nullptr) {}
  ~KeywordProviderTest() override = default;

  void SetUp() override;

  template <class ResultType>
  void RunTest(TestData<ResultType>* keyword_cases,
               int num_cases,
               ResultType AutocompleteMatch::*member);

 protected:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_{
      {.template_url_service_initializer = kTestData}};
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<KeywordProvider> kw_provider_;
};

void KeywordProviderTest::SetUp() {
  client_ = std::make_unique<MockAutocompleteProviderClient>();
  client_->set_template_url_service(
      search_engines_test_environment_.template_url_service());
  kw_provider_ = new KeywordProvider(client_.get(), nullptr);
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
  const MatchType<std::u16string> kEmptyMatch = {std::u16string(), false};
  TestData<std::u16string> edit_cases[] = {
      // Searching for a nonexistent prefix should give nothing.
      {u"Not Found", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"aaaaaNot Found", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Check that tokenization only collapses whitespace between first tokens,
      // no-query-input cases have a space appended, and action is not escaped.
      {u"z", 1, {{u"z ", true}, kEmptyMatch, kEmptyMatch}},
      {u"z    \t", 1, {{u"z ", true}, kEmptyMatch, kEmptyMatch}},

      // Check that exact, substituting keywords with a verbatim search term
      // don't generate a result.  (These are handled by SearchProvider.)
      {u"z foo", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"z   a   b   c++", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Matches should be limited to three, and sorted in quality order, not
      // alphabetical.
      {u"aaa", 2, {{u"aaaa ", false}, {u"aaaaa ", false}, kEmptyMatch}},
      {u"a 1 2 3",
       3,
       {{u"aa 1 2 3", false}, {u"ab 1 2 3", false}, {u"aaaa 1 2 3", false}}},
      {u"www.a", 3, {{u"aa ", false}, {u"ab ", false}, {u"aaaa ", false}}},
      {u"foo hello",
       2,
       {{u"fooshort.com hello", false},
        {u"foolong.co.uk hello", false},
        kEmptyMatch}},
      // Exact matches should prevent returning inexact matches.  Also, the
      // verbatim query for this keyword match should not be returned.  (It's
      // returned by SearchProvider.)
      {u"aaaa foo", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"www.aaaa foo", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Matches should be retrieved by typing the prefix of the keyword, not
      // the
      // domain name.
      {u"host foo",
       1,
       {{u"host.site.com foo", false}, kEmptyMatch, kEmptyMatch}},
      {u"host.site foo",
       1,
       {{u"host.site.com foo", false}, kEmptyMatch, kEmptyMatch}},
      {u"site foo", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Clean up keyword input properly.  "http" and "https" are the only
      // allowed schemes.
      {u"www", 1, {{u"www ", true}, kEmptyMatch, kEmptyMatch}},
      {u"www.", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      // In this particular example, stripping the "www." from "www.FOO" means
      // we can allow matching against keywords that explicitly start with
      // "FOO", even if "FOO" happens to be "www".  It's a little odd yet it
      // seems reasonable.
      {u"www.w w",
       3,
       {{u"www w", false},
        {u"weasel w", false},
        {u"www.cleantestv2.com w", false}}},
      {u"http://www", 1, {{u"www ", true}, kEmptyMatch, kEmptyMatch}},
      {u"http://www.", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"ftp: blah", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"mailto:z", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"ftp://z", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"https://z", 1, {{u"z ", true}, kEmptyMatch, kEmptyMatch}},

      // Non-substituting keywords, whether typed fully or not
      // should not add a space.
      {u"nonsu", 1, {{u"nonsub", false}, kEmptyMatch, kEmptyMatch}},
      {u"nonsub", 1, {{u"nonsub", true}, kEmptyMatch, kEmptyMatch}},
  };

  RunTest<std::u16string>(edit_cases, std::size(edit_cases),
                          &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, URL) {
  const MatchType<GURL> kEmptyMatch = { GURL(), false };
  TestData<GURL> url_cases[] = {
      // No query input -> empty destination URL.
      {u"z", 1, {{GURL(), true}, kEmptyMatch, kEmptyMatch}},
      {u"z    \t", 1, {{GURL(), true}, kEmptyMatch, kEmptyMatch}},

      // Check that tokenization only collapses whitespace between first tokens
      // and query input, but not rest of URL, is escaped.
      {u"w  bar +baz",
       3,
       {{GURL(" +%2B?=bar+%2Bbazfoo "), false},
        {GURL("bar+%2Bbaz=z"), false},
        {GURL("http://www.cleantestv2.com/?q=bar+%2Bbaz"), false}}},

      // Substitution should work with various locations of the "%s".
      {u"aaa 1a2b",
       2,
       {{GURL("http://aaaa/?aaaa=1&b=1a2b&c"), false},
        {GURL("1a2b"), false},
        kEmptyMatch}},
      {u"a 1 2 3",
       3,
       {{GURL("aa.com?foo=1+2+3"), false},
        {GURL("bogus URL 1+2+3"), false},
        {GURL("http://aaaa/?aaaa=1&b=1+2+3&c"), false}}},
      {u"www.w w",
       3,
       {{GURL(" +%2B?=wfoo "), false},
        {GURL("weaselwweasel"), false},
        {GURL("http://www.cleantestv2.com/?q=w"), false}}},
  };

  RunTest<GURL>(url_cases, std::size(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, Contents) {
  const MatchType<std::u16string> kEmptyMatch = {std::u16string(), false};
  TestData<std::u16string> contents_cases[] = {
      // No query input -> substitute "<Type search term>" into contents.
      {u"z", 1, {{u"<Type search term>", true}, kEmptyMatch, kEmptyMatch}},
      {u"z    \t",
       1,
       {{u"<Type search term>", true}, kEmptyMatch, kEmptyMatch}},

      // Exact keyword matches with remaining text should return nothing.
      {u"www.www www", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},
      {u"z   a   b   c++", 0, {kEmptyMatch, kEmptyMatch, kEmptyMatch}},

      // Exact keyword matches with remaining text when the keyword is an
      // extension keyword should return something.  This is tested in
      // chrome/browser/extensions/api/omnibox/omnibox_apitest.cc's
      // in OmniboxApiTest's Basic test.

      // There are two keywords that start with "aaa".  Suggestions will be
      // disambiguated by the description.  We do not test the description value
      // here because KeywordProvider doesn't set descriptions; these are
      // populated later by AutocompleteController.
      {u"aaa",
       2,
       {{u"<Type search term>", false},
        {u"<Type search term>", false},
        kEmptyMatch}},
      // When there is a search string, simply display it.
      {u"www.w w", 3, {{u"w", false}, {u"w", false}, {u"w", false}}},
      // Also, check that tokenization only collapses whitespace between first
      // tokens and contents are not escaped or unescaped.
      {u"a   1 2+ 3",
       3,
       {{u"1 2+ 3", false}, {u"1 2+ 3", false}, {u"1 2+ 3", false}}},
  };

  RunTest<std::u16string>(contents_cases, std::size(contents_cases),
                          &AutocompleteMatch::contents);
}

TEST_F(KeywordProviderTest, AddKeyword) {
  TemplateURLData data;
  data.SetShortName(u"Test");
  std::u16string keyword(u"foo");
  data.SetKeyword(keyword);
  data.SetURL("http://www.google.com/foo?q={searchTerms}");
  TemplateURL* template_url = client_->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(
      template_url ==
      client_->GetTemplateURLService()->GetTemplateURLForKeyword(keyword));
}

TEST_F(KeywordProviderTest, RemoveKeyword) {
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  std::u16string url(u"http://aaaa/?aaaa=1&b={searchTerms}&c");
  template_url_service->Remove(
      template_url_service->GetTemplateURLForKeyword(u"aaaa"));
  ASSERT_TRUE(template_url_service->GetTemplateURLForKeyword(u"aaaa") ==
              nullptr);
}

TEST_F(KeywordProviderTest, GetKeywordForInput) {
  EXPECT_EQ(u"aa", kw_provider_->GetKeywordForText(u"aa"));
  EXPECT_EQ(std::u16string(), kw_provider_->GetKeywordForText(u"aafoo"));
  EXPECT_EQ(u"aa", kw_provider_->GetKeywordForText(u"aa foo"));
  EXPECT_EQ(u"cleantestv1.com",
            kw_provider_->GetKeywordForText(u"http://cleantestv1.com"));
  EXPECT_EQ(u"cleantestv1.com",
            kw_provider_->GetKeywordForText(u"www.cleantestv1.com"));
  EXPECT_EQ(u"cleantestv1.com",
            kw_provider_->GetKeywordForText(u"cleantestv1.com/"));
  EXPECT_EQ(u"cleantestv1.com",
            kw_provider_->GetKeywordForText(u"https://www.cleantestv1.com/"));
  EXPECT_EQ(u"cleantestv2.com",
            kw_provider_->GetKeywordForText(u"cleantestv2.com"));
  EXPECT_EQ(u"www.cleantestv2.com",
            kw_provider_->GetKeywordForText(u"www.cleantestv2.com"));
  EXPECT_EQ(u"cleantestv2.com",
            kw_provider_->GetKeywordForText(u"cleantestv2.com/"));
  EXPECT_EQ(u"www.cleantestv3.com",
            kw_provider_->GetKeywordForText(u"www.cleantestv3.com"));
  EXPECT_EQ(std::u16string(),
            kw_provider_->GetKeywordForText(u"cleantestv3.com"));
  EXPECT_EQ(u"http://cleantestv4.com",
            kw_provider_->GetKeywordForText(u"http://cleantestv4.com"));
  EXPECT_EQ(std::u16string(),
            kw_provider_->GetKeywordForText(u"cleantestv4.com"));
  EXPECT_EQ(u"cleantestv5.com",
            kw_provider_->GetKeywordForText(u"cleantestv5.com"));
  EXPECT_EQ(u"http://cleantestv5.com",
            kw_provider_->GetKeywordForText(u"http://cleantestv5.com"));
  EXPECT_EQ(u"cleantestv6:", kw_provider_->GetKeywordForText(u"cleantestv6:"));
  EXPECT_EQ(std::u16string(), kw_provider_->GetKeywordForText(u"cleantestv6"));
  EXPECT_EQ(u"cleantestv7/", kw_provider_->GetKeywordForText(u"cleantestv7/"));
  EXPECT_EQ(std::u16string(), kw_provider_->GetKeywordForText(u"cleantestv7"));
  EXPECT_EQ(u"cleantestv8/", kw_provider_->GetKeywordForText(u"cleantestv8/"));
  EXPECT_EQ(u"cleantestv8", kw_provider_->GetKeywordForText(u"cleantestv8"));
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
      {"foo", std::u16string::npos, true, "", "foo", std::u16string::npos},
      {"aa foo", std::u16string::npos, true, "aa.com?foo={searchTerms}", "foo",
       std::u16string::npos},

      // Cursor adjustment.
      {"aa foo", std::u16string::npos, true, "aa.com?foo={searchTerms}", "foo",
       std::u16string::npos},
      {"aa foo", 4u, true, "aa.com?foo={searchTerms}", "foo", 1u},
      // Cursor at the end.
      {"aa foo", 6u, true, "aa.com?foo={searchTerms}", "foo", 3u},
      // Cursor before the first character of the remaining text.
      {"aa foo", 3u, true, "aa.com?foo={searchTerms}", "foo", 0u},

      // Trailing space.
      {"aa foo ", 7u, true, "aa.com?foo={searchTerms}", "foo ", 4u},
      // Trailing space without remaining text, cursor in the middle.
      {"aa  ", 3u, true, "aa.com?foo={searchTerms}", "", std::u16string::npos},
      // Trailing space without remaining text, cursor at the end.
      {"aa  ", 4u, true, "aa.com?foo={searchTerms}", "", std::u16string::npos},
      // Extra space after keyword, cursor at the end.
      {"aa  foo ", 8u, true, "aa.com?foo={searchTerms}", "foo ", 4u},
      // Extra space after keyword, cursor in the middle.
      {"aa  foo ", 3u, true, "aa.com?foo={searchTerms}", "foo ", 0},
      // Extra space after keyword, no trailing space, cursor at the end.
      {"aa  foo", 7u, true, "aa.com?foo={searchTerms}", "foo", 3u},
      // Extra space after keyword, no trailing space, cursor in the middle.
      {"aa  foo", 5u, true, "aa.com?foo={searchTerms}", "foo", 1u},

      // Disallow exact keyword match.
      {"aa foo", std::u16string::npos, false, "", "aa foo",
       std::u16string::npos},
  };
  for (size_t i = 0; i < std::size(cases); i++) {
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
      {u"a 1 2 3",
       3,
       {{GURL("aa.com?a=b&foo=1+2+3"), false},
        {GURL("bogus URL 1+2+3"), false},
        {GURL("http://aaaa/?aaaa=1&b=1+2+3&c"), false}}},
  };

  RunTest<GURL>(url_cases, std::size(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, DoesNotProvideMatchesOnFocus) {
  AutocompleteInput input(u"aaa", metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  kw_provider_->Start(input, false);
  ASSERT_TRUE(kw_provider_->matches().empty());
}
