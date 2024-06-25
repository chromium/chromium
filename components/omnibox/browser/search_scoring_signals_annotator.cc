// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_scoring_signals_annotator.h"

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

void SearchScoringSignalsAnnotator::AnnotateResult(
    const AutocompleteInput& input,
    AutocompleteResult* result) {
  for (auto& match : *result) {
    // Skip ineligible matches
    if (!IsEligibleMatch(match) &&
        !AutocompleteMatch::IsSearchType(match.type)) {
      continue;
    }

    // Initialize the scoring signals if needed.
    if (!match.scoring_signals) {
      match.scoring_signals = std::make_optional<ScoringSignals>();
    }

    if (!match.scoring_signals->has_search_suggest_relevance()) {
      match.scoring_signals->set_search_suggest_relevance(0);
    }
    match.scoring_signals->set_is_search_suggest_entity(
        match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  }
}
