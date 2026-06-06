// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager.h"
#include "components/personal_context/core/personal_context_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace personal_context {
class PersonalContextEnablementService;
class PersonalContextService;
}  // namespace personal_context

namespace autofill {

class PersonalContextAccessManagerImpl : public PersonalContextAccessManager {
 public:
  // The TTL for prefetched (masked/non-SPII) entities.
  static constexpr base::TimeDelta kPrefetchedEntitiesCacheTTL =
      base::Minutes(30);
  // The TTL for unmasked sensitive PII (SPII) entities.
  static constexpr base::TimeDelta kUnmaskedSpiiCacheTTL = base::Minutes(1);

  PersonalContextAccessManagerImpl(
      personal_context::PersonalContextService* personal_context_service,
      personal_context::PersonalContextEnablementService*
          personal_context_enablement_service);

  PersonalContextAccessManagerImpl(const PersonalContextAccessManagerImpl&) =
      delete;
  PersonalContextAccessManagerImpl& operator=(
      const PersonalContextAccessManagerImpl&) = delete;

  ~PersonalContextAccessManagerImpl() override;

  // PersonalContextAccessManager:
  void PrefetchAmbientAutofillContext(
      base::span<const EntityType> requested_types) override;
  std::optional<EntityInstance> GetCachedEntity(
      const EntityInstance::EntityId& id) const override;
  void GetUnmaskedSpiiEntity(const EntityInstance::EntityId& id,
                             GetUnmaskedSpiiEntityCallback callback) override;
  std::vector<EntityInstance> GetCachedEntities() const override;
  bool IsTypeCached(EntityTypeName type_name) const override;

 private:
  friend class PersonalContextAccessManagerImplTestApi;

  // Resets the cache state for `type_name` by clearing both the prefetched
  // (masked) entities and the unmasked SPII entities of this type. This ensures
  // that refreshing or invalidating prefetched data also invalidates any
  // corresponding unmasked sensitive data.
  void ResetCacheForType(EntityTypeName type_name);

  // Handles the asynchronous result of the ambient autofill context fetch.
  void OnPrefetchAmbientAutofillContextComplete(
      std::vector<EntityType> requested_types,
      personal_context::FetchContextResult result);

  // Caches a batch of prefetched `entities` and schedules new invalidations
  // after `kPrefetchedEntitiesCacheTTL`.
  void CachePrefetchedEntities(
      absl::flat_hash_map<EntityTypeName, std::vector<EntityInstance>>
          entities);

  // Caches an unmasked SPII `entity`, so it can be refilled without an
  // additional network round trip for `kUnmaskedSpiiCacheTTL`.
  void CacheUnmaskedSpiiEntity(EntityInstance entity);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<personal_context::PersonalContextEnablementService>
      personal_context_enablement_service_;

  // Cache of prefetched entity instances (containing masked/obfuscated values).
  //
  // **Eviction Mechanism**: Managed **per entity type** (not per individual
  // entity). When a type is prefetched, its lifetime is tracked in
  // `cached_types_`. After `kPrefetchedEntitiesCacheTTL` the entire
  // type expires, and all entities belonging to this type are evicted together
  // from this cache.
  //
  // **Interaction with SPII Cache**:
  // - When a type is explicitly reset or updated with new prefetched entities
  //   (via `ResetCacheForType`), both this cache and the `unmasked_spii_cache_`
  //   are cleared for that type to prevent stale unmasked data.
  // - Natural expiration of this cache also forcibly evicts all entries of this
  //   type from `unmasked_spii_cache_`.
  base::flat_set<EntityInstance, EntityInstance::CompareByGuid>
      prefetched_entity_cache_;

  // Cache of unmasked sensitive PII (SPII) entity instances.
  //
  // **Eviction Mechanism**: Managed **per individual entity** (not per type).
  // When an entity is individually unmasked, it is added here, and a separate
  // task is scheduled to evict just this entity after `kUnmaskedSpiiCacheTTL`.
  //
  // **Interaction with Prefetched Cache**:
  // - This cache is dependent on the freshness of the prefetched data. If the
  //   prefetched cache for a type is reset, updated or naturally expires, all
  //   unmasked entities of that type are immediately evicted from this cache
  //   (via `ResetCacheForType`). This ensures we do not serve unmasked SPII for
  //   entities that have been removed or updated in the masked cache, or when
  //   the prefetch cache has expired.
  base::flat_set<EntityInstance, EntityInstance::CompareByGuid>
      unmasked_spii_cache_;

  // Entity types for which their corresponding prefetched entities are within
  // their TTL.
  base::flat_set<EntityTypeName> cached_types_;

  base::WeakPtrFactory<PersonalContextAccessManagerImpl> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
