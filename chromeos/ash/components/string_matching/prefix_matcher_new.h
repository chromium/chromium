// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_NEW_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_NEW_H_

#include "base/notreached.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::string_matching {

// These are described in more detail in the .cc file.
namespace constants {

constexpr double kIsPrefixCharScore = 1.0;
constexpr double kIsFrontOfTokenCharScore = 0.8;
constexpr double kIsWeakHitCharScore = 0.6;
constexpr double kNoMatchScore = 0.0;

}  // namespace constants

// PrefixMatcher matches the chars of a given query as prefix of tokens in
// a given text. We give some specific scoring examples in the .cc file.
class PrefixMatcherNew {
 public:
  typedef std::vector<gfx::Range> Hits;

  PrefixMatcherNew(const TokenizedString& query, const TokenizedString& text);
  ~PrefixMatcherNew();

  PrefixMatcherNew(const PrefixMatcherNew&) = delete;
  PrefixMatcherNew& operator=(const PrefixMatcherNew&) = delete;

  // Stops on the first full match and returns true. Otherwise,
  // returns false to indicate no match.
  //
  // The time complexity of the match algorithm is `O(m+n)`, where m is the
  // `num_query_token` and n is the `num_text_token`. O(m) to construct the
  // `query_map` and O(n) to traverse the text tokens to find matches. Each text
  // token will be compared at most twice (one for `query_map` and one for last
  // query token).
  bool Match();

  double relevance() const { return relevance_; }
  // TODO(crbug.com/1336160): the hits_ has not yet been calculated. It will be
  // implemented in the following CLs, and is not expected to be called at
  // current stage.
  const Hits& hits() const {
    NOTREACHED();
    return hits_;
  }

 private:
  // update the relevance score based on the matched token.
  void UpdateRelevance(size_t query_pos, size_t text_pos);

  const TokenizedString& query_;
  const TokenizedString& text_;

  double relevance_ = constants::kNoMatchScore;
  Hits hits_;

  // The last query/text position that the relevance was updated.
  size_t last_query_pos_ = SIZE_MAX;
  size_t last_text_pos_ = SIZE_MAX;
  // Flag to track if we are still matching the prefixes of both the query and
  // text.
  bool is_front_ = true;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_PREFIX_MATCHER_NEW_H_
