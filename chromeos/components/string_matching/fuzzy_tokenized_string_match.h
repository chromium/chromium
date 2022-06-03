// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_STRING_MATCHING_FUZZY_TOKENIZED_STRING_MATCH_H_
#define CHROMEOS_COMPONENTS_STRING_MATCHING_FUZZY_TOKENIZED_STRING_MATCH_H_

#include "base/gtest_prod_util.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "ui/gfx/range/range.h"

namespace chromeos {
namespace string_matching {

// FuzzyTokenizedStringMatch takes two tokenized strings: one as the text and
// the other one as the query. It matches the query against the text,
// calculates a relevance score between [0, 1] and marks the matched portions
// of text. A relevance of zero means the two are completely different to each
// other. The higher the relevance score, the better the two strings are
// matched. Matched portions of text are stored as index ranges.
// TODO(crbug.com/1018613): each of these functions have too many input params,
// we should revise the structure and remove unnecessary ones.
class FuzzyTokenizedStringMatch {
 public:
  typedef std::vector<gfx::Range> Hits;

  FuzzyTokenizedStringMatch();

  FuzzyTokenizedStringMatch(const FuzzyTokenizedStringMatch&) = delete;
  FuzzyTokenizedStringMatch& operator=(const FuzzyTokenizedStringMatch&) =
      delete;

  ~FuzzyTokenizedStringMatch();

  // TokenSetRatio takes two sets of tokens, finds their intersection and
  // differences. From the intersection and differences, it rewrites the |query|
  // and |text| and find the similarity ratio between them. This function
  // assumes that TokenizedString is already normalized (converted to lower
  // case). Duplicates tokens will be removed for ratio computation.
  static double TokenSetRatio(const TokenizedString& query,
                              const TokenizedString& text,
                              bool partial,
                              double partial_match_penalty_rate,
                              bool use_edit_distance,
                              double num_matching_blocks_penalty);

  // TokenSortRatio takes two set of tokens, sorts them and find the similarity
  // between two sorted strings. This function assumes that TokenizedString is
  // already normalized (converted to lower case)
  static double TokenSortRatio(const TokenizedString& query,
                               const TokenizedString& text,
                               bool partial,
                               double partial_match_penalty_rate,
                               bool use_edit_distance,
                               double num_matching_blocks_penalty);

  // Finds the best ratio of shorter text with a part of longer text.
  // This function assumes that TokenizedString is already normalized (converted
  // to lower case). The return score is in range of [0, 1].
  static double PartialRatio(const std::u16string& query,
                             const std::u16string& text,
                             double partial_match_penalty_rate,
                             bool use_edit_distance,
                             double num_matching_blocks_penalty);

  // Combines scores from different ratio functions. This function assumes that
  // TokenizedString is already normalized (converted to lower cases).
  // The return score is in range of [0, 1].
  static double WeightedRatio(const TokenizedString& query,
                              const TokenizedString& text,
                              double partial_match_penalty_rate,
                              bool use_edit_distance,
                              double num_matching_blocks_penalty);
  // Since prefix match should always be favored over other matches, this
  // function is dedicated to calculate a prefix match score in range of [0, 1]
  // using PrefixMatcher class.
  // This score has two components: first character match and whole prefix
  // match.
  static double PrefixMatcher(const TokenizedString& query,
                              const TokenizedString& text);

  // Calculates the relevance of two strings. Returns true if two strings are
  // somewhat matched, i.e. relevance score is greater than a threshold.
  bool IsRelevant(const TokenizedString& query,
                  const TokenizedString& text,
                  double relevance_threshold,
                  bool use_prefix_only,
                  bool use_weighted_ratio,
                  bool use_edit_distance,
                  double partial_match_penalty_rate,
                  double num_matching_blocks_penalty = 0.0);
  double relevance() const { return relevance_; }
  const Hits& hits() const { return hits_; }

 private:
  // Score in range of [0,1] representing how well the query matches the text.
  double relevance_ = 0;
  Hits hits_;
};

}  // namespace string_matching
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_STRING_MATCHING_FUZZY_TOKENIZED_STRING_MATCH_H_
