// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_scoring_signals_annotator.h"

#include <optional>
#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/history_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

void UrlScoringSignalsAnnotator::AnnotateResult(const AutocompleteInput& input,
                                                AutocompleteResult* result) {
  // Split input text into terms.
  std::u16string lower_raw_string(base::i18n::ToLower(input.text()));
  auto [lower_raw_terms, lower_terms_to_word_starts_offsets] =
      URLIndexPrivateData::GetTermsAndWordStartsOffsets(lower_raw_string);
  for (auto& match : *result) {
    // Skip ineligible matches
    if (!match.IsMlSignalLoggingEligible()) {
      continue;
    }

    // Initialize the scoring signals if needed.
    if (!match.scoring_signals) {
      match.scoring_signals = std::make_optional<ScoringSignals>();
    }

    match.scoring_signals->set_length_of_url(
        match.destination_url.spec().length());
    match.scoring_signals->set_is_host_only(
        history::HistoryMatch::IsHostOnly(match.destination_url));
    match.scoring_signals->set_allowed_to_be_default_match(
        match.allowed_to_be_default_match);

    // Populate query-URL matching signals if not set.
    if (!match.scoring_signals->has_total_url_match_length() &&
        !match.destination_url.is_empty()) {
      PopulateQueryUrlMatchingSignals(
          lower_raw_terms, lower_terms_to_word_starts_offsets,
          match.destination_url, &*match.scoring_signals);
    }
  }
}

void UrlScoringSignalsAnnotator::PopulateQueryUrlMatchingSignals(
    const String16Vector& find_terms,
    const WordStarts& terms_to_word_starts_offsets,
    const GURL& url,
    ScoringSignals* scoring_signals) {
  base::OffsetAdjuster::Adjustments adjustments;
  std::u16string cleaned_up_url =
      bookmarks::CleanUpUrlForMatching(url, &adjustments);

  WordStarts url_word_starts;
  String16Set url_words =
      String16SetFromString16(cleaned_up_url, &url_word_starts);
  TermMatches url_matches = FindTermMatchesForTerms(
      find_terms, terms_to_word_starts_offsets, cleaned_up_url, url_word_starts,
      /*allow_mid_word_matching=*/true);

  const auto filtered_url_matches = ScoredHistoryMatch::FilterUrlTermMatches(
      terms_to_word_starts_offsets, url, url_word_starts, adjustments,
      url_matches);
  const auto url_matching_signals =
      ScoredHistoryMatch::ComputeUrlMatchingSignals(
          terms_to_word_starts_offsets, url, url_word_starts, adjustments,
          filtered_url_matches);

  if (url_matching_signals.first_url_match_position.has_value()) {
    // Not set if there is no URL match.
    scoring_signals->set_first_url_match_position(
        *(url_matching_signals.first_url_match_position));
  }
  if (url_matching_signals.host_match_at_word_boundary.has_value()) {
    // Not set if there is no match in the host.
    scoring_signals->set_host_match_at_word_boundary(
        *(url_matching_signals.host_match_at_word_boundary));
    scoring_signals->set_has_non_scheme_www_match(
        *(url_matching_signals.has_non_scheme_www_match));
  }
  scoring_signals->set_total_url_match_length(
      url_matching_signals.total_url_match_length);
  scoring_signals->set_total_host_match_length(
      url_matching_signals.total_host_match_length);
  scoring_signals->set_total_path_match_length(
      url_matching_signals.total_path_match_length);
  scoring_signals->set_total_query_or_ref_match_length(
      url_matching_signals.total_query_or_ref_match_length);
  scoring_signals->set_num_input_terms_matched_by_url(
      url_matching_signals.num_input_terms_matched_by_url);
}
