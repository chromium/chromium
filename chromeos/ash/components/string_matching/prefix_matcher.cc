// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher.h"

#include "base/check.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_char_iterator.h"

namespace ash::string_matching {

// TODO(crbug.com/1336160): Paradigm shift 1: Reconsider the value of
// search-via-acronym, i.e. the logic around `kIsFrontOfWordMultiplier`.
//
// TODO(crbug.com/1336160): Paradigm shift 2: Consider scoring matching prefixes
// of tokens with equal value, regardless of whether the token is a first token
// or non-first token.
//
// PrefixMatcher:
//
// The factors below are applied when the current char of query matches
// the current char of the text to be matched. Different factors are chosen
// based on where the match happens:
//
// 1) `kIsPrefixMultiplier` is used when the matched portion is a prefix of both
// the query and the text, which implies that the matched chars are at the same
// position in query and text. This is the most preferred case thus it has the
// highest score.
//
// When the current char of the query and the text do not match, the algorithm
// moves to the next token in the text and tries to match from there.
//
// 2) `kIsFrontOfWordMultiplier` will be used if the first char of the token
// matches the current char of the query.
//
// 3) Otherwise, the match is considered as weak, and `kIsWeakHitMultiplier` is
// used.
//
// Examples:
//
//   For text: 'Google Chrome'.
//
//   Query 'go' would yield kIsPrefixMultiplier for each char.
//   Query 'gc' would use kIsPrefixMultiplier for 'g' and
//       kIsFrontOfWordMultiplier for 'c'.
//   Query 'ch' would use kIsFrontOfWordMultiplier for 'c' and
//       kIsWeakHitMultiplier for 'h'.

// kNoMatchScore is a relevance score that represents no match.

PrefixMatcher::PrefixMatcher(const TokenizedString& query,
                             const TokenizedString& text)
    : query_iter_(query),
      text_iter_(text),
      current_match_(gfx::Range::InvalidRange()),
      current_relevance_(constants::kNoMatchScore) {}

bool PrefixMatcher::Match() {
  while (!RunMatch()) {
    // No match found and no more states to try. Bail out.
    if (states_.empty()) {
      current_relevance_ = constants::kNoMatchScore;
      current_hits_.clear();
      return false;
    }

    PopState();

    // Skip restored match to try other possibilities.
    AdvanceToNextTextToken();
  }

  if (current_match_.IsValid())
    current_hits_.push_back(current_match_);

  return true;
}

PrefixMatcher::State::State() : relevance(constants::kNoMatchScore) {}
PrefixMatcher::State::~State() = default;
PrefixMatcher::State::State(double relevance,
                            const gfx::Range& current_match,
                            const Hits& hits,
                            const TokenizedStringCharIterator& query_iter,
                            const TokenizedStringCharIterator& text_iter)
    : relevance(relevance),
      current_match(current_match),
      hits(hits.begin(), hits.end()),
      query_iter_state(query_iter.GetState()),
      text_iter_state(text_iter.GetState()) {}
PrefixMatcher::State::State(const PrefixMatcher::State& state) = default;

bool PrefixMatcher::RunMatch() {
  bool have_match_already = false;
  while (!query_iter_.end() && !text_iter_.end()) {
    if (query_iter_.Get() == text_iter_.Get()) {
      PushState();

      if (query_iter_.GetArrayPos() == text_iter_.GetArrayPos())
        current_relevance_ += constants::kIsPrefixMultiplier;
      else if (text_iter_.IsFirstCharOfToken())
        current_relevance_ += constants::kIsFrontOfWordMultiplier;
      else
        current_relevance_ += constants::kIsWeakHitMultiplier;

      if (!current_match_.IsValid())
        current_match_.set_start(text_iter_.GetArrayPos());
      current_match_.set_end(text_iter_.GetArrayPos() +
                             text_iter_.GetCharSize());

      query_iter_.NextChar();
      text_iter_.NextChar();
      have_match_already = true;
    } else {
      // Character mismatch. Multiple possibilities:

      if (text_iter_.IsFirstCharOfToken()) {
        if (have_match_already) {
          // We have a mismatch in the first letter of the current token, and
          // have observed matches in previous tokens. Consider this a no match.
          return false;
        } else {
          // No matches have been found so far. Skip over current token.
          AdvanceToNextTextToken();
        }
      } else if (text_iter_.IsSecondCharOfToken()) {
        // We have a match in the first letter of the current token, and the
        // next character doesn't match. In this case we can
        // AdvanceToNextTextToken().
        AdvanceToNextTextToken();
      } else {
        // Mismatch is in the third or further char of the text token. Consider
        // this a no match.
        return false;
      }
    }
  }

  return query_iter_.end();
}

void PrefixMatcher::AdvanceToNextTextToken() {
  if (current_match_.IsValid()) {
    current_hits_.push_back(current_match_);
    current_match_ = gfx::Range::InvalidRange();
  }

  text_iter_.NextToken();
}

void PrefixMatcher::PushState() {
  states_.push_back(State(current_relevance_, current_match_, current_hits_,
                          query_iter_, text_iter_));
}

void PrefixMatcher::PopState() {
  DCHECK(!states_.empty());

  State& last_match = states_.back();
  current_relevance_ = last_match.relevance;
  current_match_ = last_match.current_match;
  current_hits_.swap(last_match.hits);
  query_iter_.SetState(last_match.query_iter_state);
  text_iter_.SetState(last_match.text_iter_state);

  states_.pop_back();
}

}  // namespace ash::string_matching
