// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager_impl.h"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "components/prefs/pref_service.h"
#include "net/base/backoff_entry.h"

namespace autofill {

namespace {

// Configuration for exponential backoff on failed prefetch requests.
// Subsequent prefetch requests are blocked until the backoff delay expires
// (starts at 1s, doubles for each consecutive failure, capped at 1 hour).
constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Seconds(1).InMilliseconds(),
    .multiply_factor = 2.0,
    .jitter_factor = 0.0,
    .maximum_backoff_ms = base::Hours(1).InMilliseconds(),
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false};

bool IsPersonalContextEligible(
    personal_context::PersonalContextEligibilityState state) {
  using enum personal_context::PersonalContextEligibilityState;
  switch (state) {
    case kDisabledNotEligible:
    case kDisabledNeedsOptIn:
      return false;
    case kEligible:
      return true;
  }
}

personal_context::proto::ContextMemoryAmbientAutofillRequest
CreateAmbientAutofillRequest(base::span<EntityType> types,
                             bool return_spii_presence) {
  personal_context::proto::ContextMemoryAmbientAutofillRequest request;
  for (const EntityType& type : types) {
    request.add_requested_types(
        AutofillEntityTypeToPersonalContextEntityType(type));
  }
  request.set_return_spii_presence(return_spii_presence);
  return request;
}

bool IsPersonalContextSpiiType(EntityType type) {
  return GetPersonalContextSpiiType(
             type, EntityInstance::RecordType::kPersonalContext) ==
         EntityInstance::PersonalContextSpiiType::kSpii;
}

}  // namespace

AutofillAiPersonalContextAccessManagerImpl::
    AutofillAiPersonalContextAccessManagerImpl(
        personal_context::PersonalContextService* personal_context_service,
        personal_context::PersonalContextEligibilityService*
            personal_context_eligibility_service,
        PrefService* pref_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      personal_context_eligibility_service_(
          CHECK_DEREF(personal_context_eligibility_service)),
      pref_service_(pref_service) {
  eligibility_service_observation_.Observe(
      personal_context_eligibility_service);
  if (pref_service_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::BindRepeating(&AutofillAiPersonalContextAccessManagerImpl::
                                OnPersonalContextSettingsToggleChanged,
                            base::Unretained(this)));
  }
}

AutofillAiPersonalContextAccessManagerImpl::
    ~AutofillAiPersonalContextAccessManagerImpl() = default;

void AutofillAiPersonalContextAccessManagerImpl::PrefetchContext(
    base::span<const EntityType> requested_types) {
  // Types to request in Request 1 (which includes all non-SPII types and any
  // SPII types for which we want to check presence signals).
  std::vector<EntityType> non_spii_and_presence_to_request;
  // SPII types for which we want to fetch the actual masked entity data in
  // Request 2.
  std::vector<EntityType> spii_to_request;

  for (const EntityType& type : requested_types) {
    if (ShouldRequestType(type)) {
      non_spii_and_presence_to_request.push_back(type);
      SetTypeStatus(type, RequestStatus::kPending);

      if (IsPersonalContextSpiiType(type)) {
        spii_to_request.push_back(type);
      }
    }
  }

  if (non_spii_and_presence_to_request.empty()) {
    NotifyPrefetchStatusObservers(base::span<const EntityInstance>());
    return;
  }

  const bool has_spii_types = !spii_to_request.empty();

  // Request 1: collects non-spii entities and asks for spii presence if any of
  // the requested_types contains SPII types.
  {
    auto request =
        CreateAmbientAutofillRequest(non_spii_and_presence_to_request,
                                     /*return_spii_presence=*/has_spii_types);
    personal_context_service_->FetchContext(
        personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
        request,
        /*options=*/{},
        base::BindOnce(&AutofillAiPersonalContextAccessManagerImpl::
                           OnPrefetchContextRequestComplete,
                       weak_factory_.GetWeakPtr(),
                       std::move(non_spii_and_presence_to_request),
                       /*requested_spii_presence=*/has_spii_types));
  }

  // Request 2: collects spii entities without asking for spii presence.
  if (has_spii_types) {
    auto request = CreateAmbientAutofillRequest(spii_to_request,
                                                /*return_spii_presence=*/false);
    personal_context_service_->FetchContext(
        personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
        request,
        /*options=*/{},
        base::BindOnce(&AutofillAiPersonalContextAccessManagerImpl::
                           OnPrefetchContextRequestComplete,
                       weak_factory_.GetWeakPtr(), std::move(spii_to_request),
                       /*requested_spii_presence=*/false));
  }
}

