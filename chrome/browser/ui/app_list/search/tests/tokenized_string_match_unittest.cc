// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/tokenized_string_match.h"

#include <stddef.h>

#include <string>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

// Returns a string of |text| marked the hits in |match| using block bracket.
// e.g. text= "Text", match.hits = [{0,1}], returns "[T]ext".
std::string MatchHit(const base::string16& text,
                     const ash::TokenizedStringMatch& match) {
  base::string16 marked = text;

  const ash::TokenizedStringMatch::Hits& hits = match.hits();
  for (ash::TokenizedStringMatch::Hits::const_reverse_iterator it =
           hits.rbegin();
       it != hits.rend(); ++it) {
    const gfx::Range& hit = *it;
    marked.insert(hit.end(), 1, ']');
    marked.insert(hit.start(), 1, '[');
  }

  return base::UTF16ToUTF8(marked);
}

TEST(TokenizedStringMatchTest, NotMatch) {
  struct {
    const char* text;
    const char* query;
  } kTestCases[] = {
      {"", ""},        {"", "query"},
      {"text", ""},    {"!", "!@#$%^&*()<<<**>>>"},
      {"abd", "abcd"}, {"cd", "abcd"},
  };

  ash::TokenizedStringMatch match;
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const base::string16 text(base::UTF8ToUTF16(kTestCases[i].text));
    EXPECT_FALSE(match.Calculate(base::UTF8ToUTF16(kTestCases[i].query), text))
        << "Test case " << i << " : text=" << kTestCases[i].text
        << ", query=" << kTestCases[i].query;
  }
}

TEST(TokenizedStringMatchTest, Match) {
  struct {
    const char* text;
    const char* query;
    const char* expect;
  } kTestCases[] = {
      {"ScratchPad", "pad", "Scratch[Pad]"},
      {"ScratchPad", "sp", "[S]cratch[P]ad"},
      {"Chess2", "che", "[Che]ss2"},
      {"Chess2", "c2", "[C]hess[2]"},
      {"Cut the rope", "cut ro", "[Cut] the [ro]pe"},
      {"Cut the rope", "cr", "[C]ut the [r]ope"},
      {"John Doe", "jdoe", "[J]ohn [Doe]"},
      {"John Doe", "johnd", "[John D]oe"},
      {"Secure Shell", "she", "Secure [She]ll"},
      {"Simple Secure Shell", "sish", "[Si]mple Secure [Sh]ell"},
      {"Netflix", "flix", "Net[flix]"},
  };

  ash::TokenizedStringMatch match;
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const base::string16 text(base::UTF8ToUTF16(kTestCases[i].text));
    EXPECT_TRUE(match.Calculate(base::UTF8ToUTF16(kTestCases[i].query), text));
    EXPECT_EQ(kTestCases[i].expect, MatchHit(text, match));
  }
}

TEST(TokenizedStringMatchTest, Relevance) {
  struct {
    const char* text;
    const char* query_low;
    const char* query_high;
  } kTestCases[] = {
      // More matched chars are better.
      {"Google Chrome", "g", "go"},
      {"Google Chrome", "go", "goo"},
      {"Google Chrome", "goo", "goog"},
      {"Google Chrome", "c", "ch"},
      {"Google Chrome", "ch", "chr"},
      // Acronym match is better than something in the middle.
      {"Google Chrome", "ch", "gc"},
      // Prefix match is better than middle match and acronym match.
      {"Google Chrome", "ch", "go"},
      {"Google Chrome", "gc", "go"},
      // Substring match has the lowest score.
      {"Google Chrome", "oo", "gc"},
      {"Google Chrome", "oo", "go"},
      {"Google Chrome", "oo", "ch"},
  };

  ash::TokenizedStringMatch match_low;
  ash::TokenizedStringMatch match_high;
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const base::string16 text(base::UTF8ToUTF16(kTestCases[i].text));
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
  struct {
    const char* text;
    const char* query;
    double expected_score;
  } kTestCases[] = {
      // The first few chars should increase the score extremely high. After
      // that, they should count less.
      // NOTE: 0.87 is a magic number, as it is the Omnibox score for a "pretty
      // good" match. We want a 3-letter prefix match to be slightly above 0.87.
      {"Google Chrome", "g", 0.5},
      {"Google Chrome", "go", 0.75},
      {"Google Chrome", "goo", 0.88},
      {"Google Chrome", "goog", 0.94},
  };

  ash::TokenizedStringMatch match;
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const base::string16 text(base::UTF8ToUTF16(kTestCases[i].text));
    EXPECT_TRUE(match.Calculate(base::UTF8ToUTF16(kTestCases[i].query), text));
    EXPECT_NEAR(match.relevance(), kTestCases[i].expected_score, kEpsilon)
        << "Test case " << i << " : text=" << kTestCases[i].text
        << ", query=" << kTestCases[i].query
        << ", expected_score=" << kTestCases[i].expected_score;
  }
}

}  // namespace test
}  // namespace app_list
