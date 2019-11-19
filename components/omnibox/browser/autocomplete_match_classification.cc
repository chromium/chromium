// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_match_classification.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "in_memory_url_index_types.h"

namespace {

base::string16 clean(base::string16 text) {
  const size_t kMaxTextLength = 2000;
  return base::i18n::ToLower(text.substr(0, kMaxTextLength));
}

}  // namespace

TermMatches FindTermMatches(base::string16 find_text,
                            base::string16 text,
                            bool allow_prefix_matching,
                            bool allow_mid_word_matching) {
  find_text = clean(find_text);
  text = clean(text);

  if (allow_prefix_matching &&
      base::StartsWith(text, find_text, base::CompareCase::SENSITIVE))
    return {{0, 0, find_text.length()}};

  String16Vector find_terms =
      String16VectorFromString16(find_text, false, NULL);

  TermMatches matches = MatchTermsInString(find_terms, text);
  matches = SortMatches(matches);
  matches = DeoverlapMatches(matches);

  if (allow_mid_word_matching)
    return matches;

  WordStarts word_starts;
  String16VectorFromString16(text, false, &word_starts);
  return ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      matches, WordStarts(find_terms.size(), 0), word_starts, 0,
      std::string::npos);
}

ACMatchClassifications ClassifyTermMatches(TermMatches matches,
                                           size_t text_length,
                                           int match_style,
                                           int non_match_style) {
  ACMatchClassifications classes;
  if (matches.empty()) {
    if (text_length)
      classes.push_back(ACMatchClassification(0, non_match_style));
    return classes;
  }
  if (matches[0].offset)
    classes.push_back(ACMatchClassification(0, non_match_style));
  size_t match_count = matches.size();
  for (size_t i = 0; i < match_count;) {
    size_t offset = matches[i].offset;
    classes.push_back(ACMatchClassification(offset, match_style));
    // Skip all adjacent matches.
    do {
      offset += matches[i].length;
      ++i;
    } while ((i < match_count) && (offset == matches[i].offset));
    if (offset < text_length)
      classes.push_back(ACMatchClassification(offset, non_match_style));
  }
  return classes;
}
