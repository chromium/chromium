// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_scoring_signals_annotator.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

bool HistoryScoringSignalsAnnotator::IsEligibleMatch(
    const AutocompleteMatch& match) {
  return match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
         match.type == AutocompleteMatchType::HISTORY_URL ||
         match.type == AutocompleteMatchType::HISTORY_TITLE ||
         match.type == AutocompleteMatchType::BOOKMARK_TITLE;
}

void HistoryScoringSignalsAnnotator::PopulateTitleMatchingSignals(
    const String16Vector& input_terms,
    const WordStarts& terms_to_word_starts_offsets,
    const std::u16string& raw_title,
    ScoringSignals* scoring_signals) {
  std::u16string title = bookmarks::CleanUpTitleForMatching(raw_title);
  WordStarts title_word_starts;
  String16VectorFromString16(title, &title_word_starts);
  TermMatches title_matches =
      FindMatchesForTerms(input_terms, terms_to_word_starts_offsets, title,
                          title_word_starts, /*allow_mid_word_matching=*/false);

  // Compute total title match length.
  int32_t total_title_match_length =
      ScoredHistoryMatch::ComputeTotalMatchLength(
          title_matches, terms_to_word_starts_offsets, title_word_starts,
          num_title_words_to_allow_, /*terms_scores=*/nullptr,
          /*score_delta=*/0);
  scoring_signals->set_total_title_match_length(total_title_match_length);

  // Count number of terms matched by title as the number of unique `term_num`s
  // in `term_matches`.
  const auto count_unique_term_nums = [&](const TermMatches& term_matches) {
    std::set<int> unique_term_nums;
    for (const auto& match : term_matches) {
      unique_term_nums.insert(match.term_num);
    }
    return unique_term_nums.size();
  };
  scoring_signals->set_num_input_terms_matched_by_title(
      count_unique_term_nums(title_matches));
}

HistoryScoringSignalsAnnotator::HistoryScoringSignalsAnnotator(
    AutocompleteProviderClient* client)
    : client_(client) {
  num_title_words_to_allow_ = OmniboxFieldTrial::HQPNumTitleWordsToAllow();
}

void HistoryScoringSignalsAnnotator::AnnotateResult(
    const AutocompleteInput& input,
    AutocompleteResult* result) {
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
      URLIndexPrivateData::GetTermsAndStartsOffsets(lower_raw_string);

  for (size_t i = 0; i < result->size(); i++) {
    AutocompleteMatch* match = result->match_at(i);

    // Skip non-URL matches.
    if (!IsEligibleMatch(*match)) {
      continue;
    }

    // Skip this match if it already has history signals.
    if (match->scoring_signals.has_typed_count() &&
        match->scoring_signals.has_total_title_match_length()) {
      continue;
    }

    history::URLRow row;
    // Return if no URL row found.
    if (url_db->GetRowForURL(match->destination_url, &row) == 0) {
      return;
    }

    // Populate scoring signals.
    if (!match->scoring_signals.has_typed_count()) {
      match->scoring_signals.set_typed_count(row.typed_count());
      match->scoring_signals.set_visit_count(row.visit_count());
      match->scoring_signals.set_elapsed_time_last_visit_secs(
          (base::Time::Now() - row.last_visit()).InSeconds());
    }
    if (!match->scoring_signals.has_total_title_match_length()) {
      PopulateTitleMatchingSignals(lower_raw_terms,
                                   lower_terms_to_word_starts_offsets,
                                   row.title(), &match->scoring_signals);
    }
  }
}