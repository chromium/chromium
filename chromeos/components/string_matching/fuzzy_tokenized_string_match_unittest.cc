// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/string_matching/fuzzy_tokenized_string_match.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/string_matching/sequence_matcher.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace string_matching {

namespace {

constexpr float kEps = 1e-5f;

// Default parameters.
constexpr bool kUseWeightedRatio = false;
constexpr bool kUseEditDistance = false;
constexpr double kPartialMatchPenaltyRate = 0.9;

void ExpectAllNearlyEqual(const std::vector<double>& scores,
                          double epsilon = kEps) {
  for (size_t i = 1; i < scores.size(); ++i) {
    EXPECT_NEAR(scores[0], scores[i], epsilon);
  }
}

void ExpectIncreasing(const std::vector<double>& scores,
                      int start_index,
                      int end_index,
                      double epsilon = 0.0) {
  if (!end_index) {
    end_index = scores.size();
  }

  for (int i = start_index; i < end_index - 1; ++i) {
    EXPECT_LT(scores[i], scores[i + 1] + epsilon);
  }
}

// Check that values between `scores[start_index]` (inclusive) and
// `scores[end_index]` (exclusive) are mostly increasing. Allow wiggle room of
// `epsilon` in the definition of "increasing".
//
// Why this is useful:
//
// When the text is long, and depending on the exact input params to
// FuzzyTokenizedStringMatch, we can get variable and sometimes unexpected
// sequences of relevance scores. Scores may or may not be influenced by, e.g.:
// (1) space characters and (2) partial tokens.
void ExpectMostlyIncreasing(const std::vector<double>& scores,
                            double epsilon,
                            int start_index = 0,
                            int end_index = 0) {
  ExpectIncreasing(scores, start_index, end_index, epsilon);
}

// Check that values between `scores[start_index]` (inclusive) and
// `scores[end_index]` (exclusive) are strictly increasing.
void ExpectStrictlyIncreasing(const std::vector<double>& scores,
                              int start_index = 0,
                              int end_index = 0) {
  ExpectIncreasing(scores, start_index, end_index, /*epsilon*/ 0.0);
}

double CalculateRelevance(const std::u16string& query,
                          const std::u16string& text) {
  FuzzyTokenizedStringMatch match;
  return match.Relevance(TokenizedString(query), TokenizedString(text),
                         kUseWeightedRatio, kUseEditDistance,
                         kPartialMatchPenaltyRate);
}

// Return a string formatted for displaying query-text relevance score details.
// Allow specification of query-first/text-first ordering because different
// series of tests will favor different visual display.
std::string FormatRelevanceResult(const std::u16string& query,
                                  const std::u16string& text,
                                  double relevance,
                                  bool query_first = true) {
  if (query_first) {
    return base::StringPrintf("query: %s, text: %s, relevance: %f",
                              base::UTF16ToUTF8(query).data(),
                              base::UTF16ToUTF8(text).data(), relevance);
  } else {
    return base::StringPrintf("text: %s, query: %s, relevance: %f",
                              base::UTF16ToUTF8(text).data(),
                              base::UTF16ToUTF8(query).data(), relevance);
  }
}

}  // namespace

class FuzzyTokenizedStringMatchTest : public testing::Test {};

/**********************************************************************
 * Benchmarking tests                                                 *
 **********************************************************************/
// The tests in this section perform benchmarking on the quality of
// relevance scores. See the README for details. These tests are divided into
// two sections:
//
//   1) Abstract test cases - which illustrate our intended string matching
//   principles generically.
//   2) Non-abstract test cases - which use real-world examples to:
//      a) support the principles in (1).
//      b) document bugs.
//
//  Both sections will variously cover the following dimensions:
//
// - Special characters:
//   - Upper/lower case
//   - Numerals
//   - Punctuation
// - Typos and misspellings
// - Full vs. partial matches
// - Prefix-related logic
// - Single- vs. multi-token texts
// - Single- vs. multi-token queries
// - Single vs. multiple possible matches
// - Duplicate tokens
//
// Some test cases cover an intersection of multiple dimensions.
//
// Future benchmarking work may cover:
//
// - Special token delimiters
//   - Camel case
//   - Non-whitespace token delimiters

