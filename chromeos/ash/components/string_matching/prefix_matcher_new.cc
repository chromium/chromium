// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher_new.h"

#include <cstddef>
#include <queue>
#include <string>

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::string_matching {

// TODO(crbug.com/1336160): Replace PrefixMatcher with PrefixMatcherNew
//
// PrefixMatcher:
//
// The factors below are applied when the current char of query matches
// the current char of the text to be matched. Different factors are chosen
// based on where the match happens:
//
// 1) `kIsPrefixCharScore` is used when the matched portion is a prefix of both
// the query and the text, which implies that the matched chars are at the same
// position in query and text. This is the most preferred case thus it has the
// highest score.
//
// 2) `kIsFrontOfTokenCharScore` will be used if the first char of the token
// matches the current char of the query.
//
// 3) Otherwise, the match is considered as weak, and `kIsWeakHitCharScore` is
// used.
//
// Examples:
//
//   For text: 'Google Chrome'.
//
//   Query 'go' would yield kIsPrefixCharScore for each char.
//   Query 'ch' would use kIsFrontOfTokenCharScore for 'c' and
//       kIsWeakHitCharScore for 'h'.
//
// kNoMatchScore is a relevance score that represents no match.

PrefixMatcherNew::PrefixMatcherNew(const TokenizedString& query,
                                   const TokenizedString& text)
    : query_(query), text_(text) {}
PrefixMatcherNew::~PrefixMatcherNew() = default;

bool PrefixMatcherNew::Match() {
  const size_t num_query_token = query_.tokens().size();
  const size_t num_text_token = text_.tokens().size();

  // Not a prefix match if `query_`  contains more tokens than  `text_` , or if
  // `query_`  is empty.
  if (query_.tokens().empty() || num_query_token > num_text_token)
    return false;

  // We use `query_map` to allow fast match between query tokens and text
  // tokens. Queue stores the query token index and ensures the tokens to match
  // in sequence when multiple same tokens exists. It does not store the last
  // query token as the last query token allows token prefix matching.
  base::flat_map<std::u16string, std::queue<size_t>> query_map;
  for (size_t i = 0; i < num_query_token - 1; ++i) {
    query_map[query_.tokens()[i]].emplace(i);
  }
  const std::u16string last_query_token = query_.tokens()[num_query_token - 1];

  size_t matched_num = 0;
  for (size_t text_pos = 0; text_pos < num_text_token; ++text_pos) {
    const std::u16string text_token = text_.tokens()[text_pos];

    if (query_map.contains(text_token)) {
      DCHECK(!query_map[text_token].empty());
      size_t query_pos = query_map[text_token].front();

      query_map[text_token].pop();
      if (query_map[text_token].empty())
        query_map.erase(text_token);

      UpdateRelevance(query_pos, text_pos);
      ++matched_num;
    } else if (last_query_token.size() <= text_token.size() &&
               text_token.compare(0, last_query_token.size(),
                                  last_query_token) == 0) {
      // This case handles an incomplete last query.
      // Example:
      //   For text: 'Google Chrome'.
      //
      //   Query 'Google Ch' is also a match as the query token 'ch' is the
      //   prefix of the text token 'chrome'.
      UpdateRelevance(num_query_token - 1, text_pos);
      ++matched_num;
    }

    if (matched_num == num_query_token)
      return true;
  }

  relevance_ = constants::kNoMatchScore;
  hits_.clear();
  return false;
}

void PrefixMatcherNew::UpdateRelevance(size_t query_pos, size_t text_pos) {
  // TODO(crbug.com/1336160): Update the hits information. We concentrate on the
  // correctness of the prefix matching and the relevance score in the first
  // implement. The hits will be calculated and tested when prefix matcher is
  // replaced in TokenizedStringMatch.
  if (is_front_ &&
      (query_pos != last_query_pos_ + 1 || text_pos != last_text_pos_ + 1)) {
    is_front_ = false;
  }
  relevance_ += is_front_ ? constants::kIsPrefixCharScore *
                                query_.tokens().at(query_pos).size()
                          : constants::kIsFrontOfTokenCharScore +
                                constants::kIsWeakHitCharScore *
                                    (query_.tokens().at(query_pos).size() - 1);
  last_query_pos_ = query_pos;
  last_text_pos_ = text_pos;
}

}  // namespace ash::string_matching
