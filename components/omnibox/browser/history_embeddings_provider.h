// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_EMBEDDINGS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_EMBEDDINGS_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class TemplateURL;
class AutocompleteInput;
struct AutocompleteMatch;

class HistoryEmbeddingsProvider : public AutocompleteProvider {
 public:
  explicit HistoryEmbeddingsProvider(AutocompleteProviderClient* client);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

 private:
  friend class FakeHistoryEmbeddingsProvider;

  ~HistoryEmbeddingsProvider() override;

  // Callback for querying `HistoryEmbeddingsService::Search()`.
  void OnReceivedSearchResult(std::u16string input_text,
                              history_embeddings::SearchResult result);

  // Assigned in `Start()`, accessed in `OnReceivedSearchResult()` which is only
  // called asyncly after `Start()`, so it's never null when accessed.
  raw_ptr<const TemplateURL> starter_pack_engine_;

  // Never null. Owned by `AutocompleteController`, which also owns this
  // provider, ensuring `client_` outlives this.
  raw_ptr<AutocompleteProviderClient> client_;

  base::WeakPtrFactory<HistoryEmbeddingsProvider> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_EMBEDDINGS_PROVIDER_H_
