// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_H_

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "ui/gfx/range/range.h"

namespace ash::string_matching {

// These are described in more detail in the .cc file.
namespace prefix_matcher_constants {

constexpr double kIsPrefixCharScore = 1.0;
constexpr double kIsFrontOfTokenCharScore = 0.8;
constexpr double kIsWeakHitCharScore = 0.6;
constexpr double kNoMatchScore = 0.0;

}  // namespace prefix_matcher_constants

namespace {
struct MatchInfo {
 public:
  typedef std::vector<gfx::Range> Hits;

  MatchInfo();
  ~MatchInfo();

  MatchInfo(const MatchInfo&) = delete;
  MatchInfo& operator=(const MatchInfo&) = delete;

  double relevance = prefix_matcher_constants::kNoMatchScore;
  Hits hits;

  gfx::Range current_match = gfx::Range::InvalidRange();
  // The last query/text position that the relevance was updated.
  size_t last_query_pos = SIZE_MAX;
  size_t last_text_pos = SIZE_MAX;
  // Flag to track if we are still matching the prefixes of both the query and
  // text.
  bool is_front = true;
};
}  // namespace

// PrefixMatcher matches the chars of a given query as prefix of tokens in
// a given text. We give some specific scoring examples in the .cc file.
class PrefixMatcher {
 public:
  typedef std::vector<gfx::Range> Hits;

  PrefixMatcher(const TokenizedString& query, const TokenizedString& text);
  ~PrefixMatcher();

  PrefixMatcher(const PrefixMatcher&) = delete;
  PrefixMatcher& operator=(const PrefixMatcher&) = delete;

  // Return true if we found either sentence prefix matching or token prefix
  // matching. If no full match is found, return false.
  bool Match();

  double relevance() const { return relevance_; }
  const Hits& hits() const { return hits_; }

 private:
  // Stops on the first full sentence prefix match and updates the relevance
  // score. If no match found, set relevance as kNoMatchScore.
  //
  // We treat the following as sentence prefix match:
  // query        |       text
  // chromeos     | [chrome os] flex        (prefix)
  // chrome os    | google [chromeos]       (non-prefix)
  // google pixel | buy [google pixel]book  (unfinished)
  //
  // But not the following:
  // query        |       text
  // cof          | chrome os flex
  // go chrome    | google chromeos
  void SentencePrefixMatch(MatchInfo& sentence_match_info);

  // Stops on the first full token prefix match and updates the relevance
  // score. If no match found, set relevance as kNoMatchScore.
  //
  // We treat the following as token prefix match:
  // query        |       text
  // chrome store | my [chrome store]       (continuous)
  // chrome store | [chrome] web [store]    (discrete)
  // chrome google| [google chrome]         (unordered)
  // google pixel | buy [google pixel]book  (unfinished)
  //
  // But not the following:
  // query        |       text
  // cof          | chrome os flex
  // chrome flex  | chromeos flex
  //
  // The time complexity of the token prefix match algorithm is `O(m+n)`, where
  // m is the `num_query_token` and n is the `num_text_token`. O(m) to construct
  // the `query_map` and O(n) to traverse the text tokens to find matches. Each
  // text token will be compared at most twice (one for `query_map` and one for
  // last query token).
  void TokenPrefixMatch(MatchInfo& token_match_info);

  // Update the relevance score of token prefix based on the matched token. This
  // method can cope with full and partial token matches as it always update the
  // `relevance` and `hits` according the query size.
  void UpdateInfoForTokenPrefixMatch(size_t query_pos,
                                     size_t text_pos,
                                     MatchInfo& token_match_info);

  const raw_ref<const TokenizedString> query_;
  const raw_ref<const TokenizedString> text_;

  double relevance_ = prefix_matcher_constants::kNoMatchScore;
  Hits hits_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_H_