void AutofillAiPersonalContextAccessManagerImpl::
    OnPrefetchContextRequestComplete(
        std::vector<EntityType> requested_types,
        bool requested_spii_presence,
        personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    HandleFailedResponse(requested_types, requested_spii_presence);
    return;
  }

  base::expected<std::vector<ParsedEntity>,
                 personal_context::ContextMemoryError>
      parsed_entities =
          ExtractEntitiesFromResponse(result.response.value().value());

  if (!parsed_entities.has_value()) {
    HandleFailedResponse(requested_types, requested_spii_presence);
    return;
  }

  std::vector<EntityType> prefetched_types;
  std::vector<SpiiEntityPresenceSignal> presence_signals;

  for (const EntityType& type : requested_types) {
    if (!requested_spii_presence || !IsPersonalContextSpiiType(type)) {
      prefetched_types.push_back(type);
    }
  }

  for (const ParsedEntity& parsed_entity : *parsed_entities) {
    if (const SpiiEntityPresenceSignal* signal =
            std::get_if<SpiiEntityPresenceSignal>(&parsed_entity.instance)) {
      presence_signals.push_back(*signal);
    }
  }

  ProcessPrefetchedEntities(std::move(prefetched_types),
                            std::move(*parsed_entities));
}

base::expected<
    std::vector<AutofillAiPersonalContextAccessManagerImpl::ParsedEntity>,
    personal_context::ContextMemoryError>
AutofillAiPersonalContextAccessManagerImpl::ExtractEntitiesFromResponse(
    std::string_view serialized_response) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  if (!response.ParseFromString(serialized_response)) {
    return base::unexpected(
        personal_context::ContextMemoryError::FromExecutionError(
            personal_context::ContextMemoryError::ExecutionError::
                kResponseParseError));
  }

  std::vector<ParsedEntity> entities;
  entities.reserve(response.entities_size());
  for (const personal_context::proto::Entity& entity : response.entities()) {
    if (entity.entity_case() ==
        personal_context::proto::Entity::kSensitivePiiPresence) {
      if (std::optional<EntityType> type =
              ToEntityType(entity.sensitive_pii_presence().type())) {
        entities.push_back({*type, entity});
      }
    } else {
      if (std::optional<EntityInstance> converted =
              PersonalContextEntityToEntityInstance(entity)) {
        entities.push_back({std::move(*converted), entity});
      }
    }
  }
  return entities;
}

