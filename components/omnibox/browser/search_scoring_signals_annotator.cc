// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_scoring_signals_annotator.h"

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"

void SearchScoringSignalsAnnotator::AnnotateResult(
    const AutocompleteInput& input,
    AutocompleteResult* result) {
  for (auto& match : *result) {
    // Skip ineligible matches
    if (!match.IsMlSignalLoggingEligible()) {
      continue;
    }

    // Initialize the scoring signals if needed.
    if (!match.scoring_signals) {
      match.scoring_signals = std::make_optional<ScoringSignals>();
    }

    if (!match.scoring_signals->has_search_suggest_relevance()) {
      match.scoring_signals->set_search_suggest_relevance(0);
    }
    UpdateMatchTypeScoringSignals(match, input.text());
  }
}

// static
void SearchScoringSignalsAnnotator::UpdateMatchTypeScoringSignals(
    AutocompleteMatch& match,
    const std::u16string& input_text) {
  match.scoring_signals->set_is_search_suggest_entity(
      match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  match.scoring_signals->set_is_verbatim(match.IsVerbatimType() ||
                                         match.contents == input_text);
  match.scoring_signals->set_is_navsuggest(
      match.type == AutocompleteMatchType::NAVSUGGEST ||
      match.type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED ||
      match.type == AutocompleteMatchType::TILE_NAVSUGGEST ||
      match.type == AutocompleteMatchType::TILE_MOST_VISITED_SITE);
  match.scoring_signals->set_is_search_suggest_tail(
      match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL);
  match.scoring_signals->set_is_answer_suggest(
      match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED);
  match.scoring_signals->set_is_calculator_suggest(
      match.type == AutocompleteMatchType::CALCULATOR);
}
