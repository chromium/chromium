// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_embeddings_provider.h"

#include <string>
#include <tuple>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/search_engines/template_url.h"

namespace {
constexpr int kMaxScore = 1000;
}

HistoryEmbeddingsProvider::HistoryEmbeddingsProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS),
      client_(client) {
  CHECK(client_);
}

HistoryEmbeddingsProvider::~HistoryEmbeddingsProvider() = default;

void HistoryEmbeddingsProvider::Start(const AutocompleteInput& input,
                                      bool minimal_changes) {
  if (!history_embeddings::IsHistoryEmbeddingEnabled()) {
    return;
  }

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, starter_pack_engine] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client_->GetTemplateURLService());
  starter_pack_engine_ = starter_pack_engine;

  matches_.clear();

  history_embeddings::HistoryEmbeddingsService* service =
      client_->GetHistoryEmbeddingsService();
  CHECK(service);
  service->Search(
      base::UTF16ToUTF8(adjusted_input.text()), {}, provider_max_matches_,
      base::BindOnce(&HistoryEmbeddingsProvider::OnReceivedSearchResult,
                     weak_factory_.GetWeakPtr(), input.text()));
}

void HistoryEmbeddingsProvider::Stop(bool clear_cached_results,
                                     bool due_to_user_inactivity) {
  // TODO(b/333770460): Once `HistoryEmbeddingsService` has a stop API, we
  //   should call it here.
}

void HistoryEmbeddingsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // TODO(b/333770460): Should delete the entry in the history & embeddings DBs.
  //   Should also set `match.deletable = true`.
}

void HistoryEmbeddingsProvider::OnReceivedSearchResult(
    std::u16string input_text,
    history_embeddings::SearchResult result) {
  for (const history_embeddings::ScoredUrlRow& scored_url_row :
       result.scored_url_rows) {
    AutocompleteMatch match(this, scored_url_row.scored_url.score * kMaxScore,
                            false, AutocompleteMatchType::HISTORY_BODY);
    match.destination_url = scored_url_row.row.url();

    match.contents =
        AutocompleteMatch::SanitizeString(scored_url_row.row.title());
    match.contents_class = ClassifyTermMatches(
        FindTermMatches(input_text, match.contents), match.contents.size(),
        ACMatchClassification::MATCH, ACMatchClassification::NONE);

    match.description = base::UTF8ToUTF16(scored_url_row.row.url().spec());
    match.description_class = ClassifyTermMatches(
        FindTermMatches(input_text, match.description),
        match.description.size(), ACMatchClassification::MATCH,
        ACMatchClassification::URL);

    if (starter_pack_engine_) {
      match.keyword = starter_pack_engine_->keyword();
      match.transition = ui::PAGE_TRANSITION_KEYWORD;
    }

    matches_.push_back(match);
  }
  NotifyListeners(!matches_.empty());
}
