// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <utility>

#include "base/check.h"

namespace autofill {

AtMemoryPersistedStateManager::AtMemoryPersistedStateManager() = default;
AtMemoryPersistedStateManager::~AtMemoryPersistedStateManager() = default;

const std::optional<AtMemoryManagerState>&
AtMemoryPersistedStateManager::GetInitialStateForField(
    const FieldGlobalId& field_id) {
  if (field_id_ != field_id) {
    field_id_ = field_id;
    state_.reset();
  }
  return state_;
}

void AtMemoryPersistedStateManager::OnFilterChanged(
    std::u16string_view filter) {
  CHECK(field_id_);
  if (filter.empty()) {
    state_.reset();
    return;
  }
  if (!state_) {
    state_.emplace();
  }
  state_->filter = filter;
  state_->suggestions.clear();
  state_->is_searching = false;
}

void AtMemoryPersistedStateManager::OnFilterSubmitted(
    const std::u16string& filter) {
  CHECK(field_id_);
  if (!state_) {
    state_.emplace();
  }
  state_->filter = filter;
  state_->is_searching = true;
}

void AtMemoryPersistedStateManager::OnSuggestionsChanged(
    std::vector<Suggestion> suggestions) {
  if (!state_) {
    return;
  }
  state_->suggestions = std::move(suggestions);
}

void AtMemoryPersistedStateManager::OnSuggestionAccepted() {
  field_id_ = FieldGlobalId();
  state_.reset();
}

bool AtMemoryPersistedStateManager::IsSearching() const {
  return state_ && state_->is_searching;
}

void AtMemoryPersistedStateManager::StopSearching() {
  if (state_) {
    if (state_->is_searching) {
      state_->suggestions.clear();
    }
    state_->is_searching = false;
  }
}

}  // namespace autofill
