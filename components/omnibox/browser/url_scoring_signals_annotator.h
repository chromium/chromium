// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_URL_SCORING_SIGNALS_ANNOTATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_URL_SCORING_SIGNALS_ANNOTATOR_H_

#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

class AutocompleteInput;
class AutocompleteResult;
class GURL;

// Annotates autocomplete URL suggestion candidates with scoring signals derived
// from URLs and query-URL matching patterns, including: `is_host_only`,
// `length_of_url`, `first_url_match_position`, `host_match_at_word_boundary`,
// `has_non_scheme_www_match`, `total_url_match_length`,
// `total_host_match_length`, `total_path_match_length`,
// `total_query_or_ref_match_length`, `num_input_terms_matched_by_url`, and
// `allowed_to_be_default_match`.
class UrlScoringSignalsAnnotator : public AutocompleteScoringSignalsAnnotator {
 public:
  UrlScoringSignalsAnnotator() = default;
  UrlScoringSignalsAnnotator(const UrlScoringSignalsAnnotator&) = delete;
  UrlScoringSignalsAnnotator& operator=(const UrlScoringSignalsAnnotator&) =
      delete;
  ~UrlScoringSignalsAnnotator() override = default;

  // Annotates the URL suggestions of the autocomplete result.
  void AnnotateResult(const AutocompleteInput& input,
                      AutocompleteResult* result) override;

 private:
  void PopulateQueryUrlMatchingSignals(
      const String16Vector& find_terms,
      const WordStarts& terms_to_word_starts_offsets,
      const GURL& gurl,
      ScoringSignals* scoring_signals);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_URL_SCORING_SIGNALS_ANNOTATOR_H_
