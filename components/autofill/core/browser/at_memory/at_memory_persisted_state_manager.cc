// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <utility>

#include "base/check.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

AtMemoryPersistedStateManager::AtMemoryPersistedStateManager() = default;
AtMemoryPersistedStateManager::~AtMemoryPersistedStateManager() = default;

const std::optional<AtMemorySearchState>&
AtMemoryPersistedStateManager::GetStateForField(
    const FieldGlobalId& field_id,
    const url::Origin& field_origin) {
  if (field_id_ != field_id) {
    field_id_ = field_id;
    field_origin_ = field_origin;
    search_state_.reset();
  }
  return search_state_;
}

void AtMemoryPersistedStateManager::OnFilterChanged(
    std::u16string_view filter) {
  CHECK(field_id_);
  if (filter.empty()) {
    search_state_.reset();
    return;
  }
  if (!search_state_) {
    search_state_.emplace();
  }
  search_state_->filter = filter;
  search_state_->suggestions.clear();
  search_state_->is_searching = false;
}

void AtMemoryPersistedStateManager::OnFilterSubmitted(
    const std::u16string& filter) {
  CHECK(field_id_);
  if (!search_state_) {
    search_state_.emplace();
  }
  search_state_->filter = filter;
  search_state_->is_searching = true;
}

void AtMemoryPersistedStateManager::OnSuggestionsChanged(
    std::vector<Suggestion> suggestions) {
  if (!search_state_) {
    return;
  }
  search_state_->suggestions = std::move(suggestions);
}

void AtMemoryPersistedStateManager::OnSuggestionAccepted(
    const Suggestion& suggestion) {
  field_id_ = FieldGlobalId();
  search_state_.reset();
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryPreviouslyFilled)) {
    return;
  }
  // TODO(crbug.com/494559543): Deduplicate suggestions.
  // TODO(crbug.com/494559543): For secondary suggestions, push their
  // corresponding primary suggestion instead.
  previously_filled_suggestions_.push_back(suggestion);
}

bool AtMemoryPersistedStateManager::IsSearching() const {
  return search_state_ && search_state_->is_searching;
}

void AtMemoryPersistedStateManager::StopSearching() {
  if (!search_state_ || !search_state_->is_searching) {
    return;
  }
  search_state_->suggestions.clear();
  search_state_->is_searching = false;
}

}  // namespace autofill
