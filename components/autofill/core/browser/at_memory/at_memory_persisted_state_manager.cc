// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/history/core/browser/history_service.h"

namespace autofill {

namespace {

// Returns a pointer to the parent suggestion within `suggestions` that contains
// `suggestion` in its children, or `nullptr` if none is found.
const Suggestion* FindParentSuggestion(
    const Suggestion& suggestion,
    base::span<const Suggestion> suggestions) {
  const auto it =
      std::ranges::find_if(suggestions, [&suggestion](const Suggestion& entry) {
        return std::ranges::contains(entry.children, suggestion);
      });
  return it != suggestions.end() ? &(*it) : nullptr;
}

// Returns the primary parent suggestion for `accepted_suggestion` if it is a
// child suggestion in `search_state` or `previously_filled_suggestions`.
// Otherwise returns `accepted_suggestion` itself.
const Suggestion& GetSuggestionToStore(
    const Suggestion& accepted_suggestion,
    const std::optional<AtMemorySearchState>& search_state,
    base::span<const Suggestion> previously_filled_suggestions) {
  if (search_state) {
    if (const Suggestion* parent = FindParentSuggestion(
            accepted_suggestion, search_state->suggestions)) {
      return *parent;
    }
  }
  if (const Suggestion* parent = FindParentSuggestion(
          accepted_suggestion, previously_filled_suggestions)) {
    return *parent;
  }
  return accepted_suggestion;
}

}  // namespace

AtMemoryPersistedStateManager::AtMemoryPersistedStateManager(
    history::HistoryService* history_service) {
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

AtMemoryPersistedStateManager::~AtMemoryPersistedStateManager() = default;

const std::optional<AtMemorySearchState>&
AtMemoryPersistedStateManager::GetStateForField(
    const FieldGlobalId& field_id,
    const url::Origin& field_origin) {
  if (field_id_ != field_id) {
    search_state_timer_.Stop();
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
    search_state_timer_.Stop();
    return;
  }
  if (!search_state_) {
    search_state_.emplace();
  }
  search_state_->filter = filter;
  search_state_->suggestions.clear();
  search_state_->is_searching = false;
  RestartSearchStateTimer();
}

void AtMemoryPersistedStateManager::OnFilterSubmitted(
    const std::u16string& filter) {
  CHECK(field_id_);
  if (!search_state_) {
    search_state_.emplace();
  }
  search_state_->filter = filter;
  search_state_->is_searching = true;
  RestartSearchStateTimer();
}

void AtMemoryPersistedStateManager::OnSuggestionsChanged(
    std::vector<Suggestion> suggestions) {
  if (!search_state_) {
    return;
  }
  search_state_->suggestions = std::move(suggestions);
  RestartSearchStateTimer();
}

void AtMemoryPersistedStateManager::OnSuggestionAccepted(
    const Suggestion& suggestion) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryPreviouslyFilled)) {
    const Suggestion& suggestion_to_store = GetSuggestionToStore(
        suggestion, search_state_, previously_filled_suggestions_);
    const auto it =
        std::ranges::find(previously_filled_suggestions_, suggestion_to_store);
    if (it != previously_filled_suggestions_.end()) {
      std::rotate(it, it + 1, previously_filled_suggestions_.end());
    } else {
      if (previously_filled_suggestions_.size() >=
          kMaxPreviouslyFilledSuggestions) {
        previously_filled_suggestions_.erase(
            previously_filled_suggestions_.begin());
      }
      previously_filled_suggestions_.push_back(suggestion_to_store);
    }
  }
  ResetSearchState();
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

void AtMemoryPersistedStateManager::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  Reset();
}

void AtMemoryPersistedStateManager::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observation_.Reset();
}

void AtMemoryPersistedStateManager::Reset() {
  ResetSearchState();
  previously_filled_suggestions_.clear();
}

void AtMemoryPersistedStateManager::ResetSearchState() {
  search_state_timer_.Stop();
  field_id_ = FieldGlobalId();
  field_origin_ = url::Origin();
  search_state_.reset();
}

void AtMemoryPersistedStateManager::RestartSearchStateTimer() {
  search_state_timer_.Start(FROM_HERE, kTimeToLive, this,
                            &AtMemoryPersistedStateManager::ResetSearchState);
}

}  // namespace autofill
