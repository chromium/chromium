// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/search_utils.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "chromeos/components/string_matching/sequence_matcher.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace chromeos {
namespace local_search_service {

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
  return chromeos::string_matching::SequenceMatcher(
             query, text, false /* use_edit_distance */,
             0.1 /*num_matching_blocks_penalty*/)
      .Ratio();
}

bool IsRelevantApproximately(const std::u16string& query,
                             const std::u16string& text,
                             float prefix_threshold,
                             float block_threshold) {
  return (ExactPrefixMatchScore(query, text) >= prefix_threshold ||
          BlockMatchScore(query, text) >= block_threshold);
}

bool CompareResults(const Result& r1, const Result& r2) {
  return r1.score > r2.score;
}

}  // namespace local_search_service
}  // namespace chromeos
