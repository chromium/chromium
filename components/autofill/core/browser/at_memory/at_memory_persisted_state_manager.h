// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "components/autofill/core/browser/at_memory/at_memory_manager_state.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Manages the single-field in-memory persisted search state for AtMemory
// autofill.
//
// Lifecycle invariant:
// `GetInitialStateForField()` MUST be called whenever a field is focused before
// invoking any mutation methods (`OnFilterChanged`, `OnFilterSubmitted`).
// When a new field is focused, any existing state for a prior field is reset.
//
// A `std::nullopt` state represents the unmodified / initial 0-state for the
// active field (e.g. before any filter has been entered, or after clearing the
// filter). `state_` is only instantiated once the user enters or submits a
// query.
class AtMemoryPersistedStateManager {
 public:
  AtMemoryPersistedStateManager();
  ~AtMemoryPersistedStateManager();

  AtMemoryPersistedStateManager(const AtMemoryPersistedStateManager&) = delete;
  AtMemoryPersistedStateManager& operator=(
      const AtMemoryPersistedStateManager&) = delete;

  // Returns the existing persisted state if `field_id` matches the stored
  // persisted AtMemory search state. Otherwise resets state and initializes a
  // new persisted AtMemory search state for `field_id`.
  const std::optional<AtMemoryManagerState>& GetInitialStateForField(
      const FieldGlobalId& field_id);

  void OnFilterChanged(std::u16string_view filter);
  void OnFilterSubmitted(const std::u16string& filter);
  void OnSuggestionsChanged(std::vector<Suggestion> suggestions);
  void OnSuggestionAccepted();

  bool IsSearching() const;
  void StopSearching();

 private:
  FieldGlobalId field_id_;
  std::optional<AtMemoryManagerState> state_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_PERSISTED_STATE_MANAGER_H_
