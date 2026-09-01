// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_

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
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class PrefService;

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace autofill {

// Manages fetching masked and unmasked pContext entities. In particular:
// - Prefetches masked entities and broadcasts the result through its observer.
//   The masked entities are not cached by the access manager itself, but
//   instead by the EntityDataManager [though this is opaque to this class].
// - Unmasks masked entities. Results are cached by the class itself.
// - Schedules eviction of masked and unmasked entities:
//   - For masked entities, eviction notices are broadcast through the observer.
//   - For unmasked entities, the class handles the cache changes internally.
class AutofillAiPersonalContextAccessManagerImpl
    : public AutofillAiPersonalContextAccessManager,
      public personal_context::PersonalContextEligibilityService::Observer,
      public subscription_eligibility::SubscriptionEligibilityService::Observer,
      public EntitySuppressionManager::Observer {
 public:
  // Represents the type of personal context network request sent to the server.
  enum class RequestType {
    // Request for non-sensitive data and presence signals for sensitive data.
    kNonSpiiAndPresence,
    // Request for masked sensitive data.
    kSpiiMasked,
    // Request for unmasking sensitive data.
    kSpiiUnmasking,
  };

  AutofillAiPersonalContextAccessManagerImpl(
      personal_context::PersonalContextService* personal_context_service,
      personal_context::PersonalContextEligibilityService*
          personal_context_eligibility_service,
      subscription_eligibility::SubscriptionEligibilityService*
          subscription_eligibility_service,
      PrefService* pref_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      EntitySuppressionManager* suppression_manager);

  AutofillAiPersonalContextAccessManagerImpl(
      const AutofillAiPersonalContextAccessManagerImpl&) = delete;
  AutofillAiPersonalContextAccessManagerImpl& operator=(
      const AutofillAiPersonalContextAccessManagerImpl&) = delete;

  ~AutofillAiPersonalContextAccessManagerImpl() override;

  // AutofillAiPersonalContextAccessManager:
  void PrefetchContext(DenseSet<EntityType> requested_types) override;
  RequestStatus GetPrefetchStatusByEntityType(EntityType type) const override;
  void GetUnmaskedSpiiEntity(const EntityInstance::EntityId& id,
                             GetUnmaskedSpiiEntityCallback callback) override;
  bool IsTypePrefetched(EntityType type) const override;
  bool ServerHasSpiiPresenceSignal(EntityType type) const override;
  void AddObserver(
      AutofillAiPersonalContextAccessManager::Observer* observer) override;
  void RemoveObserver(
      AutofillAiPersonalContextAccessManager::Observer* observer) override;

  // personal_context::PersonalContextEligibilityService::Observer:
  void OnEligibilityStateChanged(
      personal_context::PersonalContextEligibilityState new_state) override;

  // subscription_eligibility::SubscriptionEligibilityService::Observer:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

  // EntitySuppressionManager::Observer:
  void OnEntitySuppressionsChanged() override;

 private:
  friend class AutofillAiPersonalContextAccessManagerImplTestApi;
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
  void OnPrefetchContextRequestComplete(
      DenseSet<EntityType> requested_types,
      RequestType request_type,
      base::TimeTicks request_start_time,
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
      base::TimeTicks request_start_time,
      personal_context::FetchPiiEntitiesResult result);

  // Processes a batch of prefetched entities, by
  // - Updating the cache state.
  // - Scheduling eviction of the prefetched types.
  // - Scheduling eviction of spii presence signals.
  // - Notifying observers.
  void ProcessPrefetchedEntities(DenseSet<EntityType> prefetched_types,
                                 DenseSet<EntityType> requested_types,
                                 std::vector<ParsedEntity> parsed_entities);

  PersonalContextPrefetchTriggerResult DeterminePrefetchTriggerResult(
      EntityType type) const;

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
  // additional network round trip for the duration of
  // `kAutofillAmbientAutofillUnmaskedSpiiCacheTTL`.
  void CacheUnmaskedSpiiEntity(EntityInstance entity);

  // Caches a presence signal for an SPII `type`. Evicts the signal after
  // `kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL` time.
  void CachePresenceSignal(SpiiEntityPresenceSignal signal);

  // Handles a failed network response for a prefetch request targeting
  // `requested_types`. Sets their status to `kFailure` and notifies observers.
  // If `requested_spii_presence` is true, SPII types are excluded from the
  // failure status, as their outcome is governed by the dedicated SPII data
  // request.
  void HandleFailedResponse(DenseSet<EntityType> requested_types,
                            RequestType request_type);

  // Logs the total latency for a prefetch request of a specific `type`.
  // Latency is only logged if the previous status was `kPending` and the
  // request start time is valid.
  void LogPrefetchTotalLatency(EntityType type);

  // Computes the non-eligibility reason specific to personal context in
  // Autofill AI (e.g. G1 subscription status or Android premium device status)
  // and logs it to UMA if the reason has changed and the startup delay has
  // elapsed.
  void ComputeAndMaybeLogNonEligibilityReason();

  // Indicates whether `kNonEligibilityLoggingDelayOnStartup` has elapsed,
  // preventing premature UMA logging during browser startup.
  bool is_non_eligibility_startup_delay_elapsed_ = false;

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<personal_context::PersonalContextEligibilityService>
      personal_context_eligibility_service_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<syncer::DeviceInfoSyncService> device_info_sync_service_;

  // Map from EntityId to the original proto Entity received during prefetch.
  absl::flat_hash_map<EntityInstance::EntityId, personal_context::proto::Entity>
      prefetched_proto_cache_;

  // Cache of unmasked sensitive PII (SPII) entity instances.
  //
  // **Eviction Mechanism**: Managed **per individual entity** (not per type).
  // When an entity is individually unmasked, it is added here, and a separate
  // task is scheduled to evict just this entity after
  // `kAutofillAmbientAutofillUnmaskedSpiiCacheTTL`.
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
  // this signal after
  // `kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL`.
  base::flat_set<SpiiEntityPresenceSignal> spii_presence_signal_cache_;

  // The last reported non-eligibility reason for personal context in Autofill
  // AI.
  std::optional<personal_context::PersonalContextNonEligibilityReason>
      last_non_eligibility_reason_;

  base::ObserverList<AutofillAiPersonalContextAccessManager::Observer>
      observers_;

  // Converts a proto Entity into an EntityInstance (decrypting if encrypted).
  // If `mask_spii` is true, decrypted entities have their SPII fields masked,
  // retaining only a suffix.
  std::optional<EntityInstance> ConvertProtoToEntityInstance(
      const personal_context::proto::Entity& entity,
      bool mask_spii) const;

  base::ScopedObservation<
      personal_context::PersonalContextEligibilityService,
      personal_context::PersonalContextEligibilityService::Observer>
      eligibility_service_observation_{this};

  base::ScopedObservation<
      subscription_eligibility::SubscriptionEligibilityService,
      subscription_eligibility::SubscriptionEligibilityService::Observer>
      subscription_eligibility_observation_{this};

  base::ScopedObservation<EntitySuppressionManager,
                          EntitySuppressionManager::Observer>
      suppression_observation_{this};

  PrefChangeRegistrar pref_registrar_;

  base::WeakPtrFactory<AutofillAiPersonalContextAccessManagerImpl>
      weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