void AutofillAiPersonalContextAccessManagerImpl::GetUnmaskedSpiiEntity(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback) {
  if (auto it = unmasked_spii_cache_.find(id);
      it != unmasked_spii_cache_.end()) {
    std::move(callback).Run(*it);
    return;
  }

  personal_context::proto::Entity* proto_entity =
      base::FindOrNull(prefetched_proto_cache_, id);
  if (!proto_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  personal_context::proto::FetchPiiEntitiesRequest request;
  request.set_feature(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  *request.add_masked_entities() = *proto_entity;

  personal_context_service_->FetchPiiEntities(
      request, /*options=*/{},
      base::BindOnce(&AutofillAiPersonalContextAccessManagerImpl::
                         OnFetchPiiEntitiesComplete,
                     weak_factory_.GetWeakPtr(), id, std::move(callback)));
}

void AutofillAiPersonalContextAccessManagerImpl::OnFetchPiiEntitiesComplete(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback,
    personal_context::FetchPiiEntitiesResult result) {
  if (!result.response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const personal_context::proto::FetchPiiEntitiesResponse& response =
      result.response.value();
  if (response.entities().empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<EntityInstance> unmasked_entity =
      PersonalContextEntityToEntityInstance(response.entities(0),
                                            /*is_masked=*/false);
  if (!unmasked_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  EntityInstance final_entity = unmasked_entity->CopyWithNewEntityId(id);
  CacheUnmaskedSpiiEntity(final_entity);
  std::move(callback).Run(std::move(final_entity));
}

bool AutofillAiPersonalContextAccessManagerImpl::IsTypePrefetched(
    EntityType type) const {
  const RequestState* request_state = base::FindOrNull(prefetch_state_, type);
  return request_state && request_state->status == RequestStatus::kSuccess;
}

void AutofillAiPersonalContextAccessManagerImpl::AddObserver(
    AutofillAiPersonalContextAccessManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void AutofillAiPersonalContextAccessManagerImpl::RemoveObserver(
    AutofillAiPersonalContextAccessManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

AutofillAiPersonalContextAccessManager::RequestStatus
AutofillAiPersonalContextAccessManagerImpl::GetPrefetchStatusByEntityType(
    EntityType type) const {
  if (const RequestState* state = base::FindOrNull(prefetch_state_, type)) {
    return state->status;
  }
  return RequestStatus::kNotStarted;
}

bool AutofillAiPersonalContextAccessManagerImpl::ServerHasDataAvailable(
    EntityType type) const {
  return spii_presence_signal_cache_.contains(type);
}

void AutofillAiPersonalContextAccessManagerImpl::ResetStateForType(
    EntityType type) {
  // Clear existing proto entities of this type.
  absl::erase_if(prefetched_proto_cache_, [type](const auto& entry) {
    return ToEntityType(entry.second.entity_case()) == type;
  });
  // Clear unmasked SPII of this type.
  base::EraseIf(unmasked_spii_cache_, [type](EntityInstance& entity) {
    return entity.type() == type;
  });

  prefetch_state_.erase(type);
  observers_.Notify(&AutofillAiPersonalContextAccessManager::Observer::
                        OnMaskedEntityTypeEvicted,
                    *this, type);
}

void AutofillAiPersonalContextAccessManagerImpl::ProcessPrefetchedEntities(
    std::vector<EntityType> requested_types,
    std::vector<ParsedEntity> parsed_entities) {
  // Evict existing entities for the `requested_types`.
  for (EntityType type : requested_types) {
    ResetStateForType(type);
    SetTypeStatus(type, RequestStatus::kSuccess);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AutofillAiPersonalContextAccessManagerImpl::ResetStateForType,
            weak_factory_.GetWeakPtr(), type),
        kPrefetchedEntitiesAndSignalsCacheTTL);
  }
  // Populates the proto cache and notify observers about the fetched entities.
  // Also cache presence signals.
  std::vector<EntityInstance> entities;
  entities.reserve(parsed_entities.size());
  for (ParsedEntity& entity : parsed_entities) {
    if (const EntityInstance* e_instance =
            std::get_if<EntityInstance>(&entity.instance)) {
      prefetched_proto_cache_.emplace(e_instance->guid(),
                                      std::move(entity.proto));
      entities.push_back(std::move(*e_instance));
    } else {
      CachePresenceSignal(std::get<SpiiEntityPresenceSignal>(entity.instance));
    }
  }

  NotifyPrefetchStatusObservers(entities);
}

void AutofillAiPersonalContextAccessManagerImpl::CacheUnmaskedSpiiEntity(
    EntityInstance entity) {
  EntityInstance::EntityId id = entity.guid();
  auto [it, inserted] = unmasked_spii_cache_.insert(std::move(entity));
  if (!inserted) {
    return;
  }
  // Clear the cache entry after `kUnmaskedSpiiCacheTTL`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AutofillAiPersonalContextAccessManagerImpl>
                 access_manager,
             const EntityInstance::EntityId& id) {
            if (!access_manager) {
              return;
            }
            // Remove if exists.
            access_manager->unmasked_spii_cache_.erase(id);
          },
          weak_factory_.GetWeakPtr(), id),
      kUnmaskedSpiiCacheTTL);
}

void AutofillAiPersonalContextAccessManagerImpl::CachePresenceSignal(
    SpiiEntityPresenceSignal signal) {
  auto [it, inserted] = spii_presence_signal_cache_.insert(signal);
  if (!inserted) {
    return;
  }
  // Clear the cache entry after `kPrefetchedEntitiesAndSignalsCacheTTL`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AutofillAiPersonalContextAccessManagerImpl>
                 access_manager,
             const SpiiEntityPresenceSignal signal_to_remove) {
            if (!access_manager) {
              return;
            }
            // Remove if exists.
            access_manager->spii_presence_signal_cache_.erase(signal_to_remove);
          },
          weak_factory_.GetWeakPtr(), signal),
      kPrefetchedEntitiesAndSignalsCacheTTL);
}

