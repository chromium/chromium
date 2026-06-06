// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
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

}  // namespace

PersonalContextAccessManagerImpl::PersonalContextAccessManagerImpl(
    personal_context::PersonalContextService* personal_context_service,
    personal_context::PersonalContextEnablementService*
        personal_context_enablement_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      personal_context_enablement_service_(
          CHECK_DEREF(personal_context_enablement_service)) {}

PersonalContextAccessManagerImpl::~PersonalContextAccessManagerImpl() = default;

void PersonalContextAccessManagerImpl::PrefetchAmbientAutofillContext(
    base::span<const EntityType> requested_types) {
  std::vector<EntityType> non_cached_requested_types;
  for (const EntityType& type : requested_types) {
    if (!IsTypeCached(type.name())) {
      non_cached_requested_types.push_back(type);
    }
  }

  if (non_cached_requested_types.empty()) {
    return;
  }

  personal_context::proto::ContextMemoryAmbientAutofillRequest request;
  for (const EntityType& type : non_cached_requested_types) {
    request.add_requested_types(
        AutofillEntityTypeToPersonalContextEntityType(type));
  }

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, request,
      /*options=*/{},
      base::BindOnce(&PersonalContextAccessManagerImpl::
                         OnPrefetchAmbientAutofillContextComplete,
                     weak_factory_.GetWeakPtr(),
                     std::move(non_cached_requested_types)));
}

void PersonalContextAccessManagerImpl::OnPrefetchAmbientAutofillContextComplete(
    std::vector<EntityType> requested_types,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    return;
  }

  base::expected<std::vector<EntityInstance>,
                 personal_context::ContextMemoryError>
      entities = ExtractEntitiesFromResponse(result.response.value().value());

  if (!entities.has_value()) {
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
  return cached_types_.contains(type_name);
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

  cached_types_.erase(type_name);
}

void PersonalContextAccessManagerImpl::CachePrefetchedEntities(
    absl::flat_hash_map<EntityTypeName, std::vector<EntityInstance>> entities) {
  // For each type present, insert new data and schedule wipeout.
  for (auto& [type_name, type_entities] : entities) {
    prefetched_entity_cache_.insert(
        std::make_move_iterator(type_entities.begin()),
        std::make_move_iterator(type_entities.end()));
    cached_types_.insert(type_name);

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

}  // namespace autofill
