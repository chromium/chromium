// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <stddef.h>

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bool EqualClassifications(const ACMatchClassifications& lhs,
                          const ACMatchClassifications& rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t n = 0; n < lhs.size(); ++n)
    if (lhs[n].style != rhs[n].style || lhs[n].offset != rhs[n].offset)
      return false;
  return true;
}

}  // namespace

TEST(AutocompleteMatchTest, MoreRelevant) {
  struct RelevantCases {
    int r1;
    int r2;
    bool expected_result;
  } cases[] = {
    {  10,   0, true  },
    {  10,  -5, true  },
    {  -5,  10, false },
    {   0,  10, false },
    { -10,  -5, false  },
    {  -5, -10, true },
  };

  AutocompleteMatch m1(nullptr, 0, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  AutocompleteMatch m2(nullptr, 0, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);

  for (size_t i = 0; i < base::size(cases); ++i) {
    m1.relevance = cases[i].r1;
    m2.relevance = cases[i].r2;
    EXPECT_EQ(cases[i].expected_result,
              AutocompleteMatch::MoreRelevant(m1, m2));
  }
}

TEST(AutocompleteMatchTest, MergeClassifications) {
  // Merging two empty vectors should result in an empty vector.
  EXPECT_EQ(std::string(),
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ACMatchClassifications())));

  // If one vector is empty and the other is "trivial" but non-empty (i.e. (0,
  // NONE)), the non-empty vector should be returned.
  EXPECT_EQ("0,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,0"),
              AutocompleteMatch::ACMatchClassifications())));
  EXPECT_EQ("0,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ClassificationsFromString("0,0"))));

  // Ditto if the one-entry vector is non-trivial.
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1"),
              AutocompleteMatch::ACMatchClassifications())));
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ClassificationsFromString("0,1"))));

  // Merge an unstyled one-entry vector with a styled one-entry vector.
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,0"),
              AutocompleteMatch::ClassificationsFromString("0,1"))));

  // Test simple cases of overlap.
  EXPECT_EQ("0,3," "1,2",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1," "1,0"),
              AutocompleteMatch::ClassificationsFromString("0,2"))));
  EXPECT_EQ("0,3," "1,2",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,2"),
              AutocompleteMatch::ClassificationsFromString("0,1," "1,0"))));

  // Test the case where both vectors have classifications at the same
  // positions.
  EXPECT_EQ("0,3",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1," "1,2"),
              AutocompleteMatch::ClassificationsFromString("0,2," "1,1"))));

  // Test an arbitrary complicated case.
  EXPECT_EQ("0,2," "1,0," "2,1," "4,3," "5,7," "6,3," "7,7," "15,1," "17,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString(
                  "0,0," "2,1," "4,3," "7,7," "10,6," "15,0"),
              AutocompleteMatch::ClassificationsFromString(
                  "0,2," "1,0," "5,7," "6,1," "17,0"))));
}

TEST(AutocompleteMatchTest, InlineTailPrefix) {
  struct TestData {
    std::string before_contents, after_contents;
    ACMatchClassifications before_contents_class, after_contents_class;
  } cases[] = {
      {"90123456",
       "... 90123456",
       // should prepend ellipsis, and offset remainder
       {{0, ACMatchClassification::NONE}, {2, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::NONE}, {6, ACMatchClassification::MATCH}}},
      {"90123456",
       "... 90123456",
       // should prepend ellipsis
       {},
       {{0, ACMatchClassification::NONE}}},
  };
  for (const auto& test_case : cases) {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
    match.contents = base::UTF8ToUTF16(test_case.before_contents);
    match.contents_class = test_case.before_contents_class;
    match.InlineTailPrefix(base::UTF8ToUTF16("12345678"));
    EXPECT_EQ(match.contents, base::UTF8ToUTF16(test_case.after_contents));
    EXPECT_TRUE(EqualClassifications(match.contents_class,
                                     test_case.after_contents_class));
  }
}

