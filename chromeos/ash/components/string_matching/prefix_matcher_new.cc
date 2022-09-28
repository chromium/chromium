// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher_new.h"

#include <cstddef>
#include <queue>
#include <string>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::string_matching {
MatchInfo::MatchInfo() = default;
MatchInfo::~MatchInfo() = default;

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
  MatchInfo sentence_match_info;
  SentencePrefixMatch(sentence_match_info);
  MatchInfo token_match_info;
  TokenPrefixMatch(token_match_info);
  relevance_ =
      std::max(sentence_match_info.relevance, token_match_info.relevance);
  return relevance_ > 0.0;
}

void PrefixMatcherNew::SentencePrefixMatch(MatchInfo& sentence_match_info) {
  std::u16string query_sentence = base::StrCat(query_.tokens());
  std::u16string text_sentence = base::StrCat(text_.tokens());

  // Queue to store the starting index of each token in the `text_sentence`.
  std::queue<size_t> text_token_indexes;
  size_t token_index = 0;
  for (auto text_token : text_.tokens()) {
    text_token_indexes.push(token_index);
    token_index += text_token.size();
  }

  while (!text_token_indexes.empty()) {
    size_t start_index = text_token_indexes.front();
    size_t matched_index = text_sentence.find(query_sentence, start_index);

    while (!text_token_indexes.empty() &&
           text_token_indexes.front() < matched_index) {
      text_token_indexes.pop();
    }

    if (text_token_indexes.empty() ||
        text_token_indexes.front() > matched_index) {
      continue;
    }

    // Update the relevance score and break the loop if the found match begins
    // from the starting index of a token.
    DCHECK(text_token_indexes.front() == matched_index);
    if (matched_index == 0)
      sentence_match_info.relevance =
          constants::kIsPrefixCharScore * query_sentence.size();
    else {
      sentence_match_info.relevance =
          constants::kIsWeakHitCharScore * query_sentence.size();
      while (!text_token_indexes.empty() &&
             text_token_indexes.front() <
                 matched_index + query_sentence.size()) {
        text_token_indexes.pop();
        sentence_match_info.relevance += constants::kIsFrontOfTokenCharScore -
                                         constants::kIsWeakHitCharScore;
      }
    }
    return;
  }
}

void PrefixMatcherNew::TokenPrefixMatch(MatchInfo& token_match_info) {
  const size_t num_query_token = query_.tokens().size();
  const size_t num_text_token = text_.tokens().size();

  // Not a prefix match if `query_`  contains more tokens than  `text_` , or
  // if `query_`  is empty.
  if (query_.tokens().empty() || num_query_token > num_text_token)
    return;

  // We use `query_map` to allow fast match between query tokens and text
  // tokens. Queue stores the query token index and ensures the tokens to
  // match in sequence when multiple same tokens exists. It does not store the
  // last query token as the last query token allows token prefix matching.
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

      UpdateInfoForTokenPrefixMatch(query_pos, text_pos, token_match_info);
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
      UpdateInfoForTokenPrefixMatch(num_query_token - 1, text_pos,
                                    token_match_info);
      ++matched_num;
    }

    if (matched_num == num_query_token)
      return;
  }
  token_match_info.relevance = constants::kNoMatchScore;
}

void PrefixMatcherNew::UpdateInfoForTokenPrefixMatch(
    size_t query_pos,
    size_t text_pos,
    MatchInfo& token_match_info) {
  // TODO(crbug.com/1336160): Update the hits information. We concentrate on
  // the correctness of the prefix matching and the relevance score in the
  // first implement. The hits will be calculated and tested when prefix
  // matcher is replaced in TokenizedStringMatch.
  if (token_match_info.is_front &&
      (query_pos != token_match_info.last_query_pos + 1 ||
       text_pos != token_match_info.last_query_pos + 1)) {
    token_match_info.is_front = false;
  }
  token_match_info.relevance +=
      token_match_info.is_front
          ? constants::kIsPrefixCharScore * query_.tokens().at(query_pos).size()
          : constants::kIsFrontOfTokenCharScore +
                constants::kIsWeakHitCharScore *
                    (query_.tokens().at(query_pos).size() - 1);
  token_match_info.last_query_pos = query_pos;
  token_match_info.last_text_pos = text_pos;
}

}  // namespace ash::string_matching
