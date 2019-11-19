// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_FUZZY_TOKENIZED_STRING_MATCH_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_FUZZY_TOKENIZED_STRING_MATCH_H_

#include "ash/public/cpp/app_list/tokenized_string.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/range/range.h"

namespace ash {
class TokenizedString;
}  // namespace ash

namespace app_list {

// FuzzyTokenizedStringMatch takes two tokenized strings: one as the text and
// the other one as the query. It matches the query against the text,
// calculates a relevance score between [0, 1] and marks the matched portions
// of text. A relevance of zero means the two are completely different to each
// other. The higher the relevance score, the better the two strings are
// matched. Matched portions of text are stored as index ranges.
class FuzzyTokenizedStringMatch {
 public:
  typedef std::vector<gfx::Range> Hits;

  FuzzyTokenizedStringMatch();
  ~FuzzyTokenizedStringMatch();

  // Calculates the relevance of two strings. Returns true if two strings are
  // somewhat matched, i.e. relevance score is greater than a threshold.
  bool IsRelevant(const ash::TokenizedString& query,
                  const ash::TokenizedString& text);
  double relevance() const { return relevance_; }
  const Hits& hits() const { return hits_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(FuzzyTokenizedStringMatchTest, PartialRatioTest);
  FRIEND_TEST_ALL_PREFIXES(FuzzyTokenizedStringMatchTest, TokenSetRatioTest);
  FRIEND_TEST_ALL_PREFIXES(FuzzyTokenizedStringMatchTest, TokenSortRatioTest);
  FRIEND_TEST_ALL_PREFIXES(FuzzyTokenizedStringMatchTest, WeightedRatio);
  // Finds the best ratio of shorter text with a part of longer text.
  // This function assumes that TokenizedString is already normalized (converted
  // to lower case). The return score is in range of [0, 1].
  double PartialRatio(const base::string16& query, const base::string16& text);
  // TokenSetRatio takes two sets of tokens, finds their intersection and
  // differences. From the intersection and differences, it rewrites the |query|
  // and |text| and find the similarity ratio between them. This function
  // assumes that TokenizedString is already normalized (converted to lower
  // case). Duplicates tokens will be removed for ratio computation.
  double TokenSetRatio(const ash::TokenizedString& query,
                       const ash::TokenizedString& text,
                       bool partial);
  // TokenSortRatio takes two set of tokens, sorts them and find the similarity
  // between two sorted strings. This function assumes that TokenizedString is
  // already normalized (converted to lower case)
  double TokenSortRatio(const ash::TokenizedString& query,
                        const ash::TokenizedString& text,
                        bool partial);
  // Combines scores from different ratio functions. This function assumes that
  // TokenizedString is already normalized (converted to lower cases).
  // The return score is in range of [0, 1].
  double WeightedRatio(const ash::TokenizedString& query,
                       const ash::TokenizedString& text);
  // Since prefix match should always be favored over other matches, this
  // function is dedicated to calculate a prefix match score in range of [0, 1].
  // This score has two components: first character match and whole prefix
  // match.
  double PrefixMatcher(const ash::TokenizedString& query,
                       const ash::TokenizedString& text);
  // Score in range of [0,1] representing how well the query matches the text.
  double relevance_ = 0;
  Hits hits_;

  DISALLOW_COPY_AND_ASSIGN(FuzzyTokenizedStringMatch);
};

namespace internal {
double FirstCharacterMatch(const ash::TokenizedString& query,
                           const ash::TokenizedString& text);
double PrefixMatch(const ash::TokenizedString& query,
                   const ash::TokenizedString& text);
}  // namespace internal
}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_FUZZY_TOKENIZED_STRING_MATCH_H_