TEST(AutocompleteMatchTest, GetMatchComponents) {
  struct MatchComponentsTestData {
    const std::string url;
    std::vector<std::string> input_terms;
    bool expected_match_in_scheme;
    bool expected_match_in_subdomain;
  };

  MatchComponentsTestData test_cases[] = {
      // Match in scheme.
      {"http://www.google.com", {"ht"}, true, false},
      // Match within the scheme, but not starting at the beginning, i.e. "ttp".
      {"http://www.google.com", {"tp"}, false, false},
      // Sanity check that HTTPS still works.
      {"https://www.google.com", {"http"}, true, false},

      // Match within the subdomain.
      {"http://www.google.com", {"www"}, false, true},
      {"http://www.google.com", {"www."}, false, true},
      // Don't consider matches on the '.' delimiter as a match_in_subdomain.
      {"http://www.google.com", {"."}, false, false},
      {"http://www.google.com", {".goo"}, false, false},
      // Matches within the domain.
      {"http://www.google.com", {"goo"}, false, false},
      // Verify that in private registries, we detect matches in subdomains.
      {"http://www.appspot.com", {"www"}, false, true},

      // Matches spanning the scheme, subdomain, and domain.
      {"http://www.google.com", {"http://www.goo"}, true, true},
      {"http://www.google.com", {"ht", "www"}, true, true},
      // But we should not flag match_in_subdomain if there is no subdomain.
      {"http://google.com", {"http://goo"}, true, false},

      // Matches spanning the subdomain and path.
      {"http://www.google.com/abc", {"www.google.com/ab"}, false, true},
      {"http://www.google.com/abc", {"www", "ab"}, false, true},

      // Matches spanning the scheme, subdomain, and path.
      {"http://www.google.com/abc", {"http://www.google.com/ab"}, true, true},
      {"http://www.google.com/abc", {"ht", "ww", "ab"}, true, true},

      // Intranet sites.
      {"http://foobar/biz", {"foobar"}, false, false},
      {"http://foobar/biz", {"biz"}, false, false},

      // Ensure something sane happens when the URL input is invalid.
      {"", {""}, false, false},
      {"foobar", {"bar"}, false, false},
  };
  for (auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << " url=" << test_case.url << " first input term="
                 << test_case.input_terms[0] << " expected_match_in_scheme="
                 << test_case.expected_match_in_scheme
                 << " expected_match_in_subdomain="
                 << test_case.expected_match_in_subdomain);
    bool match_in_scheme = false;
    bool match_in_subdomain = false;
    std::vector<AutocompleteMatch::MatchPosition> match_positions;
    for (auto& term : test_case.input_terms) {
      size_t start = test_case.url.find(term);
      ASSERT_NE(std::string::npos, start);
      size_t end = start + term.size();
      match_positions.push_back(std::make_pair(start, end));
    }
    AutocompleteMatch::GetMatchComponents(GURL(test_case.url), match_positions,
                                          &match_in_scheme,
                                          &match_in_subdomain);
    EXPECT_EQ(test_case.expected_match_in_scheme, match_in_scheme);
    EXPECT_EQ(test_case.expected_match_in_subdomain, match_in_subdomain);
  }
}

TEST(AutocompleteMatchTest, FormatUrlForSuggestionDisplay) {
  // This test does not need to verify url_formatter's functionality in-depth,
  // since url_formatter has its own unit tests. This test is to validate that
  // flipping feature flags and varying the trim_scheme parameter toggles the
  // correct behavior within AutocompleteMatch::GetFormatTypes.
  struct FormatUrlTestData {
    const std::string url;
    bool preserve_scheme;
    bool preserve_subdomain;
    const wchar_t* expected_result;

    void Validate() {
      SCOPED_TRACE(testing::Message()
                   << " url=" << url << " preserve_scheme=" << preserve_scheme
                   << " preserve_subdomain=" << preserve_subdomain
                   << " expected_result=" << expected_result);
      auto format_types = AutocompleteMatch::GetFormatTypes(preserve_scheme,
                                                            preserve_subdomain);
      EXPECT_EQ(base::WideToUTF16(expected_result),
                url_formatter::FormatUrl(GURL(url), format_types,
                                         net::UnescapeRule::SPACES, nullptr,
                                         nullptr, nullptr));
    }
  };

  FormatUrlTestData normal_cases[] = {
      // Test the |preserve_scheme| parameter.
      {"http://google.com", false, false, L"google.com"},
      {"https://google.com", false, false, L"google.com"},
      {"http://google.com", true, false, L"http://google.com"},
      {"https://google.com", true, false, L"https://google.com"},

      // Test the |preserve_subdomain| parameter.
      {"http://www.google.com", false, false, L"google.com"},
      {"http://www.google.com", false, true, L"www.google.com"},

      // Test that paths are preserved in the default case.
      {"http://google.com/foobar", false, false, L"google.com/foobar"},
  };
  for (FormatUrlTestData& test_case : normal_cases)
    test_case.Validate();
}

