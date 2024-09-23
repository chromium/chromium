// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_scoring_signals_annotator.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

HistoryScoringSignalsAnnotator::HistoryScoringSignalsAnnotator(
    AutocompleteProviderClient* client)
    : client_(client) {
  num_title_words_to_allow_ = OmniboxFieldTrial::HQPNumTitleWordsToAllow();
}

void HistoryScoringSignalsAnnotator::AnnotateResult(
    const AutocompleteInput& input,
    AutocompleteResult* result) {
  TRACE_EVENT0("omnibox", "HistoryScoringSignalsAnnotator::AnnotateResult");

  history::HistoryService* const history_service = client_->GetHistoryService();
  if (!history_service) {
    return;
  }
  // Get the in-memory URL database.
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db) {
    return;
  }

  // Split input text into terms.
  std::u16string lower_raw_string(base::i18n::ToLower(input.text()));
  auto [lower_raw_terms, lower_terms_to_word_starts_offsets] =
      URLIndexPrivateData::GetTermsAndWordStartsOffsets(lower_raw_string);

  for (auto& match : *result) {
    // Skip ineligible matches.
    if (!match.IsMlSignalLoggingEligible()) {
      continue;
    }

    history::URLRow url_info;
    if (AutocompleteMatch::IsSearchType(match.type)) {
      if (!OmniboxFieldTrial::GetMLConfig()
               .enable_history_scoring_signals_annotator_for_searches) {
        continue;
      }

      // Initialize the scoring signals if needed.
      if (!match.scoring_signals) {
        match.scoring_signals = std::make_optional<ScoringSignals>();
      }

      // Skip this match if it already has history signals.
      if (match.scoring_signals->has_elapsed_time_last_visit_secs()) {
        continue;
      }

      // Skip this match if no relevant history data was found.
      if (!url_db->GetAggregateURLDataForKeywordSearchTerm(match.contents,
                                                           &url_info)) {
        continue;
      }

      // Populate scoring signals.
      match.scoring_signals->set_typed_count(url_info.typed_count());
      match.scoring_signals->set_visit_count(url_info.visit_count());
      match.scoring_signals->set_elapsed_time_last_visit_secs(
          (base::Time::Now() - url_info.last_visit()).InSeconds());
    } else {
      // Initialize the scoring signals if needed.
      if (!match.scoring_signals) {
        match.scoring_signals = std::make_optional<ScoringSignals>();
      }

      // Skip this match if it already has history signals.
      if (match.scoring_signals->has_typed_count() &&
          match.scoring_signals->has_total_title_match_length()) {
        continue;
      }

      // Skip this match if no URL row found.
      if (url_db->GetRowForURL(match.destination_url, &url_info) == 0) {
        continue;
      }

      // Populate scoring signals.
      if (!match.scoring_signals->has_typed_count()) {
        match.scoring_signals->set_typed_count(url_info.typed_count());
        match.scoring_signals->set_visit_count(url_info.visit_count());
        match.scoring_signals->set_elapsed_time_last_visit_secs(
            (base::Time::Now() - url_info.last_visit()).InSeconds());
      }

      // Populate title-match scoring signals.
      if (!match.scoring_signals->has_total_title_match_length()) {
        PopulateTitleMatchingSignals(lower_raw_terms,
                                     lower_terms_to_word_starts_offsets,
                                     url_info.title(), &*match.scoring_signals);
      }
    }
  }
}

void HistoryScoringSignalsAnnotator::PopulateTitleMatchingSignals(
    const String16Vector& input_terms,
    const WordStarts& terms_to_word_starts_offsets,
    const std::u16string& raw_title,
    ScoringSignals* scoring_signals) {
  std::u16string title = bookmarks::CleanUpTitleForMatching(raw_title);
  WordStarts title_word_starts;
  String16VectorFromString16(title, &title_word_starts);
  TermMatches title_matches = FindTermMatchesForTerms(
      input_terms, terms_to_word_starts_offsets, title, title_word_starts,
      /*allow_mid_word_matching=*/false);

  // Compute total title match length.
  size_t total_title_match_length = ScoredHistoryMatch::ComputeTotalMatchLength(
      terms_to_word_starts_offsets, title_matches, title_word_starts,
      num_title_words_to_allow_);
  scoring_signals->set_total_title_match_length(total_title_match_length);

  scoring_signals->set_num_input_terms_matched_by_title(
      ScoredHistoryMatch::CountUniqueMatchTerms(title_matches));
}
