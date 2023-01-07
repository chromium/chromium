// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/search_utils.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "chromeos/ash/components/string_matching/sequence_matcher.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace ash::local_search_service {

float ExactPrefixMatchScore(const std::u16string& query,
                            const std::u16string& text) {
  const size_t query_len = query.size();
  if (query_len == 0)
    return 0;

  const size_t text_len = text.size();
  if (query_len > text_len)
    return 0;

  if (text.substr(0, query_len) == query)
    return static_cast<float>(query_len) / text_len;

  return 0.0;
}

float BlockMatchScore(const std::u16string& query, const std::u16string& text) {
  return string_matching::SequenceMatcher(query, text,
                                          0.1 /*num_matching_blocks_penalty*/)
      .Ratio();
}

float RelevanceCoefficient(const std::u16string& query,
                           const std::u16string& text,
                           float prefix_threshold,
                           float block_threshold) {
  const float prefix_score = ExactPrefixMatchScore(query, text);
  const float block_score = BlockMatchScore(query, text);
  bool is_relevant =
      prefix_score >= prefix_threshold || block_score >= block_threshold;
  return is_relevant ? std::max(prefix_score, block_score) : 0;
}

bool CompareResults(const Result& r1, const Result& r2) {
  return r1.score > r2.score;
}

}  // namespace ash::local_search_service
