// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_string_filtering_util.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/strings/levenshtein_distance.h"

namespace autofill {

namespace {

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

bool FuzzyMatchesOrderedTokens(std::u16string_view normalized_target,
                               std::u16string_view normalized_filter) {
  if (normalized_filter.empty()) {
    return true;
  }

  const std::vector<std::u16string> query_tokens =
      TokenizeString(normalized_filter);
  if (query_tokens.empty()) {
    return true;
  }

  const std::vector<std::u16string> target_tokens =
      TokenizeString(normalized_target);
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

}  // namespace autofill
