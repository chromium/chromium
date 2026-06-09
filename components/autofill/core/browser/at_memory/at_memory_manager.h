// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"

namespace accessibility_annotator {
struct MemorySearchResults;
}

namespace autofill {

class BrowserAutofillManager;

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
                    bool is_context_secure,
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
                                 const FormGlobalId& form_id,
                                 const FieldGlobalId& field_id,
                                 const Suggestion& suggestion);

  // Returns true if a search is currently in progress.
  bool IsSearching() const;

  // Appends the personal context notice to the suggestions if necessary.
  void MaybeAppendPersonalContextNotice(
      std::vector<Suggestion>& suggestions) const;

 private:
  // Executes the search query.
  void ExecuteQuery(const std::u16string& filter);

  // Callback handler for the search query. `query` is the original search
  // string. `result` contains the search results.
  void OnSearchResultsReceived(
      const std::u16string& query,
      accessibility_annotator::MemorySearchResults result);

  // Creates a suggestion to display when the query is not supported.
  Suggestion CreateUnsupportedQuerySuggestion(const std::u16string& query);

  // Creates the search affordance suggestion.
  Suggestion CreateSearchAffordanceSuggestion(std::u16string query);

  // Cancels any pending search queries and resets searching states.
  void CancelPendingQueries();

  // Sends the given suggestions to the UI.
  void SendSuggestions(std::vector<Suggestion> suggestions);

  // Clears all currently shown suggestions in the UI.
  void ClearSuggestions();

  // Fills the unmasked IBAN value after fetching it.
  void FillIban(const Suggestion::AtMemoryPayload::Identifier& identifier,
                const FormGlobalId& form_id,
                const FieldGlobalId& field_id,
                const Suggestion& suggestion,
                std::unique_ptr<AtMemoryFunnelMetrics> metrics);

  // Fills the unmasked credit card value after fetching it.
  void FillCreditCard(const Suggestion::AtMemoryPayload::Identifier& identifier,
                      const FormGlobalId& form_id,
                      const FieldGlobalId& field_id,
                      const Suggestion& suggestion,
                      std::unique_ptr<AtMemoryFunnelMetrics> metrics);

  // Fills the unmasked AutofillAI value after fetching it.
  void FillSensitiveAutofillAiData(
      const EntityInstance::EntityId& entity_id,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      const AtMemoryDataType& data_type,
      std::unique_ptr<AtMemoryFunnelMetrics> metrics);

  // Callback handler when the unmasked AutofillAI entity has been fetched.
  void OnAutofillAiFetched(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      const AtMemoryDataType& data_type,
      std::unique_ptr<AtMemoryFunnelMetrics> metrics,
      base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
          result,
      bool reauth_attempted);

  const raw_ptr<BrowserAutofillManager> owner_;

  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;

  UpdateSuggestionsCallback update_callback_;

  std::unique_ptr<AtMemoryFunnelMetrics> at_memory_funnel_metrics_;

  // Indicates whether the current tab and the form uses a secure connection.
  bool is_context_secure_ = false;
  // Flag indicating that a search query is in progress.
  bool is_searching_ = false;

  // Factory for search queries, used to identify currently active query and
  // discard the old ones.
  base::WeakPtrFactory<AtMemoryManager> query_weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemoryManager> fill_weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
