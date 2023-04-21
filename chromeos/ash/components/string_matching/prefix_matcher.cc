// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher.h"

#include <cstddef>
#include <queue>
#include <string>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::string_matching {
namespace {
using prefix_matcher_constants::kIsFrontOfTokenCharScore;
using prefix_matcher_constants::kIsPrefixCharScore;
using prefix_matcher_constants::kIsWeakHitCharScore;
using prefix_matcher_constants::kNoMatchScore;

MatchInfo::MatchInfo() = default;
MatchInfo::~MatchInfo() = default;
}  // namespace

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

PrefixMatcher::PrefixMatcher(const TokenizedString& query,
                             const TokenizedString& text)
    : query_(query), text_(text) {}
PrefixMatcher::~PrefixMatcher() = default;

bool PrefixMatcher::Match() {
  MatchInfo sentence_match_info;
  SentencePrefixMatch(sentence_match_info);
  MatchInfo token_match_info;
  TokenPrefixMatch(token_match_info);

  MatchInfo& better_match =
      sentence_match_info.relevance >= token_match_info.relevance
          ? sentence_match_info
          : token_match_info;
  relevance_ = better_match.relevance;
  hits_ = better_match.hits;
  return relevance_ > 0.0;
}

void PrefixMatcher::SentencePrefixMatch(MatchInfo& sentence_match_info) {
  // Since we are concatenating the tokens, we do not have whitespace
  // separation, and so we have to be careful with index calculation later on.
  std::u16string query_sentence = base::StrCat(query_->tokens());
  std::u16string text_sentence = base::StrCat(text_->tokens());

  // Queue to store the starting index of each token in the `text_sentence`.
  std::queue<size_t> text_token_indexes;
  size_t token_index = 0;
  for (auto text_token : text_->tokens()) {
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

    // Calculate the `relevance` score and `hits` and return if the found match
    // begins from the starting index of a token.
    DCHECK(text_token_indexes.front() == matched_index);
    size_t text_pos = text_->tokens().size() - text_token_indexes.size();
    size_t text_sentence_end_pos = matched_index + query_sentence.size();

    sentence_match_info.relevance = 0.0;
    sentence_match_info.current_match.set_start(
        text_->mappings()[text_pos].start());

    while (!text_token_indexes.empty() &&
           text_token_indexes.front() < text_sentence_end_pos) {
      // Text size may be smaller than the token size as prefix matching is
      // allowed for the last token.
      size_t text_size =
          std::min(text_->tokens()[text_pos].size(),
                   text_sentence_end_pos - text_token_indexes.front());
      sentence_match_info.relevance +=
          matched_index == 0 ? kIsPrefixCharScore * text_size
                             : kIsFrontOfTokenCharScore +
                                   kIsWeakHitCharScore * (text_size - 1);
      sentence_match_info.current_match.set_end(
          text_->mappings()[text_pos].start() + text_size);

      ++text_pos;
      text_token_indexes.pop();
    }
    sentence_match_info.hits.push_back(sentence_match_info.current_match);
    return;
  }
}

void PrefixMatcher::TokenPrefixMatch(MatchInfo& token_match_info) {
  const size_t num_query_token = query_->tokens().size();
  const size_t num_text_token = text_->tokens().size();

  // Not a prefix match if `query_`  contains more tokens than  `text_` , or
  // if `query_`  is empty.
  if (query_->tokens().empty() || num_query_token > num_text_token) {
    return;
  }

  // We use `query_map` to allow fast match between query tokens and text
  // tokens. Queue stores the query token index and ensures the tokens to
  // match in sequence when multiple same tokens exists. It does not store the
  // last query token as the last query token allows token prefix matching.
  base::flat_map<std::u16string, std::queue<size_t>> query_map;
  for (size_t i = 0; i < num_query_token - 1; ++i) {
    query_map[query_->tokens()[i]].emplace(i);
  }
  const std::u16string last_query_token = query_->tokens()[num_query_token - 1];

  size_t matched_num = 0;
  bool last_query_token_matched = false;
  for (size_t text_pos = 0; text_pos < num_text_token; ++text_pos) {
    const std::u16string text_token = text_->tokens()[text_pos];

    if (query_map.contains(text_token)) {
      DCHECK(!query_map[text_token].empty());
      size_t query_pos = query_map[text_token].front();

      query_map[text_token].pop();
      if (query_map[text_token].empty())
        query_map.erase(text_token);

      UpdateInfoForTokenPrefixMatch(query_pos, text_pos, token_match_info);
      ++matched_num;
    }
    // The last query token can be matched for at most once.
    else if (!last_query_token_matched &&
             last_query_token.size() <= text_token.size() &&
             text_token.compare(0, last_query_token.size(), last_query_token) ==
                 0) {
      // This case handles an incomplete last query.
      // Example:
      //   For text: 'Google Chrome'.
      //
      //   Query 'Google Ch' is also a match as the query token 'ch' is the
      //   prefix of the text token 'chrome'.
      UpdateInfoForTokenPrefixMatch(num_query_token - 1, text_pos,
                                    token_match_info);
      ++matched_num;
      last_query_token_matched = true;
    }

    if (matched_num == num_query_token) {
      DCHECK(token_match_info.current_match.IsValid());
      token_match_info.hits.push_back(token_match_info.current_match);
      return;
    }
  }
  token_match_info.relevance = kNoMatchScore;
}

void PrefixMatcher::UpdateInfoForTokenPrefixMatch(size_t query_pos,
                                                  size_t text_pos,
                                                  MatchInfo& token_match_info) {
  const size_t hit_start_pos = text_->mappings()[text_pos].start();
  const size_t hit_end_pos =
      text_->mappings()[text_pos].start() + query_->tokens()[query_pos].size();

  // Update the hits information.
  if (query_pos != token_match_info.last_query_pos + 1 ||
      text_pos != token_match_info.last_query_pos + 1) {
    token_match_info.is_front = false;

    // When it's not continuous matching and we have a valid `current_match`,
    // push it `hits` and start a new match.
    if (token_match_info.current_match.IsValid()) {
      token_match_info.hits.push_back(token_match_info.current_match);
      token_match_info.current_match = gfx::Range::InvalidRange();
    }
  }
  // It's a continuous matching if the `current_match` is valid.
  // Update only the end position if it's a continuous matching, and update both
  // the start && end positions otherwise.
  if (!token_match_info.current_match.IsValid())
    token_match_info.current_match.set_start(hit_start_pos);
  token_match_info.current_match.set_end(hit_end_pos);

  // Update relevance score.
  token_match_info.relevance +=
      token_match_info.is_front
          ? kIsPrefixCharScore * query_->tokens().at(query_pos).size()
          : kIsFrontOfTokenCharScore +
                kIsWeakHitCharScore *
                    (query_->tokens().at(query_pos).size() - 1);
  token_match_info.last_query_pos = query_pos;
  token_match_info.last_text_pos = text_pos;
}

}  // namespace ash::string_matching
