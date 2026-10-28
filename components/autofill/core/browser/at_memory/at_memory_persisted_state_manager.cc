// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/linear_find.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
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
  return base::LinearFindIf(
      suggestions, [&suggestion](const Suggestion& entry) {
        return std::ranges::contains(entry.children, suggestion);
      });
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
    const AtMemorySearchState& search_state,
    base::span<const AtMemoryPersistedStateManager::ExpiringSuggestion>
        previously_filled_suggestions) {
  if (const Suggestion* parent =
          FindParentSuggestion(accepted_suggestion, search_state.suggestions)) {
    return *parent;
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

// Returns which kind of persisted search `state` is restored when the popup is
// shown.
AtMemoryStateRestored GetStateRestored(const AtMemorySearchState& state) {
  switch (state.status) {
    case AtMemorySearchStatus::kZeroState:
      return AtMemoryStateRestored::kNewSession;
    case AtMemorySearchStatus::kQueryModified:
      return AtMemoryStateRestored::kRestoredWithExistingQuery;
    case AtMemorySearchStatus::kSearching:
    case AtMemorySearchStatus::kPartialResponseReceived:
      return AtMemoryStateRestored::kRestoredWhileSearching;
    case AtMemorySearchStatus::kResponseReceived:
      return AtMemoryStateRestored::kRestoredWithResponse;
  }
  NOTREACHED();
}

// Returns true if `payload` is an address suggestion whose underlying profile
// no longer exists in `adm`, or was modified after `insertion_time`. Returns
// false for non-address suggestions or if the profile identifier is not known.
bool ShouldPruneSuggestionOnAddressDataChange(
    const Suggestion::AtMemoryPayload& payload,
    base::Time insertion_time,
    AddressDataManager& adm) {
  // Not a Contact Info suggestion - ignore.
  if (GetMemoryDataTypeCategory(payload.memory_data_type) !=
      MemoryDataTypeCategory::kContactInfo) {
    return false;
  }
  const std::string* guid = std::get_if<std::string>(&payload.identifier);
  // Invalid id - ignore.
  if (!guid || guid->empty()) {
    return false;
  }
  const AutofillProfile* profile = adm.GetProfileByGUID(*guid);
  // Profile was deleted - prune.
  if (!profile) {
    return true;
  }
  // If profile was modified after the suggestion was inserted, prune.
  return !insertion_time.is_null() &&
         profile->usage_history().modification_date() > insertion_time;
}

// Returns true if `payload` is a payment suggestion (credit card or IBAN) whose
// underlying data no longer exists in `pdm`, or was modified after
// `insertion_time`. Returns false for non-payment suggestions or if the payment
// identifier is not known.
bool ShouldPruneSuggestionOnPaymentsDataChange(
    const Suggestion::AtMemoryPayload& payload,
    base::Time insertion_time,
    PaymentsDataManager& pdm) {
  const MemoryDataTypeCategory category =
      GetMemoryDataTypeCategory(payload.memory_data_type);
  if (category == MemoryDataTypeCategory::kCreditCard) {
    const std::string* guid = std::get_if<std::string>(&payload.identifier);
    // Invalid id - ignore.
    if (!guid || guid->empty()) {
      return false;
    }
    const CreditCard* card = pdm.GetCreditCardByGUID(*guid);
    // Card was deleted - prune.
    if (!card) {
      return true;
    }
    // If card was modified after the suggestion was inserted, prune.
    return !insertion_time.is_null() &&
           card->usage_history().modification_date() > insertion_time;
  }
  if (category == MemoryDataTypeCategory::kIban) {
    const Iban* iban = nullptr;
    if (const Iban::Guid* guid = std::get_if<Iban::Guid>(&payload.identifier)) {
      iban = pdm.GetIbanByGUID(guid->value());
    } else if (const Iban::InstrumentId* instrument_id =
                   std::get_if<Iban::InstrumentId>(&payload.identifier)) {
      iban = pdm.GetIbanByInstrumentId(instrument_id->value());
    }
    // IBAN was deleted - prune.
    if (!iban) {
      return true;
    }
    // If IBAN was modified after the suggestion was inserted, prune.
    return !insertion_time.is_null() &&
           iban->usage_history().modification_date() > insertion_time;
  }
  // Not a payment suggestion - ignore.
  return false;
}

// Returns true if `payload` is an Autofill AI entity suggestion whose
// underlying entity no longer exists in `edm`, or was modified after
// `insertion_time`. Returns false for non-entity suggestions or if the entity
// identifier is not known.
bool ShouldPruneSuggestionOnEntityDataChange(
    const Suggestion::AtMemoryPayload& payload,
    base::Time insertion_time,
    EntityDataManager& edm) {
  // Not a Autofill AI entity suggestion - ignore.
  if (!ToEntityType(payload.memory_data_type).has_value()) {
    return false;
  }
  const EntityInstance::EntityId* guid =
      std::get_if<EntityInstance::EntityId>(&payload.identifier);
  // Invalid id - ignore.
  if (!guid || guid->value().empty()) {
    return false;
  }
  const base::optional_ref<const EntityInstance> entity =
      edm.GetEntityInstance(*guid);
  // Entity was deleted - prune.
  if (!entity.has_value()) {
    return true;
  }
  // If entity was modified after the suggestion was inserted, prune.
  return !insertion_time.is_null() && entity->date_modified() > insertion_time;
}

// Returns true if `payload` is a loyalty card suggestion whose underlying card
// no longer exists in `vdm`. Returns false for non-loyalty card suggestions or
// if the card identifier is not known.
bool ShouldPruneSuggestionOnValuablesDataChange(
    const Suggestion::AtMemoryPayload& payload,
    ValuablesDataManager& vdm) {
  // Not a loyalty card suggestion - ignore.
  if (GetMemoryDataTypeCategory(payload.memory_data_type) !=
      MemoryDataTypeCategory::kLoyaltyCard) {
    return false;
  }
  const ValuableId* id = std::get_if<ValuableId>(&payload.identifier);
  // Invalid id - ignore.
  if (!id || id->value().empty()) {
    return false;
  }
  // Card was deleted - prune.
  return !vdm.GetLoyaltyCardById(*id).has_value();
}
}  // namespace

AtMemoryPersistedStateManager::AtMemoryPersistedStateManager(
    AutofillClient* client,
    history::HistoryService* history_service,
    base::RepeatingClosure on_reset_callback)
    : on_reset_callback_(std::move(on_reset_callback)) {
  CHECK(client);
  if (client->HasPersonalDataManager()) {
    address_data_manager_observation_.Observe(
        &client->GetPersonalDataManager().address_data_manager());
    payments_data_manager_observation_.Observe(
        &client->GetPersonalDataManager().payments_data_manager());
  }
  if (EntityDataManager* entity_data_manager = client->GetEntityDataManager()) {
    entity_data_manager_observation_.Observe(entity_data_manager);
  }
  if (ValuablesDataManager* valuables_data_manager =
          client->GetValuablesDataManager()) {
    valuables_data_manager_observation_.Observe(valuables_data_manager);
  }
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  if (PrefService* pref_service = client->GetPrefs()) {
    pref_registrar_.Init(pref_service);
    pref_registrar_.Add(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
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

const AtMemorySearchState& AtMemoryPersistedStateManager::GetStateForField(
    const FieldGlobalId& field_id,
    const url::Origin& field_origin) {
  if (field_id_ != field_id) {
    ResetSearchState();
    field_id_ = field_id;
    field_origin_ = field_origin;
  }
  return search_state_;
}

void AtMemoryPersistedStateManager::OnPopupShown() {
  if (metrics_recorder_) {
    metrics_recorder_->OnStateRestored(GetStateRestored(search_state_));
  }
}

void AtMemoryPersistedStateManager::OnFilterChanged(
    std::u16string_view filter) {
  CHECK(field_id_);
  if (filter.empty()) {
    search_state_ = {};
    search_state_timer_.Stop();
    return;
  }
  search_state_.filter = filter;
  search_state_.suggestions.clear();
  search_state_.status = AtMemorySearchStatus::kQueryModified;
  RestartSearchStateTimer();
}

void AtMemoryPersistedStateManager::OnFilterSubmitted(
    const std::u16string& filter) {
  CHECK(field_id_);
  search_state_.filter = filter;
  // Suggestions of a previous search must not be mistaken for (partial)
  // results of the new one.
  search_state_.suggestions.clear();
  search_state_.status = AtMemorySearchStatus::kSearching;
  RestartSearchStateTimer();
}

void AtMemoryPersistedStateManager::OnSuggestionsChanged(
    std::vector<Suggestion> suggestions) {
  // The suggestions shown in the 0-state are not persisted.
  if (search_state_.status == AtMemorySearchStatus::kZeroState) {
    return;
  }
  if (search_state_.status == AtMemorySearchStatus::kSearching &&
      !suggestions.empty()) {
    search_state_.status = AtMemorySearchStatus::kPartialResponseReceived;
  }
  search_state_.suggestions = std::move(suggestions);
  search_state_.insertion_time = base::Time::Now();
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
    const base::Time insertion_time = base::Time::Now();

    if (it != previously_filled_suggestions_.end()) {
      it->insertion_time = insertion_time;
      std::rotate(it, it + 1, previously_filled_suggestions_.end());
    } else {
      if (previously_filled_suggestions_.size() >=
          kMaxPreviouslyFilledSuggestions) {
        previously_filled_suggestions_.erase(
            previously_filled_suggestions_.begin());
      }
      previously_filled_suggestions_.push_back(ExpiringSuggestion{
          .suggestion = suggestion_to_store, .insertion_time = insertion_time});
    }
    RestartPreviouslyFilledSuggestionsTimer();
  }
  ResetSearchState();
}

void AtMemoryPersistedStateManager::SetMetricsRecorder(
    std::unique_ptr<AtMemoryMetricsRecorder> metrics_recorder) {
  metrics_recorder_ = std::move(metrics_recorder);
  // With search statefulness, `AtMemoryManager` only sets a recorder while the
  // state is anchored to the popup's field, and the recorder then shares the
  // search state's TTL. Without it, `field_id_` is never set and the recorder
  // is instead reset by `AtMemoryManager` when the popup is hidden.
  // TODO(crbug.com/535486238): Replace the condition with `CHECK(field_id_)`
  // once `kAutofillAtMemorySearchStatefulness` is fully launched.
  if (field_id_) {
    RestartSearchStateTimer();
  }
}

std::unique_ptr<AtMemoryMetricsRecorder>
AtMemoryPersistedStateManager::TakeMetricsRecorder() {
  return std::move(metrics_recorder_);
}

bool AtMemoryPersistedStateManager::IsSearching() const {
  return search_state_.status == AtMemorySearchStatus::kSearching ||
         search_state_.status == AtMemorySearchStatus::kPartialResponseReceived;
}

void AtMemoryPersistedStateManager::StopSearching(
    AtMemoryQueryCompleted query_completed) {
  if (!IsSearching()) {
    return;
  }
  search_state_.suggestions.clear();
  search_state_.status = query_completed.value()
                             ? AtMemorySearchStatus::kResponseReceived
                             : AtMemorySearchStatus::kQueryModified;
}

void AtMemoryPersistedStateManager::PruneSuggestions(
    base::FunctionRef<bool(const Suggestion::AtMemoryPayload&, base::Time)>
        should_prune) {
  auto should_prune_suggestion = [&](const Suggestion& suggestion,
                                     base::Time insertion_time) -> bool {
    const Suggestion::AtMemoryPayload* payload =
        std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload);
    if (!payload || payload->is_personal_context_sourced) {
      return false;
    }
    return should_prune(*payload, insertion_time);
  };

  std::erase_if(
      previously_filled_suggestions_, [&](const ExpiringSuggestion& es) {
        return should_prune_suggestion(es.suggestion, es.insertion_time);
      });
  std::erase_if(search_state_.suggestions, [&](const Suggestion& suggestion) {
    return should_prune_suggestion(suggestion, search_state_.insertion_time);
  });
  // If all partial results were pruned, the search awaits results again.
  if (search_state_.status == AtMemorySearchStatus::kPartialResponseReceived &&
      search_state_.suggestions.empty()) {
    search_state_.status = AtMemorySearchStatus::kSearching;
  }
}

void AtMemoryPersistedStateManager::OnAddressDataChanged() {
  AddressDataManager* adm = address_data_manager_observation_.GetSource();
  if (!adm) {
    return;
  }
  PruneSuggestions([adm](const Suggestion::AtMemoryPayload& payload,
                         base::Time insertion_time) {
    return ShouldPruneSuggestionOnAddressDataChange(payload, insertion_time,
                                                    *adm);
  });
}

void AtMemoryPersistedStateManager::OnPaymentsDataChanged() {
  PaymentsDataManager* pdm = payments_data_manager_observation_.GetSource();
  if (!pdm) {
    return;
  }
  PruneSuggestions([pdm](const Suggestion::AtMemoryPayload& payload,
                         base::Time insertion_time) {
    return ShouldPruneSuggestionOnPaymentsDataChange(payload, insertion_time,
                                                     *pdm);
  });
}

void AtMemoryPersistedStateManager::OnEntityInstancesChanged() {
  EntityDataManager* edm = entity_data_manager_observation_.GetSource();
  if (!edm) {
    return;
  }
  PruneSuggestions([edm](const Suggestion::AtMemoryPayload& payload,
                         base::Time insertion_time) {
    return ShouldPruneSuggestionOnEntityDataChange(payload, insertion_time,
                                                   *edm);
  });
}

void AtMemoryPersistedStateManager::OnValuablesDataChanged() {
  ValuablesDataManager* vdm = valuables_data_manager_observation_.GetSource();
  if (!vdm) {
    return;
  }
  PruneSuggestions(
      [vdm](const Suggestion::AtMemoryPayload& payload, base::Time) {
        return ShouldPruneSuggestionOnValuablesDataChange(payload, *vdm);
      });
}

void AtMemoryPersistedStateManager::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.is_from_expiration() ||
      deletion_info.deletion_reason() ==
          history::DeletionInfo::Reason::kDeleteAllForeignVisits ||
      !deletion_info.time_range().IsValid()) {
    return;
  }
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

void AtMemoryPersistedStateManager::OnContextSourceDisabled() {
  // Cached search results and previously filled suggestions may contain data
  // from the disabled source. Clear everything (instead of filtering by source)
  // so that in-flight queries, which may still return data from that source,
  // are cancelled as well.
  Reset();
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
      return;
    case AutofillManager::LifecycleState::kActive:
      // A main frame that turns from inactive to active becomes the primary
      // main frame. This happens when a prerendered page is activated or a
      // BFCached page is restored. Such navigations do not emit
      // `kPendingReset`, so the state of the previous page is reset here.
      // (`kPendingReset -> kActive` follows a reset that was already handled.)
      if (old_state != AutofillManager::LifecycleState::kInactive) {
        return;
      }
      break;
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
  search_state_ = {};
  metrics_recorder_.reset();
}

void AtMemoryPersistedStateManager::OnSearchStateTimerExpired() {
  const bool has_suggestions = !search_state_.suggestions.empty();
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
      previously_filled_suggestions_, {}, &ExpiringSuggestion::insertion_time);
  const base::TimeDelta delay = std::max(
      base::TimeDelta(), min_it->expiration_time() - base::Time::Now());
  previously_filled_suggestions_timer_.Start(
      FROM_HERE, delay, this,
      &AtMemoryPersistedStateManager::RemoveExpiredPreviouslyFilledSuggestions);
}

void AtMemoryPersistedStateManager::RemoveExpiredPreviouslyFilledSuggestions() {
  const base::Time now = base::Time::Now();
  std::erase_if(previously_filled_suggestions_,
                [now](const ExpiringSuggestion& entry) {
                  return entry.expiration_time() <= now;
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
