// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_EMBEDDINGS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_EMBEDDINGS_PROVIDER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/history_provider.h"

class AutocompleteProviderClient;
class TemplateURL;

class HistoryEmbeddingsProvider : public HistoryProvider {
 public:
  HistoryEmbeddingsProvider(AutocompleteProviderClient* client,
                            AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

 private:
  friend class FakeHistoryEmbeddingsProvider;

  ~HistoryEmbeddingsProvider() override;

  // Callback for querying `HistoryEmbeddingsService::Search()`.
  void OnReceivedSearchResult(history_embeddings::SearchResult search_result);

  // Creates a `HISTORY_EMBEDDINGS` match.
  AutocompleteMatch CreateMatch(
      const history_embeddings::ScoredUrlRow& scored_url_row);

  // Creates a HISTORY_EMBEDDINGS_ANSWER` match. `scored_url_row` is the
  // `ScoredUrlRow` `answerer_result` was derived from; `match` is the match
  // created by `CreateMatch()` for `scored_url_row`.
  std::optional<AutocompleteMatch> CreateAnswerMatch(
      const history_embeddings::AnswererResult& answerer_result,
      const history_embeddings::ScoredUrlRow& scored_url_row,
      const AutocompleteMatch& match);

  // Creates a HISTORY_EMBEDDINGS_ANSWER` match.
  AutocompleteMatch CreateAnswerMatchHelper(
      int score,
      const std::u16string& history_embeddings_answer_header_text,
      const std::u16string& description);

  // Assigned in `Start()`, accessed in `OnReceivedSearchResult()` which is only
  // called asyncly after `Start()`, so it's never null when accessed.
  raw_ptr<const TemplateURL> starter_pack_engine_;

  // The last input sent to `HistoryEmbeddingsService::Search()`.
  AutocompleteInput input_;

  base::WeakPtrFactory<HistoryEmbeddingsProvider> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_EMBEDDINGS_PROVIDER_H_
