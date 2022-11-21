// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/acronym_matcher.h"

#include "base/containers/adapters.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

using acronym_matcher_constants::kIsFrontOfTokenCharScore;
using acronym_matcher_constants::kIsPrefixCharScore;
using acronym_matcher_constants::kNoMatchScore;

constexpr double kAbsError = 1e-5;

// Returns a string of |text| marked with the hits in |match| using block
// bracket. e.g. text= "Text", match.hits = [{0,1}], returns "[T]ext".
//
// TODO(crbug.com/1336160): Consider defining it as a |test_util| function as it
// has been used for several unit tests.
std::u16string MatchHit(const std::u16string& text,
                        const AcronymMatcher& match) {
  std::u16string marked = text;

  const AcronymMatcher::Hits& hits = match.hits();
  for (const gfx::Range& hit : base::Reversed(hits)) {
    marked.insert(hit.end(), 1, u']');
    marked.insert(hit.start(), 1, u'[');
  }

  return marked;
}

}  // namespace

class AcronymMatcherTest : public testing::Test {};

// Note on expected score calculations:
//
// When a query successfully matches to a text, each letter of the query
// contributes some amount towards a final total. The expected score in
// each test is then the sum over all of the contributions of the individual
// query letters. This is described in more detail in acronym_matcher.cc.
//
// When a query does not successfully match to a text, the overall expected
// score is `kNoMatchScore`.

TEST_F(AcronymMatcherTest, ConsecutiveTokensWithFirstTokenMatch) {
  TokenizedString query(u"abc");
  TokenizedString text(u"axx bxx cxx dxx exx");

  AcronymMatcher am(query, text);
  double expected_score = kIsPrefixCharScore + (kIsFrontOfTokenCharScore * 2);
  EXPECT_NEAR(am.CalculateRelevance(), expected_score, kAbsError);
}

TEST_F(AcronymMatcherTest, ConsecutiveTokensWithNonFirstTokenMatch) {
  TokenizedString query(u"bcd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  AcronymMatcher am(query, text);
  double expected_score = kIsFrontOfTokenCharScore * 3;
  EXPECT_NEAR(am.CalculateRelevance(), expected_score, kAbsError);
}

TEST_F(AcronymMatcherTest, CaseInsensitive) {
  TokenizedString query(u"bCd");
  TokenizedString text(u"axx Bxx cxx Dxx exx");

  AcronymMatcher am(query, text);
  double expected_score = kIsFrontOfTokenCharScore * 3;
  EXPECT_NEAR(am.CalculateRelevance(), expected_score, kAbsError);
}

// PrefixMatcher matches the chars of a given query as prefix of tokens in
// a given text. E.g, query "abc" is a prefix matching of both text "abc dxx"
// and "zxx abcx".
TEST_F(AcronymMatcherTest, PrefixMatchingNotAllowed) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"abc def ghi");

  AcronymMatcher am(query, text);
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(am.CalculateRelevance(), expected_score, kAbsError);
}

TEST_F(AcronymMatcherTest, MixedAcronymAndPrefixMatchingNotAllowed) {
  TokenizedString query(u"adefg");
  TokenizedString text(u"abc def ghi");

  AcronymMatcher am(query, text);
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(am.CalculateRelevance(), expected_score, kAbsError);
}

TEST_F(AcronymMatcherTest, MatchHit) {
  struct {
    const std::u16string text;
    const std::u16string query;
    const std::u16string expect;
  } kTestCases[] = {
      {u"Crash of Crowns", u"coc", u"[C]rash [o]f [C]rowns"},
      {u"Crash of Crowns", u"cra", u"Crash of Crowns"},
      {u"abcxxx bxxx cxxx", u"abc", u"[a]bcxxx [b]xxx [c]xxx"},
      {u"xxx abcxxx bxxx cxxx", u"abc", u"xxx [a]bcxxx [b]xxx [c]xxx"},
  };

  for (auto& test_case : kTestCases) {
    const TokenizedString query(test_case.query);
    const TokenizedString text(test_case.text);

    AcronymMatcher am(query, text);
    am.CalculateRelevance();
    EXPECT_EQ(test_case.expect, MatchHit(test_case.text, am));
  }
}

}  // namespace ash::string_matching
