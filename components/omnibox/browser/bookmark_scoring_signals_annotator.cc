// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/bookmark_scoring_signals_annotator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/query_parser/query_parser.h"
#include "components/query_parser/snippet.h"

using ::bookmarks::TitledUrlIndex;

BookmarkScoringSignalsAnnotator::BookmarkScoringSignalsAnnotator(
    AutocompleteProviderClient* client) {
  bookmark_model_ = client ? client->GetBookmarkModel() : nullptr;
}

void BookmarkScoringSignalsAnnotator::AnnotateResult(
    const AutocompleteInput& input,
    AutocompleteResult* result) {
  if (!bookmark_model_) {
    return;
  }

  const std::u16string query =
      base::i18n::ToLower(TitledUrlIndex::Normalize(input.text()));
  if (query.empty()) {
    return;
  }

  // Use a `QueryParser` to fill in match positions.
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      query, query_parser::MatchingAlgorithm::DEFAULT, &query_nodes);

  for (auto& match : *result) {
    // Skip ineligible matches.
    if (!match.IsMlSignalLoggingEligible()) {
      continue;
    }

    // Initialize the scoring signals if needed.
    if (!match.scoring_signals) {
      match.scoring_signals = std::make_optional<ScoringSignals>();
    }

    // Skip this match if it already has bookmark signals.
    if (match.scoring_signals->has_num_bookmarks_of_url()) {
      continue;
    }

    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
        nodes = bookmark_model_->GetNodesByURL(match.destination_url);

    for (const bookmarks::BookmarkNode* node : nodes) {
      const std::u16string lower_title = base::i18n::ToLower(
          TitledUrlIndex::Normalize(node->GetTitledUrlNodeTitle()));
      query_parser::QueryWordVector title_words;
      query_parser::QueryParser::ExtractQueryWords(lower_title, &title_words);

      query_parser::Snippet::MatchPositions title_matches;
      for (const auto& query_node : query_nodes) {
        const bool has_title_matches =
            query_node->HasMatchIn(title_words, &title_matches);
        if (!has_title_matches) {
          continue;
        }
        query_parser::QueryParser::SortAndCoalesceMatchPositions(
            &title_matches);
      }

      if (!title_matches.empty()) {
        // Keep the minimum of title match positions of different bookmarks.
        if (match.scoring_signals->has_first_bookmark_title_match_position()) {
          int min_first_pos = std::min(
              match.scoring_signals->first_bookmark_title_match_position(),
              static_cast<int>(title_matches[0].first));
          match.scoring_signals->set_first_bookmark_title_match_position(
              min_first_pos);
        } else {
          match.scoring_signals->set_first_bookmark_title_match_position(
              title_matches[0].first);
        }
      }

      // Keep the maximum of title match lengths.
      int max_len =
          std::max(match.scoring_signals->total_bookmark_title_match_length(),
                   bookmarks::GetTotalTitleMatchLength(title_matches));
      match.scoring_signals->set_total_bookmark_title_match_length(max_len);
    }
    match.scoring_signals->set_num_bookmarks_of_url(nodes.size());
  }
}
