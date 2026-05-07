// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace accessibility_annotator {
struct MemorySearchResults;
}

namespace autofill {

class BrowserAutofillManager;
class FormData;
class FormFieldData;

// Manager for the accessibility annotator search feature. It handles queries
// to the AccessibilityQueryService and manages session-based metrics.
// Owned by `BrowserAutofillManager`, its lifetime is tied to it.
class AtMemoryManager {
 public:
  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<Suggestion>,
                                   AutofillSuggestionTriggerSource)>;

  explicit AtMemoryManager(BrowserAutofillManager* manager);

  AtMemoryManager(const AtMemoryManager&) = delete;
  AtMemoryManager& operator=(const AtMemoryManager&) = delete;

  ~AtMemoryManager();

  // Called when suggestions are shown. The manager initiates an @memory
  // session if the `trigger_source` is an @memory one.
  // TODO(crbug.com/507770024): Rename to OnSuggestionsShown.
  void OnPopupShown(AutofillSuggestionTriggerSource trigger_source,
                    UpdateSuggestionsCallback update_callback);

  // Called when the user types in the filter/search bar. Returns true if
  // handled by the manager (i.e., the current session is an @memory one).
  bool OnFilterChanged(const std::u16string& filter);

  // Called when the user has explicitly submitted the search. Returns true if
  // handled by the manager (i.e., the current session is an @memory one).
  bool OnSearchSubmitted(const std::u16string& filter);

  // Called when suggestions are hidden.
  void OnPopupHidden();

  // Fills or previews the selected search result.
  void FillOrPreviewSearchResult(mojom::ActionPersistence action_persistence,
                                 const FormData& form,
                                 const FormFieldData& field,
                                 const Suggestion& suggestion);

  // Returns true if a search is currently in progress.
  bool IsSearching() const;

 private:
  // Executes the search query. `full_search` is true if the search was
  // explicitly submitted by the user, and false for incremental search.
  void ExecuteQuery(const std::u16string& filter, bool full_search);

  // Callback handler for the search query. `query` is the original search
  // string. `full_search` is true if the search was explicitly submitted by
  // the user (e.g. pressing Enter), and false if it was an incremental search
  // as the user types. `result` contains the search results.
  void OnSearchResultsReceived(
      const std::u16string& query,
      bool full_search,
      accessibility_annotator::MemorySearchResults result);

  // Creates a suggestion to display when the query is not supported.
  Suggestion CreateUnsupportedQuerySuggestion(const std::u16string& query);

  // Fills the unmasked IBAN value after fetching it.
  void FillIban(const Suggestion::AtMemoryPayload::Identifier& identifier,
                const FormData& form,
                const FormFieldData& field,
                const Suggestion& suggestion);

  // Fills the unmasked credit card value after fetching it.
  void FillCreditCard(const Suggestion::AtMemoryPayload::Identifier& identifier,
                      const FormData& form,
                      const FormFieldData& field,
                      const Suggestion& suggestion);

  const raw_ptr<BrowserAutofillManager> owner_;

  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;

  UpdateSuggestionsCallback update_callback_;

  std::unique_ptr<AtMemoryFunnelMetrics> at_memory_funnel_metrics_;

  // Flag indicating that a search query is in progress.
  bool is_searching_ = false;
  // Flag to distinguish if the ongoing query is a full search (explicit submit)
  // or an incremental one (as-you-type).
  bool is_full_search_running_ = false;

  // Factory for search queries, used to identify currently active query and
  // discard the old ones.
  base::WeakPtrFactory<AtMemoryManager> query_weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemoryManager> fill_weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