/**********************************************************************
 * Benchmarking section 1 - Abstract test cases                       *
 **********************************************************************/
// TODO(crbug.com/1336160): Expand abstract benchmarking tests.

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkCaseInsensitivity) {
  std::u16string text = u"abcde";
  std::vector<std::u16string> queries = {u"abcde", u"Abcde", u"aBcDe",
                                         u"ABCDE"};
  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  ExpectAllNearlyEqual(scores);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkNumerals) {
  // TODO(crbug.com/1336160): This test is a placeholder to remember to
  // consider numerals, and should be refined/removed/expanded as appropriate
  // later.
  std::u16string text = u"abc123";
  std::u16string query = u"abc 123";
  const double relevance = CalculateRelevance(query, text);
  VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                   /*query_first*/ false);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkPunctuation) {
  std::u16string text = u"abcde'fg";
  std::vector<std::u16string> queries = {u"abcde'fg", u"abcdefg"};
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  // TODO(crbug.com/1336160): Enforce/check that scores are close, after this
  // behavior is implemented.
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkCamelCase) {
  std::u16string text = u"AbcdeFghIj";
  std::vector<std::u16string> queries = {u"AbcdeFghIj", u"abcde fgh ij",
                                         u"abcdefghij", u"abcde fghij"};
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  // TODO(crbug.com/1336160): Enforce/check that scores are close, after this
  // behavior is implemented.
}

/**********************************************************************
 * Benchmarking section 2 - Non-abstract test cases                   *
 **********************************************************************/

// TODO(crbug.com/1288662): Make matching less permissive where the strings
// are short and the matching is multi-block (e.g. "chat" vs "caret").
TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkAppsShortNamesMultiBlock) {
  std::u16string query1 = u"chat";
  std::vector<std::u16string> texts1 = {u"Chat", u"Caret", u"Calendar",
                                        u"Camera", u"Chrome"};
  for (const auto& text : texts1) {
    const double relevance = CalculateRelevance(query1, text);
    VLOG(1) << FormatRelevanceResult(query1, text, relevance,
                                     /*query_first*/ true);
  }

  std::u16string query2 = u"ses";
  std::vector<std::u16string> texts2 = {u"Sheets", u"Slides"};
  for (const auto& text : texts2) {
    const double relevance = CalculateRelevance(query2, text);
    VLOG(1) << FormatRelevanceResult(query2, text, relevance,
                                     /*query_first*/ true);
  }
}

// TODO(crbug.com/1332374): Reduce permissivity currently afforded by block
// matching algorithm.
TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkAssistantAndGamesWeather) {
  std::u16string query = u"weather";
  std::vector<std::u16string> texts = {u"weather", u"War Thunder",
                                       u"Man Eater"};
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkChromeMultiBlock) {
  std::u16string text = u"Chrome";
  // N.B. "c", "ch", "chr", are not multiblock matches to "Chrome", but are
  // included for comparison.
  std::vector<std::u16string> queries = {u"c",   u"ch",  u"chr", u"co",  u"com",
                                         u"cho", u"che", u"cr",  u"cro", u"cre",
                                         u"ho",  u"hom", u"hoe", u"roe"};
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkChromePrefix) {
  std::vector<std::u16string> texts = {u"Chrome", u"Google Chrome"};
  std::vector<std::u16string> queries = {u"c",    u"ch",    u"chr",
                                         u"chro", u"chrom", u"chrome"};
  for (const auto& text : texts) {
    std::vector<double> scores;

    for (const auto& query : queries) {
      const double relevance = CalculateRelevance(query, text);
      scores.push_back(relevance);
      VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                       /*query_first*/ false);
    }
    ExpectStrictlyIncreasing(scores);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkChromeTransposition) {
  std::u16string text = u"Chrome";
  // Single character-pair transpositions.
  std::vector<std::u16string> queries = {u"chrome", u"hcrome", u"crhome",
                                         u"chorme", u"chroem"};
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkGamesArk) {
  std::u16string query = u"ark";
  // Intended string matching guidelines for these cases:
  // - Favor full token matches over partial token matches.
  // - Favor prefix matches over non-prefix matches.
  // - Do not penalize for unmatched lengths of text.
  std::vector<std::u16string> texts = {u"PixARK", u"LOST ARK",
                                       u"ARK: Survival Evolved"};
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
  // TODO(crbug.com/1342440): Add expectation that scores are strictly
  // increasing, once the implementation achieves this.
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkGamesAssassinsCreed) {
  std::u16string text = {u"Assassin's Creed"};
  // Variations on punctuation and spelling
  std::vector<std::u16string> queries = {
      u"assassin", u"assassin'", u"assassin's", u"assassins",
      u"assasin",  u"assasin's", u"assasins"};
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkKeyboardShortcutsScreenshot) {
  std::u16string query = u"screenshot";
  std::vector<std::u16string> texts = {u"Take fullscreen screenshot",
                                       u"Take partial screenshot/recording",
                                       u"Take screenshot/recording"};
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
}

