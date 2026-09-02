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
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/at_memory/at_memory_search_state.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/history/core/browser/history_service_observer.h"
#include "url/origin.h"

namespace history {
class HistoryService;
}

namespace autofill {

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
class AtMemoryPersistedStateManager : public history::HistoryServiceObserver {
 public:
  static constexpr size_t kMaxPreviouslyFilledSuggestions = 20;
  static constexpr base::TimeDelta kTimeToLive = base::Minutes(30);

  explicit AtMemoryPersistedStateManager(
      history::HistoryService* history_service);
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

  const std::vector<Suggestion>& previously_filled_suggestions() const {
    return previously_filled_suggestions_;
  }

  const url::Origin& field_origin() const {
    CHECK(field_id_);
    return field_origin_;
  }

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  void Reset();
  void ResetSearchState();
  void RestartSearchStateTimer();

  // Field id for which the `search_state_` is kept.
  FieldGlobalId field_id_;
  // Origin of the field for which the `search_state_` is kept.
  url::Origin field_origin_;
  // State of the search for the active field. Reset if
  // `GetStateForField` is called for another field.
  std::optional<AtMemorySearchState> search_state_;
  // Stores previously filled suggestions.
  std::vector<Suggestion> previously_filled_suggestions_;
  base::OneShotTimer search_state_timer_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_
