// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_string_filtering_util.h"

#include <cstddef>
#include <string_view>

#include "base/i18n/break_iterator.h"
#include "base/notreached.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/whitespace_constants.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_util.h"

namespace autofill {

namespace {

// Minimum number of characters, excluding whitespace, that a query needs to
// match.
constexpr size_t kMinQueryLength = 2;

std::vector<std::u16string> TokenizeString(std::u16string_view text) {
  std::vector<std::u16string> tokens;
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return tokens;
  }
  while (iter.Advance()) {
    if (iter.IsWord()) {
      tokens.emplace_back(iter.GetString());
    }
  }
  return tokens;
}

std::u16string RemoveWhitespace(std::u16string_view text) {
  std::u16string result;
  base::RemoveChars(text, base::kWhitespaceUTF16, &result);
  return result;
}

}  // namespace

bool FuzzyMatchesSingleToken(std::u16string_view target_token,
                             std::u16string_view query_token) {
  if (query_token.empty()) {
    return true;
  }

  // Determine maximum allowed Levenshtein edit distance based on query length:
  // - Short tokens (len <= 2): 0 edits allowed (exact or exact prefix match).
  // - Medium tokens (3 <= len <= 5): max 1 edit allowed.
  // - Long tokens (len > 5): max 2 edits allowed.
  const size_t query_len = query_token.length();
  const size_t max_distance = (query_len <= 2) ? 0 : (query_len <= 5 ? 1 : 2);

  // Check full token edit distance.
  if (base::LevenshteinDistance(query_token, target_token, max_distance) <=
      max_distance) {
    return true;
  }

  // Check prefix token edit distance (for incomplete query words during
  // typing).
  if (target_token.length() >= query_len) {
    const std::u16string_view target_prefix = target_token.substr(0, query_len);
    if (base::LevenshteinDistance(query_token, target_prefix, max_distance) <=
        max_distance) {
      return true;
    }
  }

  return false;
}

bool FuzzyMatchesOrderedTokens(base::span<const std::u16string> target_tokens,
                               base::span<const std::u16string> query_tokens) {
  if (query_tokens.size() > target_tokens.size()) {
    return false;
  }

  size_t target_idx = 0;
  for (const std::u16string& query_token : query_tokens) {
    bool matched = false;
    while (target_idx < target_tokens.size()) {
      if (FuzzyMatchesSingleToken(target_tokens[target_idx], query_token)) {
        target_idx++;
        matched = true;
        break;
      }
      target_idx++;
    }
    if (!matched) {
      return false;
    }
  }

  return true;
}

bool TokenizeAndFuzzyMatchOrderedTokens(std::u16string_view normalized_target,
                                        std::u16string_view normalized_filter) {
  if (normalized_filter.empty()) {
    return true;
  }

  const std::vector<std::u16string> query_tokens =
      TokenizeString(normalized_filter);
  if (query_tokens.empty()) {
    return true;
  }

  return FuzzyMatchesOrderedTokens(TokenizeString(normalized_target),
                                   query_tokens);
}

RawValueQuery::RawValueQuery(std::u16string_view query) {
  const std::u16string normalized_query =
      normalization::NormalizeForComparison(query);
  normalized_without_whitespace = RemoveWhitespace(normalized_query);
  tokens = TokenizeString(normalized_query);
}

RawValueQuery::~RawValueQuery() = default;

bool RawValueQuery::CanMatch() const {
  // Only the characters of word tokens count: symbols such as "+" survive
  // normalization but are not tokenized, so e.g. "+1" has a single character
  // to match. This also rejects queries without any tokens.
  size_t length = 0;
  for (const std::u16string& token : tokens) {
    length += token.size();
  }
  return length >= kMinQueryLength;
}

bool MatchesRawValueQuery(MemoryDataType type,
                          std::u16string_view normalized_target,
                          const RawValueQuery& query) {
  if (!query.CanMatch()) {
    return false;
  }
  switch (GetRawValueMatchMode(type)) {
    case RawValueMatchMode::kCode:
      return RemoveWhitespace(normalized_target)
          .contains(query.normalized_without_whitespace);
    case RawValueMatchMode::kText:
      return FuzzyMatchesOrderedTokens(TokenizeString(normalized_target),
                                       query.tokens);
    case RawValueMatchMode::kNone:
      return false;
  }
  NOTREACHED();
}

}  // namespace autofill
