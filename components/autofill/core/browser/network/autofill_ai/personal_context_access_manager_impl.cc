// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <algorithm>
#include <optional>
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
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"

namespace autofill {

namespace {

// Parses the raw protobuf string and converts it into a vector of
// EntityInstances. Returns an unexpected error if parsing fails.
base::expected<std::vector<EntityInstance>,
               personal_context::ContextMemoryError>
ExtractEntitiesFromResponse(const std::string& serialized_response) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  if (!response.ParseFromString(serialized_response)) {
    return base::unexpected(
        personal_context::ContextMemoryError::FromExecutionError(
            personal_context::ContextMemoryError::ExecutionError::
                kResponseParseError));
  }

  std::vector<EntityInstance> entities;
  entities.reserve(response.entities_size());
  for (const auto& entity : response.entities()) {
    if (std::optional<EntityInstance> converted =
            PersonalContextEntityToEntityInstance(entity)) {
      entities.push_back(std::move(*converted));
    }
  }

  return entities;
}

bool IsPersonalContextEnabled(
    personal_context::PersonalContextEnablementState state) {
  using personal_context::PersonalContextEnablementState;
  switch (state) {
    case PersonalContextEnablementState::kDisabledNotEligible:
    case PersonalContextEnablementState::kDisabledNeedsOptIn:
    case PersonalContextEnablementState::
        kDisabledViaPersonalIntelligenceInAutofillToggle:
      return false;
    case PersonalContextEnablementState::kEnabled:
    case PersonalContextEnablementState::kEnabledShouldShowNotice:
      return true;
  }
}

bool IsPrefetchAmbientAutofillContextEnabled(
    personal_context::PersonalContextEnablementService& enablement_service) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAmbientAutofill)) {
    return false;
  }

  return IsPersonalContextEnabled(enablement_service.GetEnablementState());
}

}  // namespace

PersonalContextAccessManagerImpl::PersonalContextAccessManagerImpl(
    personal_context::PersonalContextService* personal_context_service,
    personal_context::PersonalContextEnablementService*
        personal_context_enablement_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      personal_context_enablement_service_(
          CHECK_DEREF(personal_context_enablement_service)) {
  enablement_service_observation_.Observe(personal_context_enablement_service);
}

PersonalContextAccessManagerImpl::~PersonalContextAccessManagerImpl() = default;

void PersonalContextAccessManagerImpl::PrefetchAmbientAutofillContext(
    base::span<const EntityType> requested_types) {
  if (!IsPrefetchAmbientAutofillContextEnabled(
          *personal_context_enablement_service_)) {
    return;
  }

  std::vector<EntityType> types_to_request;
  for (const EntityType& type : requested_types) {
    if (ShouldRequestType(type.name())) {
      types_to_request.push_back(type);
      SetTypeStatus(type.name(), RequestState::Status::kPending);
    }
  }

  if (types_to_request.empty()) {
    return;
  }

  personal_context::proto::ContextMemoryAmbientAutofillRequest request;
  for (const EntityType& type : types_to_request) {
    request.add_requested_types(
        AutofillEntityTypeToPersonalContextEntityType(type));
  }

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, request,
      /*options=*/{},
      base::BindOnce(&PersonalContextAccessManagerImpl::
                         OnPrefetchAmbientAutofillContextComplete,
                     weak_factory_.GetWeakPtr(), std::move(types_to_request)));
}

void PersonalContextAccessManagerImpl::OnPrefetchAmbientAutofillContextComplete(
    std::vector<EntityType> requested_types,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    for (const EntityType& type : requested_types) {
      SetTypeStatus(type.name(), RequestState::Status::kFailure);
    }
    return;
  }

  base::expected<std::vector<EntityInstance>,
                 personal_context::ContextMemoryError>
      entities = ExtractEntitiesFromResponse(result.response.value().value());

  if (!entities.has_value()) {
    for (const EntityType& type : requested_types) {
      SetTypeStatus(type.name(), RequestState::Status::kFailure);
    }
    return;
  }

  absl::flat_hash_map<EntityTypeName, std::vector<EntityInstance>>
      grouped_entities;
  // Initialize requested types results, including entries for empty responses
  // so that EntityTypes without responses are not fetched over and over again.
  for (EntityType type : requested_types) {
    grouped_entities[type.name()] = std::vector<EntityInstance>();
  }
  // Group entities by type.
  for (EntityInstance& entity : *entities) {
    grouped_entities[entity.type().name()].push_back(std::move(entity));
  }

  CachePrefetchedEntities(std::move(grouped_entities));
}

