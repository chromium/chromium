// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/tokenized_string_match.h"

#include <stddef.h>

#include <array>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

// Returns a string of |text| marked the hits in |match| using block bracket.
// e.g. text= "Text", match.hits = [{0,1}], returns "[T]ext".
//
// TODO(crbug.com/1336160): Consider defining it as a |test_util| function as it
// has been used for several unit tests.
std::string MatchHit(const std::u16string& text,
                     const TokenizedStringMatch& match) {
  std::u16string marked = text;

  const TokenizedStringMatch::Hits& hits = match.hits();
  for (const gfx::Range& hit : base::Reversed(hits)) {
    marked.insert(hit.end(), 1, ']');
    marked.insert(hit.start(), 1, '[');
  }

  return base::UTF16ToUTF8(marked);
}

}  // namespace

TEST(TokenizedStringMatchTest, NotMatch) {
  struct TestCase {
    const char* text;
    const char* query;
  };
  constexpr std::array kTestCases = {
      TestCase{"", ""},        TestCase{"", "query"},
      TestCase{"text", ""},    TestCase{"!", "!@#$%^&*()<<<**>>>"},
      TestCase{"abd", "abcd"}, TestCase{"cd", "abcd"},
  };

  TokenizedStringMatch match;
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const std::u16string text(base::UTF8ToUTF16(kTestCases[i].text));
    EXPECT_FALSE(match.Calculate(base::UTF8ToUTF16(kTestCases[i].query), text))
        << "Test case " << i << " : text=" << kTestCases[i].text
        << ", query=" << kTestCases[i].query;
  }
}

TEST(TokenizedStringMatchTest, Match) {
  struct TestCase {
    const char* text;
    const char* query;
    const char* expect;
  };
  constexpr std::array kTestCases = {
      TestCase{"ScratchPad", "pad", "Scratch[Pad]"},
      TestCase{"Chess2", "che", "[Che]ss2"},
      TestCase{"John Doe", "john d", "[John D]oe"},
      TestCase{"Cut the rope", "cut ro", "[Cut] the [ro]pe"},
      TestCase{"Secure Shell", "she", "Secure [She]ll"},
      TestCase{"Netflix", "flix", "Net[flix]"},
      TestCase{"John Doe", "johnd", "[John D]oe"},
      TestCase{"John Doe", "doe john", "[John] [Doe]"},
      TestCase{"John Doe", "doe joh", "[Joh]n [Doe]"},
  };

  TokenizedStringMatch match;
  for (const auto& test_case : kTestCases) {
    const std::u16string text(base::UTF8ToUTF16(test_case.text));
    EXPECT_TRUE(match.Calculate(base::UTF8ToUTF16(test_case.query), text));
    EXPECT_EQ(test_case.expect, MatchHit(text, match));
  }
}

TEST(TokenizedStringMatchTest, AcronymMatchNotAllowed) {
  struct TestCase {
    const char* text;
    const char* query;
    const char* expect;
  };
  constexpr std::array kTestCases = {
      TestCase{"ScratchPad", "sp", "ScratchPad"},
      TestCase{"Chess2", "c2", "Chess2"},
      TestCase{"John Doe", "jdoe", "John Doe"},
      TestCase{"hello John Doe", "jdoe", "hello John Doe"},
  };

  TokenizedStringMatch match;
  for (const auto& test_case : kTestCases) {
    const std::u16string text(base::UTF8ToUTF16(test_case.text));
    EXPECT_FALSE(match.Calculate(base::UTF8ToUTF16(test_case.query), text));
    EXPECT_EQ(test_case.expect, MatchHit(text, match));
  }
}

TEST(TokenizedStringMatchTest, Relevance) {
  struct TestCase {
    const char* text;
    const char* query_low;
    const char* query_high;
  };
  constexpr std::array kTestCases = {
      // More matched chars are better.
      TestCase{"Google Chrome", "g", "go"},
      TestCase{"Google Chrome", "go", "goo"},
      TestCase{"Google Chrome", "goo", "goog"},
      TestCase{"Google Chrome", "c", "ch"},
      TestCase{"Google Chrome", "ch", "chr"},
      // Prefix match is better than middle match.
      TestCase{"Google Chrome", "ch", "go"},
      // Substring match has the lowest score.
      TestCase{"Google Chrome", "oo", "go"},
      TestCase{"Google Chrome", "oo", "ch"},
  };

  TokenizedStringMatch match_low;
  TokenizedStringMatch match_high;
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const std::u16string text(base::UTF8ToUTF16(kTestCases[i].text));
    EXPECT_TRUE(
        match_low.Calculate(base::UTF8ToUTF16(kTestCases[i].query_low), text));
    EXPECT_TRUE(match_high.Calculate(
        base::UTF8ToUTF16(kTestCases[i].query_high), text));
    EXPECT_LT(match_low.relevance(), match_high.relevance())
        << "Test case " << i << " : text=" << kTestCases[i].text
        << ", query_low=" << kTestCases[i].query_low
        << ", query_high=" << kTestCases[i].query_high;
  }
}

// More specialized tests of the absolute relevance scores. (These tests are
// minimal, because they are so brittle. Changing the scoring algorithm will
// require updating this test.)
TEST(TokenizedStringMatchTest, AbsoluteRelevance) {
  const double kEpsilon = 0.006;
  struct TestCase {
    const char* text;
    const char* query;
    double expected_score;
  };
  constexpr std::array kTestCases = {
      // The first few chars should increase the score extremely high. After
      // that, they should count less.
      // NOTE: 0.87 is a magic number, as it is the Omnibox score for a "pretty
      // good" match. We want a 3-letter prefix match to be slightly above 0.87.
      TestCase{"Google Chrome", "g", 0.5},
      TestCase{"Google Chrome", "go", 0.75},
      TestCase{"Google Chrome", "goo", 0.88},
      TestCase{"Google Chrome", "goog", 0.94},
  };

  TokenizedStringMatch match;
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const std::u16string text(base::UTF8ToUTF16(kTestCases[i].text));
    EXPECT_TRUE(match.Calculate(base::UTF8ToUTF16(kTestCases[i].query), text));

    EXPECT_NEAR(match.relevance(), kTestCases[i].expected_score, kEpsilon)
        << "Test case " << i << " : text=" << kTestCases[i].text
        << ", query=" << kTestCases[i].query
        << ", expected_score=" << kTestCases[i].expected_score;
  }
}

}  // namespace ash::string_matching
