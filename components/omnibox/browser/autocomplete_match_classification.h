// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

// Finds the matches for |find_text| in |text|, classifies those matches,
// merges those classifications with |original_class|, and returns the merged
// classifications.
// If |text_is_search_query| is false, matches are classified as MATCH, and
// non-matches are classified as NONE. Otherwise, if |text_is_search_query| is
// true, matches are classified as NONE, and non-matches are classified as
// MATCH. This is done to mimic the behavior of SearchProvider which decorates
// matches according to the approach used by Google Suggest.
// |find_text| and |text| will be lowercased.
//
//   For example, given
//     |find_text| is "sp new",
//     |text| is "Sports and News at sports.somesite.com - visit us!",
//     |text_is_search_query| is false, and
//     |original_class| is {{0, NONE}, {19, URL}, {38, NONE}} (marking
//     "sports.somesite.com" as a URL),
//   Then this will return
//     {{0, MATCH}, {2, NONE}, {11, MATCH}, {14, NONE}, {19, URL|MATCH},
//     {21, URL}, {38, NONE}}; i.e.,
//     "Sports and News at sports.somesite.com - visit us!"
//      ^ ^        ^  ^    ^ ^                ^
//      0 2        11 14  19 21               38
//      M N        M  N  U|M U                N
//
//   For example, given
//     |find_text| is "canal",
//     |text| is "panama canal",
//     |text_is_search_query| is true, and
//     |original_class| is {{0, NONE}},
//   Then this will return
//     {{0,MATCH}, {7, NONE}}; i.e.,
//     "panama canal"
//      ^      ^
//      0 M    7 N
ACMatchClassifications ClassifyAllMatchesInString(
    const std::u16string& find_text,
    const std::u16string& text,
    const bool text_is_search_query,
    const ACMatchClassifications& original_class = ACMatchClassifications());

// Cleans |text|, splits |find_text| into terms by breaking on whitespaces and
// most symbols, looks for those terms in cleaned |text|, and returns the
// matched terms sorted, deduped, and possibly filtered-by-word-boundary.
// If |allow_prefix_matching| is true, and |find_text| is an exact prefix
// (ignoring case but considering symbols) of |text|, then only a single term
// representing the prefix will be returned. E.g., for |find_text| "how to tie"
// and |text| "how to tie a tie", this will return "[how to tie] a tie". On the
// other hand, for |find_text| "to tie", this will return "how [to] [tie] a
// [tie]".
// If |allow_mid_word_matching| is false, the returned terms will be
// filtered-by-word-boundary. E.g., for |find_text| "ho to ie", |text|
// "how to tie a tie", and |allow_mid_word_matching| false, this will return
// "[ho]w [to] tie a tie". On the other hand, for |allow_mid_word_matching|
// true, this will return "[ho]w [to] t[ie] a t[ie]."
TermMatches FindTermMatches(std::u16string find_text,
                            std::u16string text,
                            bool allow_prefix_matching = true,
                            bool allow_mid_word_matching = false);

// A utility function called by `FindTermMatches` to find valid matches in text
// for the given terms. Matched terms are sorted, deduped, and possibly
// filtered-by-word-boundary. If `allow_mid_word_matching` is false, the
// returned terms will be filtered-by-word-boundary. E.g., for `find_text` "ho
// to ie", `text` "how to tie a tie", and `allow_mid_word_matching` false, this
// will return "[ho]w [to] tie a tie". On the other hand, for
// |allow_mid_word_matching| true, this will return "[ho]w [to] t[ie] a t[ie]."
TermMatches FindTermMatchesForTerms(const String16Vector& find_terms,
                                    const WordStarts& find_terms_word_starts,
                                    const std::u16string& cleaned_text,
                                    const WordStarts& text_word_starts,
                                    bool allow_mid_word_matching = false);

// Return an ACMatchClassifications structure given the |matches| to highlight.
// |matches| can be retrieved from calling FindTermMatches. |text_length| should
// be the full length (not the length of the truncated text clean returns) of
// the text being classified. It is used to ensure the trailing classification
// is correct; i.e. if matches end at 20, and text_length is greater than 20,
// ClassifyTermMatches will add a non_match_style classification with offset 20.
// |match_style| and |non_match_style| specify the classifications to use for
// matched and non-matched text.
ACMatchClassifications ClassifyTermMatches(TermMatches matches,
                                           size_t text_length,
                                           int match_style,
                                           int non_match_style);

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_