// TODO(crbug.com/1323910): Improve word order flexibility.
TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkKeyboardShortcutsDesk) {
  std::u16string text = u"Create a new desk";
  std::u16string text_lower = u"create a new desk";
  std::vector<std::u16string> queries_strict_prefix = {u"crea",
                                                       u"creat",
                                                       u"create",
                                                       u"create ",
                                                       u"create a",
                                                       u"create a ",
                                                       u"create a n",
                                                       u"create a ne",
                                                       u"create a new",
                                                       u"create a new ",
                                                       u"create a new d",
                                                       u"create a new de",
                                                       u"create a new des",
                                                       u"create a new desk"};
  std::vector<std::u16string> queries_missing_words = {
      u"create a d",   u"create a de",   u"create a des",   u"create a desk",
      u"create d",     u"create de",     u"create des",     u"create desk",
      u"create n",     u"create ne",     u"create new",     u"create new ",
      u"create new d", u"create new de", u"create new des", u"create new desk",
      u"new ",         u"new d",         u"new de",         u"new des",
      u"new desk",     u"desk"};

  std::vector<double> scores;
  for (const auto& query : queries_strict_prefix) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  // Allow a flexible (rather than strict) increase in scores.
  ExpectMostlyIncreasing(scores, /*epsilon*/ 0.005);

  for (const auto& query : queries_missing_words) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
}

// TODO(crbug.com/1327090): Reduce/remove penalties for unmatched text.
TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkKeyboardShortcutsEmojiPicker) {
  std::u16string text = u"Open Emoji picker";
  std::vector<std::u16string> queries = {u"emoj", u"emoji", u"emoji ",
                                         u"emoji p", u"emoji pi"};
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
}

// TODO(crbug.com/1325088): Improve word order flexibility.
TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkKeyboardShortcutsIncognitoWindow) {
  std::u16string query = u"Open a new window in incognito mode";
  std::vector<std::u16string> texts = {u"new window incognito",
                                       u"new incognito window"};
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
}

// TODO(crbug.com/1336160): Introduce some kind of agnosticism to text length.
TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkSettingsPreferences) {
  std::u16string query = u"preferences";
  std::vector<std::u16string> texts = {
      u"Android preferences", u"Caption preferences", u"System preferences",
      u"External storage preferences"};
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
}

/**********************************************************************
 * Per-method tests                                                   *
 **********************************************************************/
// The tests in this section check the functionality of individual class
// methods (as opposed to the score benchmarking performed above).

