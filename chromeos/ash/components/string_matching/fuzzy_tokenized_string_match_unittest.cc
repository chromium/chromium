// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

// An upper limit for the purposes of catching regressions. Not for benchmarking
// of typical expected time performance, which is much faster.
constexpr base::TimeDelta kCalculationTimeUpperBound = base::Milliseconds(20);

constexpr double kEps = 1e-5;
constexpr double kCompleteMatchScore = 1.0;
constexpr double kCompleteMismatchScore = 0.0;

// Default parameters.
constexpr bool kUseWeightedRatio = false;
constexpr bool kStripDiacritics = false;

void ExpectAllNearlyEqualTo(const std::vector<double>& scores,
                            double target_score,
                            double abs_error = kEps) {
  for (const auto score : scores) {
    EXPECT_NEAR(target_score, score, abs_error);
  }
}

void ExpectAllNearlyEqual(const std::vector<double>& scores,
                          double abs_error = kEps) {
  DCHECK(scores.size() > 0);
  ExpectAllNearlyEqualTo(scores, scores[0], abs_error);
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
                         kUseWeightedRatio);
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

// Returns a string of |text| marked with the hits using block
// bracket. e.g. text= "Text", hits = [{0,1}], returns "[T]ext".
//
// TODO(crbug.com/1336160): Consider defining it as a |test_util| function as it
// has been used for several unit tests.
std::u16string MatchHit(const std::u16string& text,
                        const FuzzyTokenizedStringMatch::Hits& hits) {
  std::u16string marked = text;

  for (const gfx::Range& hit : base::Reversed(hits)) {
    marked.insert(hit.end(), 1, u']');
    marked.insert(hit.start(), 1, u'[');
  }

  return marked;
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
  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  ExpectAllNearlyEqual(scores, /*abs_error*/ 0.05);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkCompleteMatchSingleToken) {
  // A complete match between text and query should always score very well. Test
  // score calculations for pairs of identical strings, for various lengths of
  // string.
  std::u16string full_string = u"abcdefgh";

  for (size_t i = 1; i < full_string.size(); ++i) {
    // N.B. The created `substring` is compared to itself, not to `full_string`.
    std::u16string substring = full_string.substr(0, i);
    const double relevance = CalculateRelevance(substring, substring);
    VLOG(1) << FormatRelevanceResult(substring, substring, relevance,
                                     /*query_first*/ false);
    EXPECT_NEAR(relevance, kCompleteMatchScore, kEps);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkCompleteMatchMultiToken) {
  // A complete match between text and query should always score very well. Test
  // score calculations for pairs of identical strings, for various lengths of
  // string.
  std::u16string full_string = u"ab cdefgh ijk";

  for (size_t i = 1; i < full_string.size(); ++i) {
    // N.B. The created `substring` is compared to itself, not to `full_string`.
    std::u16string substring = full_string.substr(0, i);
    const double relevance = CalculateRelevance(substring, substring);
    VLOG(1) << FormatRelevanceResult(substring, substring, relevance,
                                     /*query_first*/ false);
    EXPECT_NEAR(relevance, kCompleteMatchScore, kEps);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkCompleteNonMatchSingleToken) {
  std::u16string full_text = u"abcdefgh";
  std::u16string full_query = u"stuvwxyz";
  ASSERT_EQ(full_text.size(), full_query.size());

  for (size_t i = 1; i < full_text.size(); ++i) {
    std::u16string text = full_text.substr(0, i);
    std::u16string query = full_query.substr(0, i);
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    EXPECT_NEAR(relevance, kCompleteMismatchScore, kEps);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkCompleteNonMatchMultiToken) {
  std::u16string full_text = u"ab cdefgh ijk";
  std::u16string full_query = u"pqrstu vw xyz";
  ASSERT_EQ(full_text.size(), full_query.size());

  for (size_t i = 1; i < full_text.size(); ++i) {
    std::u16string text = full_text.substr(0, i);
    std::u16string query = full_query.substr(0, i);
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    EXPECT_NEAR(relevance, kCompleteMismatchScore, kEps);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkVariedLengthUnmatchedTextSingleToken) {
  std::u16string full_text = u"abcdefghijklmnop";
  const size_t shortest_text_length = 9;
  const size_t longest_text_length = full_text.size();

  std::vector<std::u16string> queries = {u"abc", u"bcd", u"cde", u"def"};

  for (const auto& query : queries) {
    // For a fixed query, where the query is a full match to some portion of the
    // text, the relevance score should not be influenced by the
    // amounts of any remaining unmatched portions of text ("text-length
    // agnosticism").
    std::vector<double> scores;
    for (size_t i = shortest_text_length; i < longest_text_length; ++i) {
      std::u16string text = full_text.substr(0, i);
      const double relevance = CalculateRelevance(query, text);
      scores.push_back(relevance);
      VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                       /*query_first*/ true);
    }
    ExpectAllNearlyEqual(scores);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkVariedLengthUnmatchedTextMultiToken) {
  std::vector<std::u16string> texts = {u"ab cdefgh", u"ab cdefgh ijk",
                                       u"ab cdefgh ijk lmno",
                                       u"ab cdefgh ijk lmno pqrst"};

  std::vector<std::u16string> queries = {
      u"ab c",  // strict prefix of text
      u"cdef",  // token prefix of text
      u"defg",  // token in-fix of text
      u"efgh"   // token suffix of text
  };

  for (const auto& query : queries) {
    // For a fixed query, where the query is a full match to some portion of the
    // text, the relevance score should not be influenced by the
    // amounts of any remaining unmatched portions of text ("text-length
    // agnosticism").
    std::vector<double> scores;
    for (const auto& text : texts) {
      const double relevance = CalculateRelevance(query, text);
      VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                       /*query_first*/ true);
      scores.push_back(relevance);
    }
    ExpectAllNearlyEqual(scores);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkVariedLengthUnmatchedQuerySingleToken) {
  // This test contains the same strings as
  // BenchmarkVariedLengthUnmatchedTextSingleToken, with the
  // roles of text and query swapped.
  std::vector<std::u16string> texts = {u"abc", u"bcd", u"cde", u"def"};

  std::u16string full_query = u"abcdefghijklmnop";
  const size_t shortest_query_length = 6;
  const size_t longest_query_length = full_query.size();

  for (const auto& text : texts) {
    // Compare a fixed text against a number of queries of differing length,
    // where the query is a substring of the text but the query has leftover
    // unmatched characters.
    for (size_t i = shortest_query_length; i < longest_query_length; ++i) {
      std::u16string query = full_query.substr(0, i);
      const double relevance = CalculateRelevance(query, text);
      VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                       /*query_first*/ false);
    }
    // TODO(crbug.com/1336160): Decide on how to handle unmatched portions of
    // query.
  }
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkVariedLengthUnmatchedQueryMultiToken) {
  // This test contains the same strings as
  // BenchmarkVariedLengthUnmatchedTextMultiToken, with the
  // roles of text and query swapped.
  std::vector<std::u16string> texts = {
      u"ab c",  // strict prefix of query
      u"cdef",  // token prefix of query
      u"defg",  // token in-fix of query
      u"efgh"   // token suffix of query
  };

  std::vector<std::u16string> queries = {u"ab cdefgh", u"ab cdefgh ijk",
                                         u"ab cdefgh ijk lmno",
                                         u"ab cdefgh ijk lmno pqrst"};

  for (const auto& text : texts) {
    // Compare a fixed text against a number of queries of differing length,
    // where the query is a substring of the text but the query has leftover
    // unmathced characters.
    for (const auto& query : queries) {
      const double relevance = CalculateRelevance(query, text);
      VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                       /*query_first*/ false);
    }
    // TODO(crbug.com/1336160): Decide on how to handle unmatched portions of
    // query.
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkTokenOrderVariation) {
  // Case: Two words.
  std::u16string text_two_words = u"abc def";
  std::vector<std::u16string> queries_two_words = {u"abc def", u"def abc"};
  std::vector<double> scores_query_two_words;
  for (const auto& query : queries_two_words) {
    const double relevance = CalculateRelevance(query, text_two_words);
    scores_query_two_words.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text_two_words, relevance,
                                     /*query_first*/ false);
  }
  ExpectAllNearlyEqual(scores_query_two_words, /*abs_error*/ 0.1);

  // Case: Three words.
  std::u16string text_three_words = u"abc def ghi";
  std::vector<std::u16string> queries_three_words = {
      u"abc def ghi", u"abc ghi def", u"def abc ghi",
      u"def ghi abc", u"ghi abc def", u"ghi def abc"};
  std::vector<double> scores_query_three_words;
  for (const auto& query : queries_three_words) {
    const double relevance = CalculateRelevance(query, text_three_words);
    scores_query_three_words.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text_three_words, relevance,
                                     /*query_first*/ false);
  }
  ExpectAllNearlyEqual(scores_query_three_words, /*abs_error*/ 0.05);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkTokensPresentInTextButNotQuery) {
  std::u16string text = u"abc def ghi";

  // Case: multi-token text, single-token query.
  std::vector<std::u16string> queries_single_token = {u"abc", u"def", u"ghi"};
  std::vector<double> scores_query_single_token;
  for (const auto& query : queries_single_token) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores_query_single_token.push_back(relevance);
  }
  // There is a score boost in prefix matcher when a matched token is the first
  // token of both text and query.
  scores_query_single_token[0] -= 0.125;
  ExpectAllNearlyEqual(scores_query_single_token, /*abs_error*/ 0.01);

  // Case: multi-token text, two-token query.
  std::vector<std::u16string> queries_two_tokens = {u"abc def", u"abc ghi",
                                                    u"def ghi"};
  std::vector<double> scores_query_two_tokens;
  for (const auto& query : queries_two_tokens) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores_query_two_tokens.push_back(relevance);
  }
  ExpectAllNearlyEqual(scores_query_two_tokens, /*abs_error*/ 0.1);

  // TODO(crbug.com/1336160): [Later] Consider a score boost for when a matched
  // token is the first token of both text and query.
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkTokensPresentInQueryButNotText) {
  // N.B. This test contains the same texts and queries as in
  // BenchmarkTokensPresentInTextButNotQuery, with the roles of text and query
  // swapped. The expected/desired behavior is quite different, though.

  // TODO(crbug.com/1336160): Decide how to handle unmatched query tokens. When
  // a text is very short and a query very long, the text and query are probably
  // a poor match. However, when both the text and query are long, (e.g. file
  // names, keyboard shortcuts), it seems useful to allow for some degree of
  // unmatched query tokens.

  // TODO(crbug.com/1336160): [Later] Consider a score boost for when a matched
  // token is the first token of both text and query.

  std::u16string query = u"abc def ghi";

  // Case: single-token text.
  std::vector<std::u16string> text_single_token = {u"abc", u"def", u"ghi"};
  std::vector<double> scores_text_single_token;
  for (const auto& text : text_single_token) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
    scores_text_single_token.push_back(relevance);
  }
  ExpectAllNearlyEqual(scores_text_single_token);

  // Case: two-token text.
  std::vector<std::u16string> text_two_tokens = {u"abc def", u"abc ghi",
                                                 u"def ghi"};
  std::vector<double> scores_text_two_tokens;
  for (const auto& text : text_two_tokens) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
    scores_text_two_tokens.push_back(relevance);
  }
  ExpectAllNearlyEqual(scores_text_two_tokens, /*abs_error*/ 0.1);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkMultipleQueryTokensMapToOneTextToken) {
  // This test contains the same strings as
  // BenchmarkMultipleTextTokensMapToOneQueryToken, with the
  // roles of text and query swapped.
  //
  // N.B. With the upcoming fuzzy matching v2, supporting flexibility around
  // whitespace in this way may turn out to be too onerous. Consider this a
  // stretch goal.

  // Case 1: single-token text.
  std::u16string text1 = u"abcdef";

  // A query containing extra whitespace and which is otherwise a good match to
  // the text, should score highly. The idea is to treat a small amount of extra
  // whitespace as a spelling/grammar mistake.
  //
  // Here `_ordered` and `_shuffled` refer to the relative positions of the two
  // query tokens which map onto the text token "abcdef".
  std::vector<std::u16string> queries1_ordered = {u"ab cdef", u"abc def"};
  std::vector<double> scores1_ordered;
  for (const auto& query : queries1_ordered) {
    const double relevance = CalculateRelevance(query, text1);
    VLOG(1) << FormatRelevanceResult(query, text1, relevance,
                                     /*query_first*/ false);
    scores1_ordered.push_back(relevance);
  }
  ExpectAllNearlyEqualTo(scores1_ordered, 0.9, /*abs_error*/ 0.1);

  // Token-merging should only be considered for consecutive tokens, in line
  // with the spelling/grammar mistake philosophy.
  std::vector<std::u16string> queries1_shuffled = {u"cdef ab", u"def abc"};
  std::vector<double> scores1_shuffled;
  for (const auto& query : queries1_shuffled) {
    const double relevance = CalculateRelevance(query, text1);
    VLOG(1) << FormatRelevanceResult(query, text1, relevance,
                                     /*query_first*/ false);
    scores1_shuffled.push_back(relevance);
  }
  ExpectAllNearlyEqualTo(scores1_shuffled, 0.54, /*abs_error*/ 0.1);

  // Case 2: multi-token text.
  std::u16string text2 = u"abcdef ghi";

  std::vector<std::u16string> queries2_ordered = {
      u"ab cdef ghi", u"abc def ghi", u"ghi ab cdef", u"ghi abc def"};
  std::vector<double> scores2_ordered;
  for (const auto& query : queries2_ordered) {
    const double relevance = CalculateRelevance(query, text2);
    VLOG(1) << FormatRelevanceResult(query, text2, relevance,
                                     /*query_first*/ false);
    scores2_ordered.push_back(relevance);
  }
  EXPECT_GT(scores2_ordered[0], 0.9);
  EXPECT_GT(scores2_ordered[1], 0.9);
  // TODO(crbug.com/1336160): Support token-order flexibility to allow the
  // following (ish):
  //
  //   ExpectAllNearlyEqualTo(scores2_ordered, 0.9, /*abs_error*/ 0.1);

  std::vector<std::u16string> queries2_shuffled = {
      u"cdef ab ghi", u"cdef ghi ab", u"def abc ghi", u"ghi def abc"};
  std::vector<double> scores2_shuffled;
  for (const auto& query : queries2_shuffled) {
    const double relevance = CalculateRelevance(query, text2);
    VLOG(1) << FormatRelevanceResult(query, text2, relevance,
                                     /*query_first*/ false);
    scores2_shuffled.push_back(relevance);
  }
  ExpectAllNearlyEqualTo(scores2_shuffled, 0.6, /*abs_error*/ 0.25);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkMultipleTextTokensMapToOneQueryToken) {
  // This test contains the same strings as
  // BenchmarkMultipleQueryTokensMapToOneTextToken, with the
  // roles of text and query swapped.
  //
  // N.B. With the upcoming fuzzy matching v2, supporting flexibility around
  // whitespace in this way may turn out to be too onerous. Consider this a
  // stretch goal.

  // Case 1: single-token query.
  std::u16string query1 = u"abcdef";

  // A text containing extra whitespace and which is otherwise a good match to
  // the query, should score highly. The idea is to treat a small amount of
  // extra whitespace as a spelling/grammar mistake.
  //
  // Here `_ordered` and `_shuffled` refer to the relative positions of the two
  // text tokens which map onto the query token "abcdef".
  std::vector<std::u16string> texts1_ordered = {u"ab cdef", u"abc def"};
  std::vector<double> scores1_ordered;
  for (const auto& text : texts1_ordered) {
    const double relevance = CalculateRelevance(query1, text);
    VLOG(1) << FormatRelevanceResult(query1, text, relevance,
                                     /*query_first*/ true);
    scores1_ordered.push_back(relevance);
  }
  ExpectAllNearlyEqualTo(scores1_ordered, 0.9, /*abs_error*/ 0.1);

  // Token-merging should only be considered for consecutive tokens, in line
  // with the spelling/grammar mistake philosophy.
  std::vector<std::u16string> texts1_shuffled = {u"cdef ab", u"def abc"};
  std::vector<double> scores1_shuffled;
  for (const auto& text : texts1_shuffled) {
    const double relevance = CalculateRelevance(query1, text);
    VLOG(1) << FormatRelevanceResult(query1, text, relevance,
                                     /*query_first*/ true);
    scores1_shuffled.push_back(relevance);
  }
  ExpectAllNearlyEqualTo(scores1_shuffled, 0.6, /*abs_error*/ 0.2);

  // Case 2: multi-token query.
  std::u16string query2 = u"abcdef ghi";

  std::vector<std::u16string> texts2_ordered = {u"ab cdef ghi", u"abc def ghi",
                                                u"ghi ab cdef", u"ghi abc def"};
  std::vector<double> scores2_ordered;
  for (const auto& text : texts2_ordered) {
    const double relevance = CalculateRelevance(query2, text);
    VLOG(1) << FormatRelevanceResult(query2, text, relevance,
                                     /*query_first*/ true);
    scores2_ordered.push_back(relevance);
  }
  EXPECT_GT(scores2_ordered[0], 0.9);
  EXPECT_GT(scores2_ordered[1], 0.9);
  // TODO(crbug.com/1336160): Support token-order flexibility to allow the
  // following (ish):
  //
  //   ExpectAllNearlyEqualTo(scores2_ordered, 0.9, /*abs_error*/ 0.1);

  std::vector<std::u16string> texts2_shuffled = {
      u"cdef ab ghi", u"cdef ghi ab", u"def abc ghi", u"ghi def abc"};
  std::vector<double> scores2_shuffled;
  for (const auto& text : texts2_shuffled) {
    const double relevance = CalculateRelevance(query2, text);
    VLOG(1) << FormatRelevanceResult(query2, text, relevance,
                                     /*query_first*/ true);
    scores2_shuffled.push_back(relevance);
  }
  ExpectAllNearlyEqualTo(scores2_shuffled, 0.6, /*abs_error*/ 0.25);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkPartialMatchPartialMismatch) {
  std::u16string text = u"abcdef";

  std::u16string query_full = u"abcdef";
  std::u16string query_prefix = u"abc";

  std::u16string query_suffix_mismatch = u"abcxyz";
  std::u16string query_prefix_mismatch = u"xyzdef";

  std::u16string query_suffix_is_text_prefix = u"xyzabc";
  std::u16string query_prefix_is_text_suffix = u"defxyz";

  const double relevance_query_full = CalculateRelevance(query_full, text);
  VLOG(1) << FormatRelevanceResult(query_full, text, relevance_query_full,
                                   /*query_first*/ false);

  const double relevance_query_prefix = CalculateRelevance(query_prefix, text);
  VLOG(1) << FormatRelevanceResult(query_prefix, text, relevance_query_prefix,
                                   /*query_first*/ false);

  const double relevance_query_suffix_mismatch =
      CalculateRelevance(query_suffix_mismatch, text);
  VLOG(1) << FormatRelevanceResult(query_suffix_mismatch, text,
                                   relevance_query_suffix_mismatch,
                                   /*query_first*/ false);

  const double relevance_query_prefix_mismatch =
      CalculateRelevance(query_prefix_mismatch, text);
  VLOG(1) << FormatRelevanceResult(query_prefix_mismatch, text,
                                   relevance_query_prefix_mismatch,
                                   /*query_first*/ false);

  const double relevance_query_suffix_is_text_prefix =
      CalculateRelevance(query_suffix_is_text_prefix, text);
  VLOG(1) << FormatRelevanceResult(query_suffix_is_text_prefix, text,
                                   relevance_query_suffix_is_text_prefix,
                                   /*query_first*/ false);

  const double relevance_query_prefix_is_text_suffix =
      CalculateRelevance(query_prefix_is_text_suffix, text);
  VLOG(1) << FormatRelevanceResult(query_prefix_is_text_suffix, text,
                                   relevance_query_prefix_is_text_suffix,
                                   /*query_first*/ false);

  CHECK_GT(relevance_query_full, relevance_query_prefix);

  CHECK_GT(relevance_query_prefix, relevance_query_suffix_mismatch);
  CHECK_GT(relevance_query_prefix, relevance_query_prefix_mismatch);
  CHECK_GT(relevance_query_prefix, relevance_query_suffix_is_text_prefix);
  CHECK_GT(relevance_query_prefix, relevance_query_prefix_is_text_suffix);

  // TODO(crbug.com/1336160): Consider the following if/when supported:
  //
  // CHECK_GT(relevance_query_suffix_mismatch, relevance_query_prefix_mismatch);
  // CHECK_GT(relevance_query_suffix_mismatch,
  // relevance_query_suffix_is_text_prefix);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixOfVaryingLengthSingleToken) {
  std::u16string text = u"abcdefg";
  std::u16string query_full = u"abcdefg";

  std::vector<double> scores;
  for (size_t i = 1; i < text.size(); ++i) {
    std::u16string query = text.substr(0, i);
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores.push_back(relevance);
  }

  // Intuitively, it seems desirable that, for a fixed text, the longer the
  // prefix match between text and a query, the greater the relevance score
  // should be. Check for this behavior here, but revisit the utility of this
  // later as it relates to text-length agnosticism.
  ExpectStrictlyIncreasing(scores);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixOfVaryingLengthMultiToken) {
  std::u16string text = u"ghijkl abcdef";
  std::vector<std::u16string> queries = {u"a",    u"ab",    u"abc",
                                         u"abcd", u"abcde", u"abcdef"};

  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores.push_back(relevance);
  }

  // Intuitively, it seems desirable that, for a fixed text, the longer the
  // prefix match between text and a query, the greater the relevance score
  // should be. Check for this behavior here, but revisit the utility of this
  // later as it relates to text-length agnosticism.

  ExpectStrictlyIncreasing(scores);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkPrefixVsNonPrefixSingleToken) {
  std::u16string text = u"abcdefg";
  std::vector<std::u16string> queries = {u"ab", u"bc", u"cd",
                                         u"de", u"ef", u"fg"};

  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores.push_back(relevance);
  }

  // Only query "ab" should benefit from a prefix-related scoring boost, and
  // the boost should be fairly high.
  const double prefix_score_boost = 0.3;

  EXPECT_GT(scores[0], scores[1] + prefix_score_boost);
  EXPECT_GT(scores[0], scores[2] + prefix_score_boost);
  EXPECT_GT(scores[0], scores[3] + prefix_score_boost);
  EXPECT_GT(scores[0], scores[4] + prefix_score_boost);
  EXPECT_GT(scores[0], scores[5] + prefix_score_boost);

  // TODO(crbug.com/1336160): Consider whether position of match should be
  // important when the match does not involve a prefix of the text. For example
  // here, is it meaningful or useful for query "bc" to be a better match than
  // query "cd"?
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixVsNonPrefixForTextAndQuerySingleToken) {
  std::u16string text = u"abcdefgh";

  // For a fixed length of matched portion between text and query, cover cases
  // where the matched portion is:

  std::vector<std::u16string> queries = {
      u"abcd",   // case 0: a text prefix, and query prefix.
      u"cdef",   // case 1: a text non-prefix, and query prefix.
      u"xabcd",  // case 2: a text prefix, and query non-prefix.
      u"xcdef"   // case 3: a text non-prefix, and query non-prefix.
  };

  // N.B. It isn't possible to have all the queries be the same length while
  // also creating matched portions of the same length. So there is some
  // necessary overlap of prefix scoring logic with other logic here.

  double relevance0 = CalculateRelevance(queries[0], text);
  VLOG(1) << FormatRelevanceResult(queries[0], text, relevance0,
                                   /*query_first*/ false);
  double relevance1 = CalculateRelevance(queries[1], text);
  VLOG(1) << FormatRelevanceResult(queries[1], text, relevance1,
                                   /*query_first*/ false);
  double relevance2 = CalculateRelevance(queries[2], text);
  VLOG(1) << FormatRelevanceResult(queries[2], text, relevance2,
                                   /*query_first*/ false);
  double relevance3 = CalculateRelevance(queries[3], text);
  VLOG(1) << FormatRelevanceResult(queries[3], text, relevance3,
                                   /*query_first*/ false);

  // Only case 0 should benefit from a prefix-related scoring boost, and
  // the boost should be fairly high.
  const double prefix_score_boost = 0.25;

  EXPECT_GT(relevance0, relevance1 + prefix_score_boost);
  EXPECT_GT(relevance0, relevance2 + prefix_score_boost);
  EXPECT_GT(relevance0, relevance3 + prefix_score_boost);

  // TODO(crbug.com/1336160): Consider whether cases 1, 2, and 3 should have
  // some expected ordering. e.g. choose between the following:
  //
  //   A prefix match (for either text or query) provides a boost i.e.:
  //   R0 > R1, R2 > R3
  //
  //   A prefix score boost is only given in case 0 i.e.:
  //   R0 > R1 ~= R2 ~= R3
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixVsContiguousBlockSingleToken) {
  std::u16string text = u"ababc";
  std::u16string query = u"abc";

  const double relevance = CalculateRelevance(query, text);
  VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                   /*query_first*/ false);

  // TODO(crbug.com/1336160): Consider having a strong opinion on whether prefix
  // matching or longest-contiguous-block matching should take precedence in
  // these cases.
  //
  // Prefix matching:
  //
  //   text:  ababc
  //   query: ab  c
  //
  // Longest-contiguous-block matching:
  //
  //   text:  ababc
  //   query:   abc
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixVsContiguousBlockMultiToken) {
  std::u16string text = u"abxyz wabc";
  std::u16string query = u"abc";

  const double relevance = CalculateRelevance(query, text);
  VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                   /*query_first*/ false);

  // See also BenchmarkPrefixVsContiguousBlockSingleToken above, for note on
  // prefix matching vs. longest contiguous block matching.
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkHitsMatchHighestMatchingAlgorithm) {
  FuzzyTokenizedStringMatch match;

  struct {
    const std::u16string text;
    const std::u16string query;
    const std::u16string expect;
    const bool use_acronym_matcher;
  } kTestCases[] = {
      // Prefix Matcher is favored over Sequence Matcher.
      {u"xyzabc abcdef", u"abc", u"xyzabc [abc]def",
       /*use_acronym_matcher=*/true},
      {u"xyzabc abcdef", u"abc", u"xyzabc [abc]def",
       /*use_acronym_matcher=*/false},
      // Acronym Matcher is favored over Sequence Matcher.
      {u"dabc axyz bxyz cxzy", u"abc", u"dabc [a]xyz [b]xyz [c]xzy",
       /*use_acronym_matcher=*/true},
      {u"dabc axyz bxyz cxzy", u"abc", u"d[abc] axyz bxyz cxzy",
       /*use_acronym_matcher=*/false},
      // Prefix Matcher at first token is favored over Acronym Matcher.
      {u"abcxyz bxyz cxzy", u"abc", u"[abc]xyz bxyz cxzy",
       /*use_acronym_matcher=*/true},
      {u"abcxyz bxyz cxzy", u"abc", u"[abc]xyz bxyz cxzy",
       /*use_acronym_matcher=*/false},
      // Acronym Matcher is favored over Prefix Matcher at non-first token.
      {u"def abcxyz bxyz cxzy", u"abc", u"def [a]bcxyz [b]xyz [c]xzy",
       /*use_acronym_matcher=*/true},
      {u"def abcxyz bxyz cxzy", u"abc", u"def [abc]xyz bxyz cxzy",
       /*use_acronym_matcher=*/false},
  };

  for (auto& test_case : kTestCases) {
    const TokenizedString query(test_case.query);
    const TokenizedString text(test_case.text);

    double relevance =
        match.Relevance(query, text, kUseWeightedRatio, kStripDiacritics,
                        /*use_acronym_matcher=*/test_case.use_acronym_matcher);

    VLOG(1) << FormatRelevanceResult(test_case.query, test_case.text, relevance,
                                     /*query_first*/ false);
    EXPECT_EQ(test_case.expect, MatchHit(test_case.text, match.hits()));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixVariedWordOrderMultiToken) {
  std::u16string text = u"abcd efgh ijkl";
  std::vector<std::u16string> queries = {u"abcd ef", u"abcd ij", u"efgh ab",
                                         u"efgh ij", u"ijkl ab", u"ijkl ef"};
  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores.push_back(relevance);
  }
  // There is a score boost in prefix matcher when a matched token is the first
  // token of both text and query.
  scores[0] -= 0.04;
  scores[1] -= 0.04;
  ExpectAllNearlyEqual(scores, /*abs_error*/ 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixMultipleSameLengthMatchesMultiToken) {
  FuzzyTokenizedStringMatch match;
  std::u16string text = u"abcde abfgh abijk";
  std::u16string query = u"ab";

  const double relevance = match.Relevance(
      TokenizedString(query), TokenizedString(text), kUseWeightedRatio);
  VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                   /*query_first*/ false);

  // Where multiple same-length token prefix matches are possible, prioritize
  // the earliest match.
  EXPECT_EQ(match.hits().size(), 1u);
  EXPECT_EQ(match.hits()[0].start(), 0u);
  EXPECT_EQ(match.hits()[0].end(), 2u);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkPrefixMultiplePossibleVariedLengthMatchesMultiToken) {
  FuzzyTokenizedStringMatch match;
  std::u16string text = u"abxyz abcwv abcdt";
  std::u16string query = u"abcde";

  const double relevance = match.Relevance(
      TokenizedString(query), TokenizedString(text), kUseWeightedRatio);
  VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                   /*query_first*/ false);

  // Expect a single hit, for the "abcd" of "abcdt".
  EXPECT_EQ(match.hits().size(), 1u);
  EXPECT_EQ(match.hits()[0].start(), 12u);
  EXPECT_EQ(match.hits()[0].end(), 16u);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkStressTestLongText) {
  // Same as BenchmarkStressTestLongQuery, with the roles of text and query
  // reversed.

  std::u16string text(300, 'a');

  std::u16string query_high_match(25, 'a');
  std::u16string query_low_match(u"bbbbbcccccbbbbbaaaaabbbbbcccccbbbbb");
  std::u16string query_no_match(25, 'b');
  std::vector<std::u16string> queries = {query_high_match, query_low_match,
                                         query_no_match};

  for (const auto& query : queries) {
    base::Time start_time = base::Time::NowFromSystemTime();
    const double relevance = CalculateRelevance(query, text);
    base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

    EXPECT_LT(elapsed_time, kCalculationTimeUpperBound);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    VLOG(1) << "Elapsed time (ms): " << elapsed_time.InMillisecondsF();
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkStressTestLongQuery) {
  // Same as BenchmarkStressTestLongText, with the roles of text and query
  // reversed.
  std::u16string query(300, 'a');

  std::u16string text_high_match(25, 'a');
  std::u16string text_low_match(u"bbbbbcccccbbbbbaaaaabbbbbcccccbbbbb");
  std::u16string text_no_match(25, 'b');
  std::vector<std::u16string> texts = {text_high_match, text_low_match,
                                       text_no_match};

  for (const auto& text : texts) {
    base::Time start_time = base::Time::NowFromSystemTime();
    const double relevance = CalculateRelevance(query, text);
    base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

    EXPECT_LT(elapsed_time, kCalculationTimeUpperBound);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
    VLOG(1) << "Elapsed time (ms): " << elapsed_time.InMillisecondsF();
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkStressTestLongTextLongQuery) {
  std::u16string text(300, 'a');

  std::u16string query_high_match(300, 'a');
  std::u16string query_low_match = std::u16string(140, 'b') +
                                   std::u16string(20, 'a') +
                                   std::u16string(140, 'b');
  std::u16string query_no_match(300, 'b');
  std::vector<std::u16string> queries = {query_high_match, query_low_match,
                                         query_no_match};

  for (const auto& query : queries) {
    base::Time start_time = base::Time::NowFromSystemTime();
    const double relevance = CalculateRelevance(query, text);
    base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

    EXPECT_LT(elapsed_time, kCalculationTimeUpperBound);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    VLOG(1) << "Elapsed time (ms): " << elapsed_time.InMillisecondsF();
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkStressTestManyTokens) {
  std::u16string text =
      u"aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll mmm nnn ooo ppp qqq "
      u"rrr sss ttt uuu vvv www xxx yyy zzz";
  std::u16string query =
      u"zzz yyy xxx www vvv uuu ttt sss rrr qqq ppp ooo nnn mmm lll kkk jjj "
      u"iii hhh ggg fff eee ddd ccc bbb aaa";

  base::Time start_time = base::Time::NowFromSystemTime();
  const double relevance = CalculateRelevance(query, text);
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  EXPECT_LT(elapsed_time, kCalculationTimeUpperBound);
  VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                   /*query_first*/ false);
  VLOG(1) << "Elapsed time (ms): " << elapsed_time.InMillisecondsF();
}

/**********************************************************************
 * Benchmarking section 2 - Non-abstract test cases                   *
 **********************************************************************/

// TODO(crbug.com/40211626): Make matching less permissive where the strings
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

// TODO(crbug.com/40227656): Reduce permissivity currently afforded by block
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
    ExpectMostlyIncreasing(scores, /*epsilon=*/0.001);
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
  std::vector<std::u16string> texts = {u"Pixark", u"LOST ARK",
                                       u"ARK: Survival Evolved"};
  std::vector<double> scores;
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
  ExpectStrictlyIncreasing(scores);
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
      u"new ",           u"desk",
      u"new d",          u"new de",
      u"new des",        u"new desk",
      u"create d",       u"create n",
      u"create de",      u"create ne",
      u"create a d",     u"create a de",
      u"create new",     u"create des",
      u"create new ",    u"create desk",
      u"create a des",   u"create new d",
      u"create a desk",  u"create new de",
      u"create new des", u"create new desk",
  };

  std::vector<double> scores;
  for (const auto& query : queries_strict_prefix) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  // Allow a flexible (rather than strict) increase in scores.
  ExpectMostlyIncreasing(scores, /*epsilon*/ 0.005);

  scores.clear();
  for (const auto& query : queries_missing_words) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }

  // With word order flexibility, the scores are expected to increase as the
  // query length increases.
  //
  // Allow a flexible (rather than strict) increase in scores.
  ExpectMostlyIncreasing(scores, /*epsilon*/ 0.005);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkKeyboardShortcutsEmojiPicker) {
  std::u16string text = u"Open Emoji picker";
  std::vector<std::u16string> queries = {u"emoj", u"emoji", u"emoji ",
                                         u"emoji p", u"emoji pi"};
  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
    scores.push_back(relevance);
  }
  ExpectMostlyIncreasing(scores, /*epsilon=*/0.001);
  ExpectAllNearlyEqualTo(scores, 0.9, /*abs_error=*/0.1);
}

