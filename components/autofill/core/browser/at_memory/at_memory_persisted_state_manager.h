// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/at_memory/at_memory_search_state.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/origin.h"

namespace history {
class HistoryService;
}

namespace autofill {

class AutofillClient;

// Manages in-memory persisted state for AtMemory autofill:
// 1. `search_state_`: Persisted search state (filter, suggestions, search
//    status) for the active field across popup open/close lifecycles.
// 2. `previously_filled_suggestions_`: History of suggestions accepted by the
//    user, rendered in empty-query suggestion popups.
//
// Lifecycle invariant:
// `GetStateForField()` MUST be called whenever a field is focused before
// invoking any mutation methods (`OnFilterChanged`, `OnFilterSubmitted`).
// When a new field is focused, any existing state for a prior field is reset.
//
// A `std::nullopt` state represents the unmodified / initial 0-state for the
// active field (e.g. before any filter has been entered, or after clearing the
// filter). `search_state_` is only instantiated once the user enters or submits
// a query.
class AtMemoryPersistedStateManager
    : public history::HistoryServiceObserver,
      public personal_context::PersonalContextEligibilityService::Observer,
      public signin::IdentityManager::Observer,
      public AutofillManager::Observer {
 public:
  struct ExpiringSuggestion {
    Suggestion suggestion;
    base::TimeTicks expiration_time;
  };

  static constexpr size_t kMaxPreviouslyFilledSuggestions = 20;
  static constexpr base::TimeDelta kDefaultTimeToLive = base::Minutes(30);
  static constexpr base::TimeDelta kSpiiTimeToLive = base::Minutes(1);

  // `client` must be non-null and outlive `this`. The pref service, identity
  // manager and eligibility service are obtained from `client`.
  // `history_service` may be null if the profile has no history service (e.g.
  // in tests or for profiles without history). In that case AtMemory state is
  // simply not cleared on history deletions.
  AtMemoryPersistedStateManager(AutofillClient* client,
                                history::HistoryService* history_service,
                                base::RepeatingClosure on_reset_callback);
  ~AtMemoryPersistedStateManager() override;

  AtMemoryPersistedStateManager(const AtMemoryPersistedStateManager&) = delete;
  AtMemoryPersistedStateManager& operator=(
      const AtMemoryPersistedStateManager&) = delete;

  // Returns the existing persisted state if `field_id` matches the stored
  // persisted AtMemory search state. Otherwise resets state and initializes a
  // new persisted AtMemory search state for `field_id` with `field_origin`.
  const std::optional<AtMemorySearchState>& GetStateForField(
      const FieldGlobalId& field_id,
      const url::Origin& field_origin);

  void OnFilterChanged(std::u16string_view filter);
  void OnFilterSubmitted(const std::u16string& filter);
  void OnSuggestionsChanged(std::vector<Suggestion> suggestions);
  void OnSuggestionAccepted(const Suggestion& suggestion);

  bool IsSearching() const;
  void StopSearching();

  std::vector<Suggestion> previously_filled_suggestions() const;

  const url::Origin& field_origin() const {
    CHECK(field_id_);
    return field_origin_;
  }

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // personal_context::PersonalContextEligibilityService::Observer:
  void OnEligibilityStateChanged(
      personal_context::PersonalContextEligibilityState new_state) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // AutofillManager::Observer:
  // Clears all state when the tab's primary main frame navigates
  // cross-document, i.e. when its AutofillManager transitions to
  // `kPendingReset` or `kPendingDeletion`.
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillManager::LifecycleState old_state,
      AutofillManager::LifecycleState new_state) override;

 private:
  // Resets the persisted state, clears `previously_filled_suggestions_`, and
  // executes `on_reset_callback_`.
  void Reset();
  void ResetSearchState();
  void RestartSearchStateTimer();
  void RestartPreviouslyFilledSuggestionsTimer();

  // Removes previously filled suggestions that have exceeded their TTL.
  void RemoveExpiredPreviouslyFilledSuggestions();

  void OnPrefChanged();

  // Field id for which the `search_state_` is kept.
  FieldGlobalId field_id_;
  // Origin of the field for which the `search_state_` is kept.
  url::Origin field_origin_;
  // State of the search for the active field. Reset if
  // `GetStateForField` is called for another field.
  std::optional<AtMemorySearchState> search_state_;
  base::OneShotTimer search_state_timer_;

  // Stores previously filled suggestions along with their expiration time.
  std::vector<ExpiringSuggestion> previously_filled_suggestions_;
  base::OneShotTimer previously_filled_suggestions_timer_;

  PrefChangeRegistrar pref_registrar_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<
      personal_context::PersonalContextEligibilityService,
      personal_context::PersonalContextEligibilityService::Observer>
      eligibility_service_observation_{this};
  ScopedAutofillManagersObservation autofill_managers_observation_{this};
  // Callback invoked whenever persisted state is reset (e.g. due to TTL expiry,
  // history deletion, settings toggle being turned off, or eligibility loss).
  // Used by `AtMemoryManager` to cancel in-flight queries and dismiss any
  // active suggestion popup.
  // TODO(crbug.com/535486238): Consider extracting history/pref/eligibility
  // observations into a dedicated helper class (e.g.
  // `AtMemoryStateChangeObserver`) to avoid a cyclic call graph; see
  // https://crrev.com/c/8320987/comment/f3210d7d_373869e6/.
  base::RepeatingClosure on_reset_callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_