TEST(AutocompleteMatchTest, SupportsDeletion) {
  // A non-deletable match with no duplicates.
  AutocompleteMatch m(nullptr, 0, false,
                      AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_FALSE(m.SupportsDeletion());

  // A deletable match with no duplicates.
  AutocompleteMatch m1(nullptr, 0, true,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_TRUE(m1.SupportsDeletion());

  // A non-deletable match, with non-deletable duplicates.
  m.duplicate_matches.push_back(AutocompleteMatch(
      nullptr, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  m.duplicate_matches.push_back(AutocompleteMatch(
      nullptr, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  EXPECT_FALSE(m.SupportsDeletion());

  // A non-deletable match, with at least one deletable duplicate.
  m.duplicate_matches.push_back(AutocompleteMatch(
      nullptr, 0, true, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  EXPECT_TRUE(m.SupportsDeletion());
}

// Structure containing URL pairs for deduping-related tests.
struct DuplicateCase {
  const wchar_t* input;
  const std::string url1;
  const std::string url2;
  const bool expected_duplicate;
};

// Runs deduping logic against URLs in |duplicate_case| and makes sure they are
// unique or matched as duplicates as expected.
void CheckDuplicateCase(const DuplicateCase& duplicate_case) {
  SCOPED_TRACE("input=" + base::WideToUTF8(duplicate_case.input) +
               " url1=" + duplicate_case.url1 + " url2=" + duplicate_case.url2);
  AutocompleteInput input(base::WideToUTF16(duplicate_case.input),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteMatch m1(nullptr, 100, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  m1.destination_url = GURL(duplicate_case.url1);
  m1.ComputeStrippedDestinationURL(input, nullptr);
  AutocompleteMatch m2(nullptr, 100, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  m2.destination_url = GURL(duplicate_case.url2);
  m2.ComputeStrippedDestinationURL(input, nullptr);
  EXPECT_EQ(duplicate_case.expected_duplicate,
            m1.stripped_destination_url == m2.stripped_destination_url);
  EXPECT_TRUE(m1.stripped_destination_url.is_valid());
  EXPECT_TRUE(m2.stripped_destination_url.is_valid());
}

TEST(AutocompleteMatchTest, Duplicates) {
  DuplicateCase cases[] = {
    { L"g", "http://www.google.com/",  "https://www.google.com/",    true },
    { L"g", "http://www.google.com/",  "http://www.google.com",      true },
    { L"g", "http://google.com/",      "http://www.google.com/",     true },
    { L"g", "http://www.google.com/",  "HTTP://www.GOOGLE.com/",     true },
    { L"g", "http://www.google.com/",  "http://www.google.com",      true },
    { L"g", "https://www.google.com/", "http://google.com",          true },
    { L"g", "http://www.google.com/",  "wss://www.google.com/",      false },
    { L"g", "http://www.google.com/1", "http://www.google.com/1/",   false },
    { L"g", "http://www.google.com/",  "http://www.google.com/1",    false },
    { L"g", "http://www.google.com/",  "http://www.goo.com/",        false },
    { L"g", "http://www.google.com/",  "http://w2.google.com/",      false },
    { L"g", "http://www.google.com/",  "http://m.google.com/",       false },
    { L"g", "http://www.google.com/",  "http://www.google.com/?foo", false },

    // Don't allow URLs with different schemes to be considered duplicates for
    // certain inputs.
    { L"http://g", "http://google.com/",
                   "https://google.com/",  false },
    { L"http://g", "http://blah.com/",
                   "https://blah.com/",    true  },
    { L"http://g", "http://google.com/1",
                   "https://google.com/1", false },
    { L"http://g hello",    "http://google.com/",
                            "https://google.com/", false },
    { L"hello http://g",    "http://google.com/",
                            "https://google.com/", false },
    { L"hello http://g",    "http://blah.com/",
                            "https://blah.com/",   true  },
    { L"http://b http://g", "http://google.com/",
                            "https://google.com/", false },
    { L"http://b http://g", "http://blah.com/",
                            "https://blah.com/",   false },

    // If the user types unicode that matches the beginning of a
    // punycode-encoded hostname then consider that a match.
    { L"x",               "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", true  },
    { L"http://\x5317 x", "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", false },
    { L"http://\x89c6 x", "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", true  },

    // URLs with hosts containing only `www.` should produce valid stripped urls
    { L"http://www./", "http://www./", "http://google.com/", false },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    CheckDuplicateCase(cases[i]);
  }
}

TEST(AutocompleteMatchTest, DedupeDriveURLs) {
  DuplicateCase cases[] = {
      // Document URLs pointing to the same document, perhaps with different
      // /edit points, hashes, or cgiargs, are deduped.
      {L"docs", "https://docs.google.com/spreadsheets/d/the_doc-id/preview?x=1",
       "https://docs.google.com/spreadsheets/d/the_doc-id/edit?x=2#y=3", true},
      {L"report", "https://drive.google.com/open?id=the-doc-id",
       "https://docs.google.com/spreadsheets/d/the-doc-id/edit?x=2#y=3", true},
      // Similar but different URLs should not be deduped.
      {L"docs", "https://docs.google.com/spreadsheets/d/the_doc-id/preview",
       "https://docs.google.com/spreadsheets/d/another_doc-id/preview", false},
      {L"report", "https://drive.google.com/open?id=the-doc-id",
       "https://drive.google.com/open?id=another-doc-id", false},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    CheckDuplicateCase(cases[i]);
  }
}