void AutofillAiPersonalContextAccessManagerImpl::WipeCache() {
  // Invalidate weak pointers to cancel any pending fetches.
  weak_factory_.InvalidateWeakPtrs();
  // Copy the keys since `ResetStateForType()` invalidates iterators to
  // `prefetch_state_`.
  std::vector<EntityType> prefetched_types = base::ToVector(
      prefetch_state_, [](const auto& item) { return item.first; });
  for (EntityType type : prefetched_types) {
    ResetStateForType(type);
  }
  spii_presence_signal_cache_.clear();
}

void AutofillAiPersonalContextAccessManagerImpl::OnEligibilityStateChanged(
    personal_context::PersonalContextEligibilityState new_state) {
  if (!IsPersonalContextEligible(new_state)) {
    WipeCache();
  }
}

void AutofillAiPersonalContextAccessManagerImpl::
    OnPersonalContextSettingsToggleChanged() {
  if (pref_service_ &&
      !pref_service_->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    WipeCache();
  }
}

bool AutofillAiPersonalContextAccessManagerImpl::ShouldRequestType(
    EntityType type) const {
  const RequestState* request_state = base::FindOrNull(prefetch_state_, type);
  if (!request_state) {
    return true;
  }

  switch (request_state->status) {
    case RequestStatus::kPending:
      return false;
    case RequestStatus::kSuccess:
      if (base::TimeTicks::Now() - request_state->last_update_time >
          kPrefetchedEntitiesAndSignalsCacheTTL) {
        return true;
      }
      return false;
    case RequestStatus::kFailure:
      return ShouldRetryAfterFailure(*request_state);
    case RequestStatus::kNotStarted:
      return true;
  }
}

bool AutofillAiPersonalContextAccessManagerImpl::ShouldRetryAfterFailure(
    const RequestState& state) const {
  return state.backoff_entry && !state.backoff_entry->ShouldRejectRequest();
}

void AutofillAiPersonalContextAccessManagerImpl::SetTypeStatus(
    EntityType type,
    RequestStatus status) {
  RequestState& state = prefetch_state_[type];
  state.status = status;
  state.last_update_time = base::TimeTicks::Now();

  if (!state.backoff_entry) {
    state.backoff_entry = std::make_unique<net::BackoffEntry>(&kBackoffPolicy);
  }

  switch (status) {
    case RequestStatus::kPending:
      break;
    case RequestStatus::kSuccess:
      state.backoff_entry->Reset();
      break;
    case RequestStatus::kFailure:
      state.backoff_entry->InformOfRequest(/*succeeded=*/false);
      break;
    case RequestStatus::kNotStarted:
      break;
  }
}

void AutofillAiPersonalContextAccessManagerImpl::HandleFailedResponse(
    base::span<const EntityType> requested_types,
    bool requested_spii_presence) {
  for (const EntityType& type : requested_types) {
    // SPII types are handled during SPII requests, instead of their presence.
    if (requested_spii_presence && IsPersonalContextSpiiType(type)) {
      continue;
    }
    SetTypeStatus(type, RequestStatus::kFailure);
  }
  NotifyPrefetchStatusObservers({});
}

void AutofillAiPersonalContextAccessManagerImpl::NotifyPrefetchStatusObservers(
    std::optional<base::span<const EntityInstance>> entities) {
  observers_.Notify(&AutofillAiPersonalContextAccessManager::Observer::
                        OnPrefetchContextComplete,
                    *this, entities);
}

}  // namespace autofill
