// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/history/core/browser/history_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"

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

const Suggestion* FindParentSuggestion(
    const Suggestion& suggestion,
    base::span<const AtMemoryPersistedStateManager::ExpiringSuggestion>
        suggestions) {
  const auto it = std::ranges::find_if(
      suggestions,
      [&suggestion](
          const AtMemoryPersistedStateManager::ExpiringSuggestion& entry) {
        return std::ranges::contains(entry.suggestion.children, suggestion);
      });
  return it != suggestions.end() ? &it->suggestion : nullptr;
}

// Returns the primary parent suggestion for `accepted_suggestion` if it is a
// child suggestion in `search_state` or `previously_filled_suggestions`.
// Otherwise returns `accepted_suggestion` itself.
const Suggestion& GetSuggestionToStore(
    const Suggestion& accepted_suggestion,
    const std::optional<AtMemorySearchState>& search_state,
    base::span<const AtMemoryPersistedStateManager::ExpiringSuggestion>
        previously_filled_suggestions) {
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

// Returns true if `driver` belongs to the tab's *primary* main frame, i.e. the
// main frame of the page that is currently presented to the user.
//
// In MPArch terminology, a frame is the primary main frame if it is
// - a main frame: it has no parent frame (`GetParent() == nullptr`),
// - outermost: its frame tree is not embedded in another page by a
//   <fencedframe> or a GuestView (`!IsEmbedded()`), and
// - primary: the page is displayed, i.e. it is neither prerendered nor in the
//   back/forward cache (`IsActive()`).
bool IsPrimaryMainFrame(AutofillDriver& driver) {
  return driver.GetParent() == nullptr && !driver.IsEmbedded() &&
         driver.IsActive();
}

}  // namespace

AtMemoryPersistedStateManager::AtMemoryPersistedStateManager(
    AutofillClient* client,
    history::HistoryService* history_service,
    base::RepeatingClosure on_reset_callback)
    : on_reset_callback_(std::move(on_reset_callback)) {
  CHECK(client);
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  if (PrefService* pref_service = client->GetPrefs()) {
    pref_registrar_.Init(pref_service);
    pref_registrar_.Add(
        personal_context::prefs::
            kPersonalContextInAutofillSettingsToggleStatus,
        base::BindRepeating(&AtMemoryPersistedStateManager::OnPrefChanged,
                            base::Unretained(this)));
  }
  if (signin::IdentityManager* identity_manager =
          client->GetIdentityManager()) {
    identity_manager_observation_.Observe(identity_manager);
  }
  if (personal_context::PersonalContextEligibilityService* eligibility_service =
          client->GetPersonalContextEligibilityService()) {
    eligibility_service_observation_.Observe(eligibility_service);
  }
  autofill_managers_observation_.Observe(
      client, ScopedAutofillManagersObservation::InitializationPolicy::
                  kObservePreexistingManagers);
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

std::vector<Suggestion>
AtMemoryPersistedStateManager::previously_filled_suggestions() const {
  return base::ToVector(previously_filled_suggestions_,
                        &ExpiringSuggestion::suggestion);
}

void AtMemoryPersistedStateManager::OnSuggestionAccepted(
    const Suggestion& suggestion) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryPreviouslyFilled)) {
    const Suggestion& suggestion_to_store = GetSuggestionToStore(
        suggestion, search_state_, previously_filled_suggestions_);
    const auto it =
        std::ranges::find(previously_filled_suggestions_, suggestion_to_store,
                          &ExpiringSuggestion::suggestion);
    const base::TimeTicks expiration_time =
        base::TimeTicks::Now() + kDefaultTimeToLive;

    if (it != previously_filled_suggestions_.end()) {
      it->expiration_time = expiration_time;
      std::rotate(it, it + 1, previously_filled_suggestions_.end());
    } else {
      if (previously_filled_suggestions_.size() >=
          kMaxPreviouslyFilledSuggestions) {
        previously_filled_suggestions_.erase(
            previously_filled_suggestions_.begin());
      }
      previously_filled_suggestions_.push_back(
          ExpiringSuggestion{.suggestion = suggestion_to_store,
                             .expiration_time = expiration_time});
    }
    RestartPreviouslyFilledSuggestionsTimer();
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

void AtMemoryPersistedStateManager::OnEligibilityStateChanged(
    personal_context::PersonalContextEligibilityState new_state) {
  switch (new_state) {
    case personal_context::PersonalContextEligibilityState::kEligible:
      break;
    case personal_context::PersonalContextEligibilityState::
        kDisabledNotEligible:
      Reset();
      break;
  }
}

void AtMemoryPersistedStateManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    Reset();
  }
}

void AtMemoryPersistedStateManager::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
}

void AtMemoryPersistedStateManager::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState old_state,
    AutofillManager::LifecycleState new_state) {
  switch (new_state) {
    case AutofillManager::LifecycleState::kInactive:
    case AutofillManager::LifecycleState::kActive:
      return;
    case AutofillManager::LifecycleState::kPendingReset:
    case AutofillManager::LifecycleState::kPendingDeletion:
      break;
  }
  if (IsPrimaryMainFrame(manager.driver())) {
    Reset();
  }
}

void AtMemoryPersistedStateManager::Reset() {
  ResetSearchState();
  previously_filled_suggestions_.clear();
  previously_filled_suggestions_timer_.Stop();
  // TODO(crbug.com/535486238): Consider cancelling ongoing queries directly in
  // `PersonalContextService` / `AtMemoryQueryService` when enablement state
  // changes. This would require adding cancellation functions to said services.
  if (on_reset_callback_) {
    on_reset_callback_.Run();
  }
}

void AtMemoryPersistedStateManager::ResetSearchState() {
  search_state_timer_.Stop();
  field_id_ = FieldGlobalId();
  field_origin_ = url::Origin();
  search_state_.reset();
}

void AtMemoryPersistedStateManager::OnSearchStateTimerExpired() {
  const bool has_suggestions =
      search_state_ && !search_state_->suggestions.empty();
  ResetSearchState();
  if (has_suggestions && on_reset_callback_) {
    on_reset_callback_.Run();
  }
}

void AtMemoryPersistedStateManager::RestartSearchStateTimer() {
  search_state_timer_.Start(
      FROM_HERE, kDefaultTimeToLive, this,
      &AtMemoryPersistedStateManager::OnSearchStateTimerExpired);
}

void AtMemoryPersistedStateManager::RestartPreviouslyFilledSuggestionsTimer() {
  if (previously_filled_suggestions_.empty()) {
    previously_filled_suggestions_timer_.Stop();
    return;
  }
  // Finds the suggestion that expires next.
  const auto min_it = std::ranges::min_element(
      previously_filled_suggestions_, {}, &ExpiringSuggestion::expiration_time);
  const base::TimeDelta delay = std::max(
      base::TimeDelta(), min_it->expiration_time - base::TimeTicks::Now());
  previously_filled_suggestions_timer_.Start(
      FROM_HERE, delay, this,
      &AtMemoryPersistedStateManager::RemoveExpiredPreviouslyFilledSuggestions);
}

void AtMemoryPersistedStateManager::RemoveExpiredPreviouslyFilledSuggestions() {
  const base::TimeTicks now = base::TimeTicks::Now();
  std::erase_if(previously_filled_suggestions_,
                [now](const ExpiringSuggestion& entry) {
                  return entry.expiration_time <= now;
                });
  RestartPreviouslyFilledSuggestionsTimer();
}

void AtMemoryPersistedStateManager::OnPrefChanged() {
  if (!pref_registrar_.prefs()->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    Reset();
  }
}

}  // namespace autofill
