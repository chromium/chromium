// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class PrefService;

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace autofill {

// Manages fetching masked and unmasked pContext entities. In particular:
// - Prefetches masked entities and broadcasts the result through its observer.
//   The masked entities are not cached by the access manager itself, but
//   instead by the EntityDataManager [though this is opaque to this class].
// - Unmasks masked entities. Results are cached by the class itself.
// - Schedules eviction of masked and unmasked entities:
//   - For masked entities, eviction notices are broadcast through the observer.
//   - For unmasked entities, the class handles the cache changes internally.
class PersonalContextAccessManagerImpl
    : public PersonalContextAccessManager,
      public personal_context::PersonalContextEnablementService::Observer {
 public:
  // The TTL for prefetched (masked/non-SPII) entities and presence signals.
  static constexpr base::TimeDelta kPrefetchedEntitiesAndSignalsCacheTTL =
      base::Minutes(30);
  // The TTL for unmasked sensitive PII (SPII) entities.
  static constexpr base::TimeDelta kUnmaskedSpiiCacheTTL = base::Minutes(1);

  PersonalContextAccessManagerImpl(
      personal_context::PersonalContextService* personal_context_service,
      personal_context::PersonalContextEnablementService*
          personal_context_enablement_service,
      PrefService* pref_service);

  PersonalContextAccessManagerImpl(const PersonalContextAccessManagerImpl&) =
      delete;
  PersonalContextAccessManagerImpl& operator=(
      const PersonalContextAccessManagerImpl&) = delete;

  ~PersonalContextAccessManagerImpl() override;

  // PersonalContextAccessManager:
  void PrefetchContext(base::span<const EntityType> requested_types) override;
  RequestStatus GetPrefetchStatusByEntityType(EntityType type) const override;
  void GetUnmaskedSpiiEntity(const EntityInstance::EntityId& id,
                             GetUnmaskedSpiiEntityCallback callback) override;
  bool IsTypePrefetched(EntityType type) const override;
  bool ServerHasDataAvailable(EntityType type) const override;
  void AddObserver(PersonalContextAccessManager::Observer* observer) override;
  void RemoveObserver(
      PersonalContextAccessManager::Observer* observer) override;

  // personal_context::PersonalContextEnablementService::Observer:
  void OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState new_state) override;

 private:
  friend class PersonalContextAccessManagerImplTestApi;
  using SpiiEntityPresenceSignal = EntityType;

  // Results of parsing the server response during prefetch requests. It bundles
  // the internal `EntityInstance` representation with its original
  // `personal_context::proto::Entity` received from the server. The original
  // proto is required for subsequent unmasking requests (see
  // `GetUnmaskedSpiiEntity`).
  struct ParsedEntity {
    std::variant<EntityInstance, SpiiEntityPresenceSignal> instance;
    personal_context::proto::Entity proto;
  };

  struct RequestState {
    RequestStatus status = RequestStatus::kNotStarted;
    base::TimeTicks last_update_time;
    std::unique_ptr<net::BackoffEntry> backoff_entry;
  };

  // Resets the prefetch and unmasked caches for all types, notifying observers
  // to evict any cached data.
  void WipeCache();

  // Callback triggered when the user-visible toggle in Autofill settings
  // changes.
  void OnPersonalContextSettingsToggleChanged();

  // Resets the state for `type` by:
  // - Evicting masked entities for all prefetched types.
  // - Clearing the unmasked entity cache.
  void ResetStateForType(EntityType type);

  // Handles the asynchronous result of the personal context fetch.
  // `requested_spii_presence` expresses whether SPII types are fetched
  // or only their presence is indicated
  void OnPrefetchContextRequestComplete(
      std::vector<EntityType> requested_types,
      bool requested_spii_presence,
      personal_context::FetchContextResult result);

  // Parses the raw protobuf string response and converts it into a vector of
  // EntityInstances. Returns an unexpected error if parsing fails.
  base::expected<std::vector<ParsedEntity>,
                 personal_context::ContextMemoryError>
  ExtractEntitiesFromResponse(std::string_view serialized_response);

  // Handles the asynchronous result of the SPII entities fetch.
  void OnFetchPiiEntitiesComplete(
      const EntityInstance::EntityId& id,
      GetUnmaskedSpiiEntityCallback callback,
      personal_context::FetchPiiEntitiesResult result);

  // Processes a batch of prefetched entities, by
  // - Updating the cache state.
  // - Scheduling eviction of the prefetched types.
  // - Scheduling eviction of spii presence signals.
  // - Notifying observers.
  void ProcessPrefetchedEntities(std::vector<EntityType> requested_types,
                                 std::vector<ParsedEntity> parsed_entities);

  // Returns true if a network request should be initiated for `type`.
  // This is true if the type is not cached, its cache TTL has expired, or a
  // previous fetch failed and is now eligible for a retry.
  bool ShouldRequestType(EntityType type) const;

  // Evaluates whether enough time has elapsed since the last failure to
  // attempt fetching the type again, taking backoff delays into account.
  bool ShouldRetryAfterFailure(const RequestState& state) const;

  // Marks the cache state for `type` as `status`. Updates the timestamp
  // to start the cache TTL timer and sets the appropriate failure count.
  void SetTypeStatus(EntityType type, RequestStatus status);

  // Notifies observers of the prefetch status.
  void NotifyPrefetchStatusObservers(
      std::optional<base::span<const EntityInstance>> entities);

  // Caches an unmasked SPII `entity`, so it can be refilled without an
  // additional network round trip for `kUnmaskedSpiiCacheTTL`.
  void CacheUnmaskedSpiiEntity(EntityInstance entity);

  // Caches a presence signal for an SPII `type`. Evicts the signal after
  // `kPrefetchedEntitiesAndSignalsCacheTTL` time.
  void CachePresenceSignal(SpiiEntityPresenceSignal signal);

  void HandleFailedResponse(base::span<const EntityType> requested_types,
                            bool requested_spii_presence);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<personal_context::PersonalContextEnablementService>
      personal_context_enablement_service_;
  const raw_ptr<PrefService> pref_service_;

  // Map from EntityId to the original proto Entity received during prefetch.
  absl::flat_hash_map<EntityInstance::EntityId, personal_context::proto::Entity>
      prefetched_proto_cache_;

  // Cache of unmasked sensitive PII (SPII) entity instances.
  //
  // **Eviction Mechanism**: Managed **per individual entity** (not per type).
  // When an entity is individually unmasked, it is added here, and a separate
  // task is scheduled to evict just this entity after `kUnmaskedSpiiCacheTTL`.
  //
  // **Interaction with Prefetched entities**:
  // When a prefetched entity type is evicted, all unmasked entities of the same
  // type are removed as well. This ensures we do not serve unmasked SPII when
  // the prefetch state has expired.
  base::flat_set<EntityInstance, EntityInstance::CompareByGuid>
      unmasked_spii_cache_;

  // Maps entity types to their current prefetch request/response state.
  base::flat_map<EntityType, RequestState> prefetch_state_;

  // Cache of sensitive PII (SPII) presence signals indicating if sensitive
  // entities of a given type are available on the server.
  //
  // **Eviction Mechanism**: Managed per type. When a presence signal is
  // received, it is added here, and a separate task is scheduled to evict just
  // this signal after `kPrefetchedEntitiesAndSignalsCacheTTL`.
  base::flat_set<SpiiEntityPresenceSignal> spii_presence_signal_cache_;

  base::ObserverList<PersonalContextAccessManager::Observer> observers_;

  base::ScopedObservation<
      personal_context::PersonalContextEnablementService,
      personal_context::PersonalContextEnablementService::Observer>
      enablement_service_observation_{this};

  PrefChangeRegistrar pref_registrar_;

  base::WeakPtrFactory<PersonalContextAccessManagerImpl> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
