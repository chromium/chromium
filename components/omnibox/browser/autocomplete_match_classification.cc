// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_match_classification.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "in_memory_url_index_types.h"

namespace {

std::u16string clean(std::u16string text) {
  const size_t kMaxTextLength = 2000;
  return base::i18n::ToLower(text.substr(0, kMaxTextLength));
}

}  // namespace

ACMatchClassifications ClassifyAllMatchesInString(
    const std::u16string& find_text,
    const std::u16string& text,
    const bool text_is_search_query,
    const ACMatchClassifications& original_class) {
  DCHECK(!find_text.empty());

  if (text.empty()) {
    return original_class;
  }

  TermMatches term_matches = FindTermMatches(find_text, text);

  ACMatchClassifications classifications;
  if (text_is_search_query) {
    classifications = ClassifyTermMatches(term_matches, text.size(),
                                          ACMatchClassification::NONE,
                                          ACMatchClassification::MATCH);
  } else {
    classifications = ClassifyTermMatches(term_matches, text.size(),
                                          ACMatchClassification::MATCH,
                                          ACMatchClassification::NONE);
  }

  return AutocompleteMatch::MergeClassifications(original_class,
                                                 classifications);
}

TermMatches FindTermMatches(std::u16string find_text,
                            std::u16string text,
                            bool allow_prefix_matching,
                            bool allow_mid_word_matching) {
  find_text = clean(find_text);
  text = clean(text);

  if (find_text.empty())
    return {};

  if (allow_prefix_matching &&
      base::StartsWith(text, find_text, base::CompareCase::SENSITIVE))
    return {{0, 0, find_text.length()}};

  String16Vector find_terms = String16VectorFromString16(find_text, nullptr);
  WordStarts word_starts;
  // `word_starts` is unused if `allow_mid_word_matching` is true.
  if (!allow_mid_word_matching) {
    String16VectorFromString16(text, &word_starts);
  }
  return FindTermMatchesForTerms(find_terms, WordStarts(find_terms.size(), 0),
                                 text, word_starts, allow_mid_word_matching);
}

TermMatches FindTermMatchesForTerms(const String16Vector& find_terms,
                                    const WordStarts& find_terms_word_starts,
                                    const std::u16string& cleaned_text,
                                    const WordStarts& text_word_starts,
                                    bool allow_mid_word_matching) {
  TermMatches matches = MatchTermsInString(find_terms, cleaned_text);
  matches = SortMatches(matches);
  matches = DeoverlapMatches(matches);

  if (allow_mid_word_matching)
    return matches;

  return ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      matches, find_terms_word_starts, text_word_starts, 0, std::string::npos);
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
