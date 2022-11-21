// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_FUZZY_TOKENIZED_STRING_MATCH_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_FUZZY_TOKENIZED_STRING_MATCH_H_

#include <vector>

#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "ui/gfx/range/range.h"

namespace ash::string_matching {

// FuzzyTokenizedStringMatch takes two tokenized strings: one as the text and
// the other one as the query. It matches the query against the text,
// calculates a relevance score between [0, 1] and marks the matched portions
// of text ("hits").
//
// A relevance of zero means the two strings are completely different to each
// other. The higher the relevance score, the better the two strings are
// matched. Matched portions of text are stored as index ranges.
//
// TODO(crbug.com/1336160): Terminology (for example: relevance vs. ratio) is
// confusing and could be clarified.
class FuzzyTokenizedStringMatch {
 public:
  typedef std::vector<gfx::Range> Hits;

  FuzzyTokenizedStringMatch();

  FuzzyTokenizedStringMatch(const FuzzyTokenizedStringMatch&) = delete;
  FuzzyTokenizedStringMatch& operator=(const FuzzyTokenizedStringMatch&) =
      delete;

  ~FuzzyTokenizedStringMatch();

  // TODO(crbug.com/1336160): The Ratio() methods are called in sequence under
  // certain conditions, and trigger much computation. These could potentially
  // be streamlined or compressed.

  // TokenSetRatio takes two sets of tokens, finds their intersection and
  // differences. From the intersection and differences, it rewrites the |query|
  // and |text| and find the similarity ratio between them. This function
  // assumes that TokenizedString is already normalized (converted to lower
  // case). Duplicate tokens will be removed for ratio computation. The return
  // score is in range [0, 1].
  static double TokenSetRatio(const TokenizedString& query,
                              const TokenizedString& text,
                              bool partial);

  // TokenSortRatio takes two set of tokens, sorts them and find the similarity
  // between two sorted strings. This function assumes that TokenizedString is
  // already normalized (converted to lower case). The return score is in range
  // [0, 1].
  static double TokenSortRatio(const TokenizedString& query,
                               const TokenizedString& text,
                               bool partial);

  // Finds the best ratio of shorter text with a part of longer text.
  // This function assumes that TokenizedString is already normalized (converted
  // to lower case). The return score is in range of [0, 1].
  static double PartialRatio(const std::u16string& query,
                             const std::u16string& text);

  // Combines scores from different ratio functions. This function assumes that
  // TokenizedString is already normalized (converted to lower cases).
  // The return score is in range of [0, 1].
  static double WeightedRatio(const TokenizedString& query,
                              const TokenizedString& text);

  // This function is dedicated to calculate a prefix match score in range of
  // [0, 1] using PrefixMatcher class.
  static double PrefixMatcher(const TokenizedString& query,
                              const TokenizedString& text);

  // This function is dedicated to calculate a first character match (aka
  // acronym match) score in range of [0, 1] using AcronymMatcher class.
  static double AcronymMatcher(const TokenizedString& query,
                               const TokenizedString& text);

  // Calculates and returns the relevance score of |query| relative to |text|.
  // The relevance score is in range of [0,1], representing how well the query
  // matches the text.
  double Relevance(const TokenizedString& query,
                   const TokenizedString& text,
                   bool use_weighted_ratio,
                   bool strip_diacritics = false,
                   bool use_acronym_matcher = false);
  const Hits& hits() const { return hits_; }

 private:
  // This function is dedicated to calculate a prefix match score in range of
  // [0, 1] and its hits information using PrefixMatcher class.
  static double PrefixMatcher(const TokenizedString& query,
                              const TokenizedString& text,
                              std::vector<Hits>& hits_vector);
  // This function is dedicated to calculate a first character match (aka
  // acronym match) score in range of [0, 1] and its hits information using
  // AcronymMatcher class.
  static double AcronymMatcher(const TokenizedString& query,
                               const TokenizedString& text,
                               std::vector<Hits>& hits_vector);

  Hits hits_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_FUZZY_TOKENIZED_STRING_MATCH_H_
