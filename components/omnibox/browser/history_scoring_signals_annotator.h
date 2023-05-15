// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_SCORING_SIGNALS_ANNOTATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_SCORING_SIGNALS_ANNOTATOR_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class AutocompleteResult;

// History scoring signals annotator for annotating URL suggestions in
// the autocomplete result with signals derived from history, including:
// `typed_count`, `visit_count`, `elapsed_time_last_visit_secs`,
// `total_title_match_length`, and `num_input_terms_matched_by_title`.
//
// Currently, only synchronously looks up URLs from the in-memory URL DB.
// Skips suggestions that already have history signals.
//
// Can annotate eligible URL suggesions from various providers, including
// history, bookmark, and shortcut suggestions.
class HistoryScoringSignalsAnnotator
    : public AutocompleteScoringSignalsAnnotator {
 public:
  explicit HistoryScoringSignalsAnnotator(AutocompleteProviderClient* client);
  HistoryScoringSignalsAnnotator(const HistoryScoringSignalsAnnotator&) =
      delete;
  HistoryScoringSignalsAnnotator& operator=(
      const HistoryScoringSignalsAnnotator&) = delete;
  ~HistoryScoringSignalsAnnotator() override = default;

  // Annotates the URL suggestions of the autocomplete result.
  void AnnotateResult(const AutocompleteInput& input,
                      AutocompleteResult* result) override;

 private:
  // Populates signals based on the matching strings between the input text and
  // page title.
  void PopulateTitleMatchingSignals(
      const String16Vector& input_terms,
      const WordStarts& terms_to_word_starts_offsets,
      const std::u16string& title,
      ScoringSignals* scoring_signals);

  raw_ptr<AutocompleteProviderClient> client_;

  // The number of title words examined when computing matching signals.
  // Words beyond this number are ignored.
  // Currently, set by this feature param: http://shortn/_kXE02KOqFR
  int num_title_words_to_allow_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_SCORING_SIGNALS_ANNOTATOR_H_
