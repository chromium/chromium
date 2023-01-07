// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCAL_HISTORY_ZERO_SUGGEST_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCAL_HISTORY_ZERO_SUGGEST_PROVIDER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;

namespace history {
class QueryResults;
}  // namespace history

// Autocomplete provider for on-focus zero-prefix query suggestions from local
// history when Google is the default search engine.
class LocalHistoryZeroSuggestProvider : public AutocompleteProvider {
 public:
  // Creates and returns an instance of this provider.
  static LocalHistoryZeroSuggestProvider* Create(
      AutocompleteProviderClient* client,
      AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LocalHistoryZeroSuggestProviderTest, Input);

  LocalHistoryZeroSuggestProvider(AutocompleteProviderClient* client,
                                  AutocompleteProviderListener* listener);
  ~LocalHistoryZeroSuggestProvider() override;
  LocalHistoryZeroSuggestProvider(const LocalHistoryZeroSuggestProvider&) =
      delete;
  LocalHistoryZeroSuggestProvider& operator=(
      const LocalHistoryZeroSuggestProvider&) = delete;

  // Queries the keyword search terms table of the in-memory URLDatabase for the
  // recent search terms submitted to the default search provider.
  void QueryURLDatabase(const AutocompleteInput& input);

  // Called when the query results from HistoryService::QueryHistory are ready.
  // Deletes URLs in |results| that would generate |suggestion|. |query_time| is
  // the time HistoryService was queried.
  void OnHistoryQueryResults(const std::u16string& suggestion,
                             const base::TimeTicks& query_time,
                             history::QueryResults results);

  // The maximum number of matches to return.
  const size_t max_matches_;

  // Client for accessing TemplateUrlService, prefs, etc.
  const raw_ptr<AutocompleteProviderClient> client_;

  // Used for the async tasks querying the HistoryService.
  base::CancelableTaskTracker history_task_tracker_;

  base::WeakPtrFactory<LocalHistoryZeroSuggestProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCAL_HISTORY_ZERO_SUGGEST_PROVIDER_H_
