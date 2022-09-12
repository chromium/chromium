// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_H_

#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_char_iterator.h"

namespace ash::string_matching {

// These are described in more detail in the .cc file.
namespace constants {

constexpr double kIsPrefixCharScore = 1.0;
constexpr double kIsFrontOfTokenCharScore = 0.8;
constexpr double kIsWeakHitCharScore = 0.6;

constexpr double kNoMatchScore = 0.0;

}  // namespace constants

// PrefixMatcher matches the chars of a given query as prefix of tokens in
// a given text or as prefix of the acronyms of those text tokens.
class PrefixMatcher {
 public:
  typedef std::vector<gfx::Range> Hits;

  PrefixMatcher(const TokenizedString& query, const TokenizedString& text);
  ~PrefixMatcher();

  PrefixMatcher(const PrefixMatcher&) = delete;
  PrefixMatcher& operator=(const PrefixMatcher&) = delete;

  // Invokes RunMatch to perform prefix match. Use |states_| as a stack to
  // perform DFS (depth first search) so that all possible matches are
  // attempted. Stops on the first full match and returns true. Otherwise,
  // returns false to indicate no match.
  bool Match();

  double relevance() const { return current_relevance_; }
  const Hits& hits() const { return current_hits_; }

 private:
  // Context record of a match.
  struct State {
    State();
    ~State();
    State(double relevance,
          const gfx::Range& current_match,
          const Hits& hits,
          const TokenizedStringCharIterator& query_iter,
          const TokenizedStringCharIterator& text_iter);
    State(const State& state);

    // The current score of the processed query chars.
    double relevance;

    // Current matching range.
    gfx::Range current_match;

    // Completed matching ranges of the processed query chars.
    PrefixMatcher::Hits hits;

    // States of the processed query and text chars.
    TokenizedStringCharIterator::State query_iter_state;
    TokenizedStringCharIterator::State text_iter_state;
  };
  typedef std::vector<State> States;

  // Match chars from the query and text one by one. For each matching char,
  // calculate relevance and matching ranges. And the current stats is
  // recorded so that the match could be skipped later to try other
  // possibilities. Repeat until any of the iterators run out. Return true if
  // query iterator runs out, i.e. all chars in query are matched.
  bool RunMatch();

  // Skip to the next text token and close current match. Invoked when a
  // mismatch happens or to skip a restored match.
  void AdvanceToNextTextToken();

  void PushState();
  void PopState();

  TokenizedStringCharIterator query_iter_;
  TokenizedStringCharIterator text_iter_;

  States states_;
  gfx::Range current_match_;

  double current_relevance_;
  Hits current_hits_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_H_