std::optional<EntityInstance> PersonalContextAccessManagerImpl::GetCachedEntity(
    const EntityInstance::EntityId& id) const {
  if (auto it = prefetched_entity_cache_.find(id);
      it != prefetched_entity_cache_.end()) {
    return *it;
  }
  return std::nullopt;
}

void PersonalContextAccessManagerImpl::GetUnmaskedSpiiEntity(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback) {
  if (auto it = unmasked_spii_cache_.find(id);
      it != unmasked_spii_cache_.end()) {
    std::move(callback).Run(*it);
    return;
  }

  // TODO(crbug.com/516721244): Trigger a network request to unmask the entity,
  // cache the result, and run the callback.
  std::move(callback).Run(std::nullopt);
}

std::vector<EntityInstance>
PersonalContextAccessManagerImpl::GetCachedEntities() const {
  return base::ToVector(prefetched_entity_cache_);
}

bool PersonalContextAccessManagerImpl::IsTypeCached(
    EntityTypeName type_name) const {
  const RequestState* request_state = base::FindOrNull(cache_state_, type_name);
  return request_state &&
         request_state->status == RequestState::Status::kSuccess;
}

void PersonalContextAccessManagerImpl::ResetCacheForType(
    EntityTypeName type_name) {
  const auto is_entity_type_name = [type_name](EntityInstance& entity) {
    return entity.type().name() == type_name;
  };
  // Clear existing entities of this type.
  base::EraseIf(prefetched_entity_cache_, is_entity_type_name);

  // Clear unmasked SPII of this type.
  base::EraseIf(unmasked_spii_cache_, is_entity_type_name);

  cache_state_.erase(type_name);
}

void PersonalContextAccessManagerImpl::CachePrefetchedEntities(
    absl::flat_hash_map<EntityTypeName, std::vector<EntityInstance>> entities) {
  for (auto& [type_name, type_entities] : entities) {
    ResetCacheForType(type_name);
    prefetched_entity_cache_.insert(
        std::make_move_iterator(type_entities.begin()),
        std::make_move_iterator(type_entities.end()));
    SetTypeStatus(type_name, RequestState::Status::kSuccess);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PersonalContextAccessManagerImpl::ResetCacheForType,
                       weak_factory_.GetWeakPtr(), type_name),
        kPrefetchedEntitiesCacheTTL);
  }
}

void PersonalContextAccessManagerImpl::CacheUnmaskedSpiiEntity(
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
          [](base::WeakPtr<PersonalContextAccessManagerImpl> access_manager,
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

void PersonalContextAccessManagerImpl::OnEnablementStateChanged(
    personal_context::PersonalContextEnablementState new_state) {
  if (!IsPersonalContextEnabled(new_state)) {
    WipeCaches();
  }
}

void PersonalContextAccessManagerImpl::WipeCaches() {
  prefetched_entity_cache_.clear();
  unmasked_spii_cache_.clear();
  cache_state_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

bool PersonalContextAccessManagerImpl::ShouldRequestType(
    EntityTypeName type_name) const {
  const RequestState* request_state = base::FindOrNull(cache_state_, type_name);
  if (!request_state) {
    return true;
  }

  switch (request_state->status) {
    case RequestState::Status::kPending:
      return false;
    case RequestState::Status::kSuccess:
      if (base::TimeTicks::Now() - request_state->last_update_time >
          kPrefetchedEntitiesCacheTTL) {
        return true;
      }
      return false;
    case RequestState::Status::kFailure:
      return ShouldRetryAfterFailure(*request_state);
  }

  return false;
}

bool PersonalContextAccessManagerImpl::ShouldRetryAfterFailure(
    const RequestState& state) const {
  // TODO(crbug.com/516721244): Implement.
  return true;
}

void PersonalContextAccessManagerImpl::SetTypeStatus(
    EntityTypeName type_name,
    RequestState::Status status) {
  RequestState& state = cache_state_[type_name];
  state.status = status;
  state.last_update_time = base::TimeTicks::Now();
  switch (status) {
    case RequestState::Status::kPending:
      break;
    case RequestState::Status::kSuccess:
      state.failure_count = 0;
      break;
    case RequestState::Status::kFailure:
      state.failure_count++;
      break;
  }
}

}  // namespace autofill
