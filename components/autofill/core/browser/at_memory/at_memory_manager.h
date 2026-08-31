// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace history {
class HistoryService;
}

namespace autofill {

struct AtMemorySearchState;
struct MemorySearchResults;
class AutofillClient;
class BrowserAutofillManager;

// Manager for the AtMemory feature. It handles queries to the
// `AtMemoryQueryService` and manages session-based metrics. Owned by
// `AutofillClient`, its lifetime is tied to it.
class AtMemoryManager {
 public:
  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<Suggestion>,
                                   AutofillSuggestionTriggerSource)>;

  AtMemoryManager(AutofillClient* client,
                  history::HistoryService* history_service);

  AtMemoryManager(const AtMemoryManager&) = delete;
  AtMemoryManager& operator=(const AtMemoryManager&) = delete;

  ~AtMemoryManager();

  // Returns the state (suggestions and filter) for `field_id`.
  // If search statefulness is enabled and persisted state exists, returns
  // the persisted state. Otherwise, returns empty query suggestions.
  AtMemorySearchState GetStateForField(const FieldGlobalId& field_id,
                                       const url::Origin& field_origin);

  // Called when suggestions are shown. The manager initiates an AtMemory
  // session if the `trigger_source` is an AtMemory one.
  // TODO(crbug.com/507770024): Rename to OnSuggestionsShown.
  void OnPopupShown(
      BrowserAutofillManager& bam,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          parent_suggestion_metadata,
      UpdateSuggestionsCallback update_callback,
      ukm::SourceId ukm_source_id);

  // Called when the user types in the filter/search bar. Returns true if
  // handled by the manager (i.e., the current session is an AtMemory one).
  bool OnFilterChanged(const std::u16string& filter);

  // Called when the user has explicitly submitted the search. Returns true if
  // handled by the manager (i.e., the current session is an AtMemory one).
  bool OnSearchSubmitted(const std::u16string& filter);

  // Called when suggestions are hidden.
  void OnPopupHidden();

  // Fills the selected search result. Returns `IsAsync(true)` if the operation
  // involves reauthentication or server communication.
  IsAsync FillSearchResult(
      BrowserAutofillManager& bam,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          metadata);

  // Records the use of the address profile identified by the payload's
  // identifier.
  void RecordAddressProfileUse(
      const Suggestion::AtMemoryPayload::Identifier& identifier);

  // Records the use of the credit card identified by the payload's identifier.
  void RecordCreditCardUse(
      const Suggestion::AtMemoryPayload::Identifier& identifier);

  // Records the use of the Autofill AI entity identified by the payload's
  // identifier.
  void RecordAutofillAiEntityUse(
      const Suggestion::AtMemoryPayload::Identifier& identifier);

  // Returns true if a search is currently in progress.
  bool IsSearching() const;

  // Returns the list of suggestions to show when the query is empty.
  // These suggestions will be in order:
  // * kPersonalContextNotice (optional)
  // * kTitle (optional)
  // * kAtMemorySearchResult (repeated)
  std::vector<Suggestion> GetEmptyQuerySuggestions() const;

  // Appends the personal context notice to the suggestions if necessary.
  void MaybeAppendPersonalContextNotice(
      std::vector<Suggestion>& suggestions) const;

  // Appends the AI disclosure to the suggestions if necessary.
  static void MaybeAppendAiDisclosure(std::vector<Suggestion>& suggestions);

  // Creates the fetching / loading throbber suggestion. `index` determines
  // which string from the fetching cycle is used.
  static Suggestion CreateFetchingSuggestion(size_t index = 0);

  // Creates a catch-all suggestion to display when AtMemory search fails due to
  // an unexpected or generic error.
  static Suggestion CreateGenericErrorSuggestion();

  // Creates a suggestion to display when AtMemory search fails to connect to
  // the server.
  static Suggestion CreateNoConnectionSuggestion(std::u16string query);

  // Creates the search affordance suggestion.
  static Suggestion CreateSearchAffordanceSuggestion(std::u16string query);

  // Creates a source attribution suggestion ("Suggested by Gemini").
  static Suggestion CreateSourceAttributionSuggestion();

  // Transforms an AtMemory search result entry into a `Suggestion`.
  static Suggestion TransformResultIntoSuggestion(
      const MemorySearchResult& entry,
      std::string_view app_locale);

 private:
  friend class AtMemoryManagerTestApi;

  // Executes the search query.
  void ExecuteQuery(const std::u16string& filter);

  // Callback handler for the search query. `query` is the original search
  // string. `result` contains the search results.
  void OnSearchResultsReceived(const std::u16string& query,
                               MemorySearchResults result);

  // Creates a suggestion to display when the query is not supported.
  Suggestion CreateUnsupportedQuerySuggestion(const std::u16string& query);

  // Cancels any pending search queries and resets searching states.
  void CancelPendingQueries();

  // Sends the given suggestions to the UI.
  void SendSuggestions(std::vector<Suggestion> suggestions);

  // Advances to the next fetching suggestion message and updates the UI.
  void AdvanceFetchingSuggestion();

