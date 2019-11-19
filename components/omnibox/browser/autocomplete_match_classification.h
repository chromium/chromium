// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

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
TermMatches FindTermMatches(base::string16 find_text,
                            base::string16 text,
                            bool allow_prefix_matching = true,
                            bool allow_mid_word_matching = false);

// Return an ACMatchClassifications structure given the |matches| to highlight.
// |matches| can be retrieved from calling FindTermMatches. |text_length| should
// be the full length (not the length of the truncated text clean returns) of
// the text being classified. It is used to ensure the the trailing
// classification is correct; i.e. if matches end at 20, and text_length is
// greater than 20, ClassifyTermMatches will add a non_match_style
// classification with offset 20. |match_style| and |non_match_style| specify
// the classifications to use for matched and non-matched text.
ACMatchClassifications ClassifyTermMatches(TermMatches matches,
                                           size_t text_length,
                                           int match_style,
                                           int non_match_style);

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_
