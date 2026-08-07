// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

struct MemorySearchResults;
class BrowserAutofillManager;

// Manager for the AtMemory feature. It handles queries to the
// `AtMemoryQueryService` and manages session-based metrics. Owned by
// `BrowserAutofillManager`, its lifetime is tied to it.
class AtMemoryManager : public CreditCardAccessManager::Observer {
 public:
  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<Suggestion>,
                                   AutofillSuggestionTriggerSource)>;

  explicit AtMemoryManager(BrowserAutofillManager* manager);

  AtMemoryManager(const AtMemoryManager&) = delete;
  AtMemoryManager& operator=(const AtMemoryManager&) = delete;

  ~AtMemoryManager() override;

  // Called when suggestions are shown. The manager initiates an @memory
  // session if the `trigger_source` is an @memory one.
  // TODO(crbug.com/507770024): Rename to OnSuggestionsShown.
  void OnPopupShown(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          parent_suggestion_metadata,
      bool is_context_secure,
      UpdateSuggestionsCallback update_callback,
      ukm::SourceId ukm_source_id);

  // Called when the user types in the filter/search bar. Returns true if
  // handled by the manager (i.e., the current session is an @memory one).
  bool OnFilterChanged(const std::u16string& filter);

  // Called when the user has explicitly submitted the search. Returns true if
  // handled by the manager (i.e., the current session is an @memory one).
  bool OnSearchSubmitted(const std::u16string& filter);

  // Called when suggestions are hidden.
  void OnPopupHidden();

  // Fills or previews the selected search result. Returns `IsAsync(true)` if
  // the operation involves reauthentication or server communication.
  IsAsync FillOrPreviewSearchResult(
      mojom::ActionPersistence action_persistence,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          metadata = std::nullopt);

  // Fills the selected search result. Returns `IsAsync(true)` if the operation
  // involves reauthentication or server communication.
  IsAsync FillSearchResult(
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

  // Appends the personal context notice to the suggestions if necessary.
  void MaybeAppendPersonalContextNotice(
      std::vector<Suggestion>& suggestions) const;

  // Creates the AI disclosure suggestion.
  static Suggestion CreateAiDisclosureSuggestion();

  // Creates the fetching / loading throbber suggestion.
  static Suggestion CreateFetchingSuggestion();

  // Creates a catch-all suggestion to display when AtMemory search fails due to
  // an unexpected or generic error.
  static Suggestion CreateGenericErrorSuggestion();

  // Creates a suggestion to display when AtMemory search fails to connect to
  // the server.
  static Suggestion CreateNoConnectionSuggestion(std::u16string query);

  // Creates the search affordance suggestion.
  static Suggestion CreateSearchAffordanceSuggestion(std::u16string query);

  void set_target_field_origin(const url::Origin& origin) {
    target_field_origin_ = origin;
  }

  // Creates a source attribution suggestion ("Suggested by Gemini").
  static Suggestion CreateSourceAttributionSuggestion();

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

  // Shows the fetching suggestion in the UI.
  void ShowFetchingSuggestion();

  // Clears all currently shown suggestions in the UI.
  void ClearSuggestions();

  // Fills the unmasked IBAN value after fetching it. Returns `IsAsync(true)` if
  // the operation involves reauthentication or server communication.
  IsAsync FillIban(
      const std::variant<Iban::Guid, Iban::InstrumentId>& identifier,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Fills the unmasked credit card value after fetching it. Returns
  // `IsAsync(true)` if the operation involves reauthentication or server
  // communication.
  IsAsync FillCreditCard(const std::string& credit_card_guid,
                         const FormGlobalId& form_id,
                         const FieldGlobalId& field_id,
                         const Suggestion& suggestion,
                         std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // CreditCardAccessManager::Observer:
  void OnCreditCardFetchStarted(CreditCardAccessManager& manager,
                                const CreditCard& credit_card) override;
  void OnCreditCardFetchSucceeded(CreditCardAccessManager& manager,
                                  const CreditCard& credit_card) override;
  void OnCreditCardFetchFailed(CreditCardAccessManager& manager,
                               const CreditCard* credit_card) override;
  void OnCreditCardAccessManagerDestroyed(
      CreditCardAccessManager& manager) override;

  // Triggers reauthentication and fetching of the unmasked Personal Context
  // value, which fills the field upon completion. Returns `IsAsync(true)` if
  // the operation involves reauthentication or server communication.
  IsAsync FillSensitivePersonalContextData(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Fills the field with the unmasked sensitive SPII Personal Context value if
  // fetching succeeded, or records failure metrics if it failed.
  void OnSensitivePersonalContextDataFetched(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics,
      AtMemoryQueryService::SpiiRetrievalResult result);

  // Fills sensitive identity data by selecting the appropriate filling path
  // depending on whether the data is sourced from Autofill AI or Personal
  // Context. Returns `IsAsync(true)` if the operation involves reauthentication
  // or server communication.
  IsAsync FillSensitiveAutofillAiOrPersonalContextData(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Fills the unmasked AutofillAI value after fetching it. Returns
  // `IsAsync(true)` if the operation involves reauthentication or server
  // communication.
  IsAsync FillSensitiveAutofillAiData(
      const EntityInstance::EntityId& entity_id,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      AttributeType data_type,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics);

  // Callback handler when the unmasked AutofillAI entity has been fetched.
  void OnAutofillAiFetched(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const Suggestion& suggestion,
      AttributeType data_type,
      std::unique_ptr<AtMemoryMetricsRecorder> metrics,
      base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
          result,
      bool reauth_attempted,
      bool did_fetch_from_server);

  // Encapsulates active session state for an AtMemory UI interaction.
  struct SessionState {
    AutofillSuggestionTriggerSource trigger_source =
        AutofillSuggestionTriggerSource::kUnspecified;
    UpdateSuggestionsCallback update_callback;
    std::unique_ptr<AtMemoryMetricsRecorder> metrics_recorder;
    // Indicates whether the current tab and the form uses a secure connection.
    bool is_context_secure = false;
    // Flag indicating that a search query is in progress.
    bool is_searching = false;
  };

  const raw_ptr<BrowserAutofillManager> owner_;

  std::optional<SessionState> session_state_;

  base::ScopedObservation<CreditCardAccessManager,
                          CreditCardAccessManager::Observer>
      ccam_observation_{this};

  bool credit_card_fetch_in_progress_ = false;

  // Origin of the target field for the active search session.
  url::Origin target_field_origin_;
  // Factory for search queries, used to identify currently active query and
  // discard the old ones.
  base::WeakPtrFactory<AtMemoryManager> query_weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemoryManager> fill_weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_H_