  // Appends previously filled suggestions to the list of suggestions.
  void MaybeAppendPreviouslyFilledSuggestions(
      std::vector<Suggestion>& suggestions) const;

  // Shows all the suggestions in the empty state.
  // These suggestions will be in order:
  // * kPersonalContextNotice (optional)
  // * kTitle (optional)
  // * kAtMemorySearchResult (repeated)
  void ShowEmptyQuerySuggestions();

  // Shows all the suggestions in the query typing state.
  // These suggestions will be in order:
  // * kAtMemorySearchAffordance | kAtMemoryNoConnection
  // * kAtMemoryAiDisclosure | kPersonalContextNotice
  void ShowQueryTypingSuggestions(const std::u16string& query);

  // Shows all the suggestions in the fetching state.
  // These suggestions will be in order:
  // * kAtMemoryFetching
  // * kPersonalContextNotice (optional)
  void ShowFetchingStateSuggestions();

  // Shows all the suggestions in the results retrieved state.
  // These suggestions will be in order:
  // * kPersonalContextNotice (optional)
  // * kAtMemorySearchResult (repeated)
  void ShowResultsRetrievedStateSuggestions(const MemorySearchResults& result);

  // Shows all the suggestions in the no results retrieved state.
  // These suggestions will be in order:
  // * kPersonalContextNotice (optional)
  // * suggestion describing the error
  void ShowNoResultsStateSuggestions(const std::u16string& query,
                                     const MemorySearchResults& result);

  // Fills the unmasked IBAN value after fetching it.
  void FillIban(BrowserAutofillManager& bam,
                const std::variant<Iban::Guid, Iban::InstrumentId>& identifier,
                const FormGlobalId& form_id,
                const FieldGlobalId& field_id,
                const Suggestion& suggestion,
                std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Fills the unmasked credit card value after fetching it.
  void FillCreditCard(BrowserAutofillManager& bam,
                      const std::string& credit_card_guid,
                      const FormGlobalId& form_id,
                      const FieldGlobalId& field_id,
                      const Suggestion& suggestion,
                      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Triggers reauthentication and fetching of the unmasked Personal Context
  // value, which fills the field upon completion. Returns `IsAsync(true)` if
  // the operation involves reauthentication or server communication.
  IsAsync FillSensitivePersonalContextData(
      BrowserAutofillManager& bam,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Fills the field with the unmasked sensitive SPII Personal Context value if
  // fetching succeeded, or records failure metrics if it failed.
  void OnSensitivePersonalContextDataFetched(
      base::WeakPtr<BrowserAutofillManager> bam,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics,
      AtMemoryQueryService::SpiiRetrievalResult result);

  // Fills sensitive identity data by selecting the appropriate filling path
  // depending on whether the data is sourced from Autofill AI or Personal
  // Context. Returns `IsAsync(true)` if the operation involves reauthentication
  // or server communication.
  IsAsync FillSensitiveAutofillAiOrPersonalContextData(
      BrowserAutofillManager& bam,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Fills the unmasked AutofillAI value after fetching it. Returns
  // `IsAsync(true)` if the operation involves reauthentication or server
  // communication.
  IsAsync FillSensitiveAutofillAiData(
      BrowserAutofillManager& bam,
      const EntityInstance::EntityId& entity_id,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      AttributeType data_type,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Callback handler when the unmasked AutofillAI entity has been fetched.
  void OnAutofillAiFetched(
      base::WeakPtr<BrowserAutofillManager> bam,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      AttributeType data_type,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics,
      base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
          result,
      bool reauth_attempted,
      bool did_fetch_from_server);

  // Returns the active target field origin depending on whether search
  // statefulness is enabled.
  const url::Origin& target_field_origin() const;

  // Encapsulates state for the currently visible AtMemory popup.
  struct PopupState {
    AutofillSuggestionTriggerSource trigger_source =
        AutofillSuggestionTriggerSource::kUnspecified;
    UpdateSuggestionsCallback update_callback;
    // TODO(crbug.com/535486238): Reconsider where metrics_recorder should live.
    std::unique_ptr<AtMemoryMetricsRecorder> metrics_recorder;
    // Flag indicating that a search query is in progress.
    // TODO(crbug.com/535486238): Remove `is_searching` once
    // `kAutofillAtMemorySearchStatefulness` is fully launched.
    bool is_searching = false;
    // Timer used to rotate the fetching suggestions while searching.
    base::RepeatingTimer fetching_timer;
    // Index of the current fetching message to display.
    size_t fetching_string_index = 0;
  };

  const raw_ref<AutofillClient> client_;

  std::optional<PopupState> popup_state_;

  // Origin of the target field for the active search session. Only set when
  // `kAutofillAtMemorySearchStatefulness` is disabled.
  // TODO(crbug.com/535486238): Remove `target_field_origin_` once
  // `kAutofillAtMemorySearchStatefulness` is fully launched.
  url::Origin target_field_origin_;

  AtMemoryPersistedStateManager state_manager_;
  // Factory for search queries, used to identify currently active query and
  // discard the old ones.
  base::WeakPtrFactory<AtMemoryManager> query_weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemoryManager> fill_weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