// TODO(crbug.com/1336160): update the tests once params are consolidated.
TEST_F(FuzzyTokenizedStringMatchTest, PartialRatioTest) {
  FuzzyTokenizedStringMatch match;
  EXPECT_NEAR(match.PartialRatio(u"abcde", u"ababcXXXbcdeY",
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.6, 0.01);
  EXPECT_NEAR(match.PartialRatio(u"big string", u"strength",
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.71, 0.01);
  EXPECT_EQ(
      match.PartialRatio(u"abc", u"", kPartialMatchPenaltyRate, false, 0.0), 0);
  EXPECT_NEAR(match.PartialRatio(u"different in order", u"order text",
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.67, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSetRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"order different in");
    std::u16string text(u"text order");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
  }
  {
    std::u16string query(u"short text");
    std::u16string text(u"this text is really really really long");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.57, 0.01);
  }
  {
    std::u16string query(u"common string");
    std::u16string text(u"nothing is shared");
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text), true,
                            kPartialMatchPenaltyRate, false, 0.0),
        0.38, 0.01);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.33, 0.01);
  }
  {
    std::u16string query(u"token shared token same shared same");
    std::u16string text(u"token shared token text text long");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.83, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSortRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"order different in");
    std::u16string text(u"text order");
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false, 0.0),
        0.36, 0.01);
  }
  {
    std::u16string query(u"short text");
    std::u16string text(u"this text is really really really long");
    EXPECT_EQ(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false, 0.0),
        0.5 * std::pow(0.9, 1));
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false, 0.0),
        0.33, 0.01);
  }
  {
    std::u16string query(u"common string");
    std::u16string text(u"nothing is shared");
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false, 0.0),
        0.38, 0.01);
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false, 0.0),
        0.33, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, WeightedRatio) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.81, 0.01);
  }
  {
    std::u16string query(u"final fantasy");
    std::u16string text(u"finalfantasy");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.96, 0.01);
  }
  {
    std::u16string query(u"short text!!!");
    std::u16string text(
        u"this sentence is much much much much much longer "
        u"than the text before");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.49, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, PrefixMatcherTest) {
  {
    std::u16string query(u"clas");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.94, 0.01);
  }
  {
    std::u16string query(u"clash clan");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"c o c");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.84, 0.01);
  }
  {
    std::u16string query(u"wifi");
    std::u16string text(u"wi-fi");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.91, 0.01);
  }
  {
    std::u16string query(u"clam");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"rp");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"remove play");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"google play");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.99, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest1) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_LT(match.Relevance(TokenizedString(query), TokenizedString(text),
                              true, false, kPartialMatchPenaltyRate, 0.0),
              0.4);
  }
  {
    std::u16string query(u"CC");
    std::u16string text(u"Clash Of Clan");
    EXPECT_LT(match.Relevance(TokenizedString(query), TokenizedString(text),
                              true, false, kPartialMatchPenaltyRate, 0.0),
              0.25);
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_GT(match.Relevance(TokenizedString(query), TokenizedString(text),
                              true, false, kPartialMatchPenaltyRate, 0.0),
              0.4);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest2) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_LT(match.Relevance(TokenizedString(query), TokenizedString(text),
                              true, false, kPartialMatchPenaltyRate, 0.0),
              0.5);
  }
  {
    std::u16string query(u"CC");
    std::u16string text(u"Clash Of Clan");
    EXPECT_LT(match.Relevance(TokenizedString(query), TokenizedString(text),
                              true, false, kPartialMatchPenaltyRate),
              0.25);
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_LT(match.Relevance(TokenizedString(query), TokenizedString(text),
                              true, false, kPartialMatchPenaltyRate, 0.0),
              0.5);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, OtherParamTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(u"anonymous");
  std::u16string text(u"famous");
  const double relevance =
      match.Relevance(TokenizedString(query), TokenizedString(text), false,
                      true, kPartialMatchPenaltyRate, 0.0);

  EXPECT_LT(relevance, 0.35);
  EXPECT_NEAR(relevance, 0.33 / 2, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, ExactTextMatchTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(u"yat");
  std::u16string text(u"YaT");
  const double relevance =
      match.Relevance(TokenizedString(query), TokenizedString(text), false,
                      true, kPartialMatchPenaltyRate, 0.0);
  EXPECT_GT(relevance, 0.35);
  EXPECT_DOUBLE_EQ(relevance, 1.0);
  EXPECT_EQ(match.hits().size(), 1u);
  EXPECT_EQ(match.hits()[0].start(), 0u);
  EXPECT_EQ(match.hits()[0].end(), 3u);
}

}  // namespace string_matching
}  // namespace chromeos