TEST_F(FuzzyTokenizedStringMatchTest,
       BenchmarkKeyboardShortcutsIncognitoWindow) {
  std::u16string text = u"Open a new window in incognito mode";
  std::vector<std::u16string> queries = {u"new window incognito",
                                         u"new incognito window"};
  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  ExpectAllNearlyEqual(scores);
}

TEST_F(FuzzyTokenizedStringMatchTest, BenchmarkSettingsPreferences) {
  std::u16string query = u"preferences";
  std::vector<std::u16string> texts = {
      u"Android preferences", u"Caption preferences", u"System preferences",
      u"External storage preferences"};
  std::vector<double> scores;
  for (const auto& text : texts) {
    const double relevance = CalculateRelevance(query, text);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ true);
  }
  ExpectAllNearlyEqual(scores);
}

/**********************************************************************
 * Per-method tests                                                   *
 **********************************************************************/
// The tests in this section check the functionality of individual class
// methods (as opposed to the score benchmarking performed above).

// TODO(crbug.com/1336160): update the tests once params are consolidated.
TEST_F(FuzzyTokenizedStringMatchTest, PartialRatioTest) {
  FuzzyTokenizedStringMatch match;
  EXPECT_NEAR(match.PartialRatio(u"abcde", u"ababcXXXbcdeY"), 0.6, 0.1);
  EXPECT_NEAR(match.PartialRatio(u"big string", u"strength"), 0.7, 0.1);
  EXPECT_EQ(match.PartialRatio(u"abc", u""), 0);
  EXPECT_NEAR(match.PartialRatio(u"different in order", u"order text"), 0.6,
              0.1);
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSetRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"order different in");
    std::u16string text(u"text order");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true),
              1);
    EXPECT_NEAR(match.TokenSetRatio(TokenizedString(query),
                                    TokenizedString(text), false),
                0.6, 0.1);
  }
  {
    std::u16string query(u"short text");
    std::u16string text(u"this text is really really really long");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true),
              1);
    EXPECT_NEAR(match.TokenSetRatio(TokenizedString(query),
                                    TokenizedString(text), false),
                0.5, 0.1);
  }
  {
    std::u16string query(u"common string");
    std::u16string text(u"nothing is shared");
    EXPECT_NEAR(match.TokenSetRatio(TokenizedString(query),
                                    TokenizedString(text), true),
                0.3, 0.1);
    EXPECT_NEAR(match.TokenSetRatio(TokenizedString(query),
                                    TokenizedString(text), false),
                0.3, 0.1);
  }
  {
    std::u16string query(u"token shared token same shared same");
    std::u16string text(u"token shared token text text long");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true),
              1);
    EXPECT_NEAR(match.TokenSetRatio(TokenizedString(query),
                                    TokenizedString(text), false),
                0.8, 0.1);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSortRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"order different in");
    std::u16string text(u"text order");
    EXPECT_NEAR(match.TokenSortRatio(TokenizedString(query),
                                     TokenizedString(text), true),
                0.6, 0.1);
    EXPECT_NEAR(match.TokenSortRatio(TokenizedString(query),
                                     TokenizedString(text), false),
                0.3, 0.1);
  }
  {
    std::u16string query(u"short text");
    std::u16string text(u"this text is really really really long");
    EXPECT_EQ(match.TokenSortRatio(TokenizedString(query),
                                   TokenizedString(text), true),
              0.5 * std::pow(0.9, 1));
    EXPECT_NEAR(match.TokenSortRatio(TokenizedString(query),
                                     TokenizedString(text), false),
                0.3, 0.1);
  }
  {
    std::u16string query(u"common string");
    std::u16string text(u"nothing is shared");
    EXPECT_NEAR(match.TokenSortRatio(TokenizedString(query),
                                     TokenizedString(text), true),
                0.3, 0.1);
    EXPECT_NEAR(match.TokenSortRatio(TokenizedString(query),
                                     TokenizedString(text), false),
                0.3, 0.1);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, WeightedRatio) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text)), 0.6,
        0.1);
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text)), 0.8,
        0.1);
  }
  {
    std::u16string query(u"final fantasy");
    std::u16string text(u"finalfantasy");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text)), 0.9,
        0.1);
  }
  {
    std::u16string query(u"short text!!!");
    std::u16string text(
        u"this sentence is much much much much much longer "
        u"than the text before");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text)), 0.6,
        0.1);
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
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.99, 0.01);
  }
  {
    std::u16string query(u"clan clash");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.98, 0.01);
  }
  {
    std::u16string query(u"clashofclan");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                1.0, 0.01);
  }
  {
    std::u16string query(u"coc");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"c o c");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"wifi");
    std::u16string text(u"wi-fi");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.94, 0.01);
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
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                1.0, 0.01);
  }
  {
    std::u16string query(u"google play");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.99, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, AcronymMatchTest) {
  {
    std::u16string query(u"coc");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::AcronymMatcher(
                    TokenizedString(query), TokenizedString(text)),
                0.84, 0.01);
  }
  // TODO(crbug.com/1336160): Consider allowing acronym matching for query with
  // space in between.
  // E.g., query: "c o c" and text: "Clash of Clan".
  // But we may also want to exclude some cases.
  // E.g., query: "c oc" and text: "Clash of Clan".
  {
    std::u16string query(u"c o c");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::AcronymMatcher(TokenizedString(query),
                                                        TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"cloc");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::AcronymMatcher(TokenizedString(query),
                                                        TokenizedString(text)),
              0.0);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_LT(
        match.Relevance(TokenizedString(query), TokenizedString(text), true),
        0.64);
  }
  {
    std::u16string query(u"CC");
    std::u16string text(u"Clash Of Clan");
    EXPECT_LT(
        match.Relevance(TokenizedString(query), TokenizedString(text), true),
        0.5);
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_LT(
        match.Relevance(TokenizedString(query), TokenizedString(text), true),
        0.77);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ExactTextMatchTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(u"yat");
  std::u16string text(u"YaT");
  const double relevance =
      match.Relevance(TokenizedString(query), TokenizedString(text), false);
  EXPECT_GT(relevance, 0.35);
  EXPECT_DOUBLE_EQ(relevance, 1.0);
  EXPECT_EQ(match.hits().size(), 1u);
  EXPECT_EQ(match.hits()[0].start(), 0u);
  EXPECT_EQ(match.hits()[0].end(), 3u);
}

TEST_F(FuzzyTokenizedStringMatchTest, DiacriticsStripTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string text = u"áêïôu";
  std::vector<std::u16string> queries = {u"aeiou", u"ÀēíÔù", u"aÉiøÚ",
                                         u"ÅĒÎÓÙ"};
  std::vector<double> scores;
  for (const auto& query : queries) {
    const double relevance =
        match.Relevance(TokenizedString(query), TokenizedString(text),
                        kUseWeightedRatio, /*strip_diacritics*/ true);
    scores.push_back(relevance);
    VLOG(1) << FormatRelevanceResult(query, text, relevance,
                                     /*query_first*/ false);
  }
  ExpectAllNearlyEqual(scores);
}

}  // namespace ash::string_matching
