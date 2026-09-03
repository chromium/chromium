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
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/data_model_util.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_util.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
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
    // Jitter is disabled (0.0) to keep the retry delays deterministic.
    .jitter_factor = 0.0,
    .maximum_backoff_ms = base::Hours(1).InMilliseconds(),
    // -1 indicates infinite lifetime; entries persist for the lifecycle of the
    // manager.
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false};

bool IsPersonalContextEligible(
    personal_context::PersonalContextEligibilityState state) {
  using enum personal_context::PersonalContextEligibilityState;
  switch (state) {
    case kDisabledNotEligible:
      return false;
    case kEligible:
      return true;
  }
}

personal_context::proto::ContextMemoryAmbientAutofillRequest
CreateAmbientAutofillRequest(DenseSet<EntityType> types,
                             bool return_spii_presence,
                             std::string client_id) {
  personal_context::proto::ContextMemoryAmbientAutofillRequest request;
  for (EntityType type : types) {
    request.add_requested_types(
        AutofillEntityTypeToPersonalContextEntityType(type));
  }
  // Do not request presence if spii cache is enabled.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAmbientAutofillSpiiCache)) {
    request.set_return_spii_presence(return_spii_presence);
  }
  request.set_client_id(std::move(client_id));
  return request;
}

bool IsPersonalContextSpiiType(EntityType type) {
  return GetPersonalContextSpiiType(
             type, EntityInstance::RecordType::kPersonalContext) ==
         EntityInstance::PersonalContextSpiiType::kSpii;
}

bool IsValidDateWithinTtl(const EntityInstance& entity,
                          AttributeTypeName type_name,
                          base::TimeDelta ttl) {
  base::optional_ref<const AttributeInstance> attr =
      entity.attribute(AttributeType(type_name));
  if (!attr) {
    return false;
  }

  data_util::Date date;
  if (!data_util::ParseDate(attr->GetCompleteRawInfo(), u"YYYY-MM-DD", date) ||
      !data_util::IsValidDateForFormat(date, u"YYYY-MM-DD")) {
    return false;
  }

  // Set the time to the end of the day so the date remains valid throughout the
  // whole day.
  base::Time::Exploded exploded = {
      .year = date.year,
      .month = date.month,
      .day_of_month = date.day,
      .hour = 23,
      .minute = 59,
      .second = 59,
      .millisecond = 999,
  };
  base::Time time;
  return base::Time::FromLocalExploded(exploded, &time) &&
         time + ttl >= base::Time::Now();
}

bool ValidateTtl(const EntityInstance& entity) {
  switch (entity.type().name()) {
    case EntityTypeName::kPassport:
      return IsValidDateWithinTtl(
          entity, AttributeTypeName::kPassportExpirationDate, base::Days(0));
    case EntityTypeName::kDriversLicense:
      return IsValidDateWithinTtl(
          entity, AttributeTypeName::kDriversLicenseExpirationDate,
          base::Days(0));
    case EntityTypeName::kNationalIdCard:
      return IsValidDateWithinTtl(
          entity, AttributeTypeName::kNationalIdCardExpirationDate,
          base::Days(0));
    case EntityTypeName::kOrder:
      return IsValidDateWithinTtl(entity, AttributeTypeName::kOrderDate,
                                  base::Days(90));
    case EntityTypeName::kShipment:
      return IsValidDateWithinTtl(
          entity, AttributeTypeName::kShipmentShippedDate, base::Days(30));
    case EntityTypeName::kFlightReservation:
      return IsValidDateWithinTtl(
          entity, AttributeTypeName::kFlightReservationDepartureDate,
          base::Days(90));
    case EntityTypeName::kVehicle:
      return true;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      // Unsupported by Ambient Autofill.
      return false;
  }
  NOTREACHED();
}

bool IsValidAmbientAutofillEntity(const EntityInstance& entity) {
  return AttributesMeetImportConstraints(
             entity.type(),
             DenseSet(entity.attributes(), &AttributeInstance::type)) &&
         ValidateTtl(entity);
}

// Logs the request latency of a personal context network request.
void LogRequestLatency(
    AutofillAiPersonalContextAccessManagerImpl::RequestType request_type,
    base::TimeDelta latency) {
  switch (request_type) {
    using enum AutofillAiPersonalContextAccessManagerImpl::RequestType;
    case kNonSpiiAndPresence:
      base::UmaHistogramMediumTimes(
          "Autofill.Ai.PersonalContext.RequestLatency."
          "PrefetchNonSpiiAndPresence",
          latency);
      break;
    case kSpiiMasked:
      base::UmaHistogramMediumTimes(
          "Autofill.Ai.PersonalContext.RequestLatency.PrefetchSpiiMasked",
          latency);
      break;
    case kSpiiUnmasking:
      base::UmaHistogramMediumTimes(
          "Autofill.Ai.PersonalContext.RequestLatency.SpiiUnmasking", latency);
      break;
  }
}

std::string GetLocalDeviceGuid(
    syncer::DeviceInfoSyncService* device_info_sync_service) {
  if (!device_info_sync_service) {
    return std::string();
  }
  const syncer::LocalDeviceInfoProvider* provider =
      device_info_sync_service->GetLocalDeviceInfoProvider();
  if (!provider) {
    return std::string();
  }
  const syncer::DeviceInfo* device_info = provider->GetLocalDeviceInfo();
  if (!device_info) {
    return std::string();
  }
  return device_info->guid();
}

}  // namespace

AutofillAiPersonalContextAccessManagerImpl::
    AutofillAiPersonalContextAccessManagerImpl(
        personal_context::PersonalContextService* personal_context_service,
        personal_context::PersonalContextEligibilityService*
            personal_context_eligibility_service,
        subscription_eligibility::SubscriptionEligibilityService*
            subscription_eligibility_service,
        PrefService* pref_service,
        syncer::DeviceInfoSyncService* device_info_sync_service,
        EntitySuppressionManager* suppression_manager)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      personal_context_eligibility_service_(
          CHECK_DEREF(personal_context_eligibility_service)),
      pref_service_(pref_service),
      device_info_sync_service_(device_info_sync_service) {
  eligibility_service_observation_.Observe(
      personal_context_eligibility_service);
  if (subscription_eligibility_service) {
    subscription_eligibility_observation_.Observe(
        subscription_eligibility_service);
  }
  if (suppression_manager) {
    suppression_observation_.Observe(suppression_manager);
  }
  if (pref_service_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::BindRepeating(&AutofillAiPersonalContextAccessManagerImpl::
                                OnPersonalContextSettingsToggleChanged,
                            base::Unretained(this)));
  }

  // Called after the startup delay (`kNonEligibilityLoggingDelayOnStartup`)
  // has elapsed to enable non-eligibility UMA logging and record the initial
  // reason.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AutofillAiPersonalContextAccessManagerImpl> self) {
            if (!self) {
              return;
            }
            self->is_non_eligibility_startup_delay_elapsed_ = true;
            self->ComputeAndMaybeLogNonEligibilityReason();
          },
          weak_factory_.GetWeakPtr()),
      kNonEligibilityLoggingDelayOnStartup);
}

AutofillAiPersonalContextAccessManagerImpl::
    ~AutofillAiPersonalContextAccessManagerImpl() = default;

void AutofillAiPersonalContextAccessManagerImpl::PrefetchContext(
    DenseSet<EntityType> requested_types) {
  // Types to request in Request 1 (which includes all non-SPII types and any
  // SPII types for which we want to check presence signals).
  DenseSet<EntityType> non_spii_and_presence_to_request;
  // SPII types for which we want to fetch the actual masked entity data in
  // Request 2.
  DenseSet<EntityType> spii_to_request;

  DenseSet<PersonalContextPrefetchTriggerResult> unique_trigger_results;
  for (EntityType type : requested_types) {
    PersonalContextPrefetchTriggerResult trigger_result =
        DeterminePrefetchTriggerResult(type);
    unique_trigger_results.insert(trigger_result);

    if (trigger_result == PersonalContextPrefetchTriggerResult::kInitiated) {
      non_spii_and_presence_to_request.insert(type);
      SetTypeStatus(type, RequestStatus::kPending);

      if (IsPersonalContextSpiiType(type)) {
        spii_to_request.insert(type);
      }
    }
  }

  LogPersonalContextPrefetchTriggerResults(unique_trigger_results);

  if (non_spii_and_presence_to_request.empty()) {
    NotifyPrefetchStatusObservers(base::span<const EntityInstance>());
    return;
  }

  const bool has_spii_types = !spii_to_request.empty();
  const std::string client_id = GetLocalDeviceGuid(device_info_sync_service_);

  // Request 1: collects non-spii entities and asks for spii presence if any of
  // the requested_types contains SPII types.
  // If `kAutofillAmbientAutofillSpiiCache` is enabled, presence isn't requested
  // anymore and spii is part of this request instead.
  {
    personal_context::proto::ContextMemoryAmbientAutofillRequest request =
        CreateAmbientAutofillRequest(non_spii_and_presence_to_request,
                                     /*return_spii_presence=*/has_spii_types,
                                     client_id);
    personal_context_service_->FetchContext(
        personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
        request,
        /*options=*/{},
        base::BindOnce(
            &AutofillAiPersonalContextAccessManagerImpl::
                OnPrefetchContextRequestComplete,
            weak_factory_.GetWeakPtr(), non_spii_and_presence_to_request,
            RequestType::kNonSpiiAndPresence, base::TimeTicks::Now()));
  }

  // Request 2: collects spii entities without asking for spii presence.
  // If `kAutofillAmbientAutofillSpiiCache` is enabled, spii is already fetched
  // in the first request.
  if (has_spii_types && !base::FeatureList::IsEnabled(
                            features::kAutofillAmbientAutofillSpiiCache)) {
    personal_context::proto::ContextMemoryAmbientAutofillRequest request =
        CreateAmbientAutofillRequest(spii_to_request,
                                     /*return_spii_presence=*/false, client_id);
    personal_context_service_->FetchContext(
        personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
        request,
        /*options=*/{},
        base::BindOnce(&AutofillAiPersonalContextAccessManagerImpl::
                           OnPrefetchContextRequestComplete,
                       weak_factory_.GetWeakPtr(), spii_to_request,
                       RequestType::kSpiiMasked, base::TimeTicks::Now()));
  }
}

void AutofillAiPersonalContextAccessManagerImpl::
    OnPrefetchContextRequestComplete(
        DenseSet<EntityType> requested_types,
        RequestType request_type,
        base::TimeTicks request_start_time,
        personal_context::FetchContextResult result) {
  LogRequestLatency(request_type, base::TimeTicks::Now() - request_start_time);

  if (!result.response.has_value()) {
    HandleFailedResponse(requested_types, request_type);
    return;
  }

  base::expected<std::vector<ParsedEntity>,
                 personal_context::ContextMemoryError>
      parsed_entities = ExtractEntitiesFromResponse(result.response->value());

  if (!parsed_entities.has_value()) {
    HandleFailedResponse(requested_types, request_type);
    return;
  }

  DenseSet<EntityType> prefetched_types;
  for (EntityType type : requested_types) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillAmbientAutofillSpiiCache) ||
        request_type == RequestType::kSpiiMasked ||
        !IsPersonalContextSpiiType(type)) {
      prefetched_types.insert(type);
    }
  }

  ProcessPrefetchedEntities(prefetched_types, requested_types,
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
    } else if (std::optional<EntityInstance> converted =
                   ConvertProtoToEntityInstance(entity, /*mask_spii=*/true)) {
      // TODO(crbug.com/501037715): Record metrics for
      // `IsValidAmbientAutofillEntity`.
      if (IsValidAmbientAutofillEntity(*converted)) {
        entities.push_back({std::move(*converted), entity});
      }
    }
  }
  return entities;
}

std::optional<EntityInstance>
AutofillAiPersonalContextAccessManagerImpl::ConvertProtoToEntityInstance(
    const personal_context::proto::Entity& entity,
    bool mask_spii) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAmbientAutofillSpiiCache) &&
      entity.entity_case() ==
          personal_context::proto::Entity::kEncryptedEntity) {
    return personal_context_service_->DecryptEntity(entity).and_then(
        [mask_spii](personal_context::proto::Entity decrypted) {
          if (mask_spii) {
            MaskSpiiEntityFields(decrypted);
          }
          return PersonalContextEntityToEntityInstance(decrypted,
                                                       /*is_masked=*/mask_spii);
        });
  }

  return PersonalContextEntityToEntityInstance(entity,
                                               /*is_masked=*/mask_spii);
}

void AutofillAiPersonalContextAccessManagerImpl::GetUnmaskedSpiiEntity(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback) {
  if (auto it = unmasked_spii_cache_.find(id);
      it != unmasked_spii_cache_.end()) {
    LogUnmaskResult(EntityInstance::RecordType::kPersonalContext,
                    AutofillAiUnmaskResult::kCacheHit);
    std::move(callback).Run(*it);
    return;
  }

  personal_context::proto::Entity* proto_entity =
      base::FindOrNull(prefetched_proto_cache_, id);
  if (!proto_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  const base::TimeTicks request_start_time = base::TimeTicks::Now();

  if (base::FeatureList::IsEnabled(
          features::kAutofillAmbientAutofillSpiiCache)) {
    if (std::optional<EntityInstance> unmasked_entity =
            ConvertProtoToEntityInstance(*proto_entity,
                                         /*mask_spii=*/false)) {
      EntityInstance final_entity = unmasked_entity->CopyWithNewEntityId(id);
      CacheUnmaskedSpiiEntity(final_entity);
      LogUnmaskResult(EntityInstance::RecordType::kPersonalContext,
                      AutofillAiUnmaskResult::kSuccess);
      LogRequestLatency(RequestType::kSpiiUnmasking,
                        base::TimeTicks::Now() - request_start_time);
      std::move(callback).Run(std::move(final_entity));
      return;
    }
    LogUnmaskResult(EntityInstance::RecordType::kPersonalContext,
                    AutofillAiUnmaskResult::kDecryptionFailed);
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
                     weak_factory_.GetWeakPtr(), id, std::move(callback),
                     request_start_time));
}

void AutofillAiPersonalContextAccessManagerImpl::OnFetchPiiEntitiesComplete(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback,
    base::TimeTicks request_start_time,
    personal_context::FetchPiiEntitiesResult result) {
  LogRequestLatency(RequestType::kSpiiUnmasking,
                    base::TimeTicks::Now() - request_start_time);
  using enum AutofillAiUnmaskResult;
  if (!result.response.has_value()) {
    using ExecutionError = personal_context::ContextMemoryError::ExecutionError;
    const AutofillAiUnmaskResult outcome =
        result.response.error().error() == ExecutionError::kResponseParseError
            ? kParsingError
            : kNetworkError;
    LogUnmaskResult(EntityInstance::RecordType::kPersonalContext, outcome);
    std::move(callback).Run(std::nullopt);
    return;
  }

  const personal_context::proto::FetchPiiEntitiesResponse& response =
      result.response.value();
  if (response.entities().empty()) {
    LogUnmaskResult(EntityInstance::RecordType::kPersonalContext,
                    kEmptyResponse);
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<EntityInstance> unmasked_entity =
      PersonalContextEntityToEntityInstance(response.entities(0),
                                            /*is_masked=*/false);
  if (!unmasked_entity) {
    LogUnmaskResult(EntityInstance::RecordType::kPersonalContext,
                    kParsingError);
    std::move(callback).Run(std::nullopt);
    return;
  }

  EntityInstance final_entity = unmasked_entity->CopyWithNewEntityId(id);
  CacheUnmaskedSpiiEntity(final_entity);
  LogUnmaskResult(EntityInstance::RecordType::kPersonalContext, kSuccess);
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

bool AutofillAiPersonalContextAccessManagerImpl::ServerHasSpiiPresenceSignal(
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
    DenseSet<EntityType> prefetched_types,
    DenseSet<EntityType> requested_types,
    std::vector<ParsedEntity> parsed_entities) {
  // Evict existing entities for the `prefetched_types`.
  for (EntityType type : prefetched_types) {
    LogPrefetchTotalLatency(type);
    ResetStateForType(type);
    SetTypeStatus(type, RequestStatus::kSuccess);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AutofillAiPersonalContextAccessManagerImpl::ResetStateForType,
            weak_factory_.GetWeakPtr(), type),
        features::kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL
            .Get());
  }

  // Populates the proto cache and notify observers about the fetched entities.
  // Also cache presence signals.
  std::vector<EntityInstance> entities;
  entities.reserve(parsed_entities.size());
  const EntitySuppressionManager* suppression_manager =
      suppression_observation_.GetSource();
  for (ParsedEntity& entity : parsed_entities) {
    if (const auto* signal =
            std::get_if<SpiiEntityPresenceSignal>(&entity.instance)) {
      if (requested_types.contains(*signal)) {
        CachePresenceSignal(*signal);
      }
      continue;
    }

    EntityInstance& instance = std::get<EntityInstance>(entity.instance);
    if (!requested_types.contains(instance.type())) {
      continue;
    }

    prefetched_proto_cache_.emplace(instance.guid(), std::move(entity.proto));
    if (!suppression_manager || !suppression_manager->IsSuppressed(instance)) {
      entities.push_back(std::move(instance));
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
      features::kAutofillAmbientAutofillUnmaskedSpiiCacheTTL.Get());
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
      features::kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL
          .Get());
}

void AutofillAiPersonalContextAccessManagerImpl::WipeCache() {
  // Invalidate weak pointers to cancel any pending fetches.
  weak_factory_.InvalidateWeakPtrs();
  // Copy the keys since `ResetStateForType()` invalidates iterators to
  // `prefetch_state_`.
  std::vector<EntityType> prefetched_types = base::ToVector(
      prefetch_state_, [](const auto& item) { return item.first; });
  for (const EntityType& type : prefetched_types) {
    ResetStateForType(type);
  }
  spii_presence_signal_cache_.clear();
}

void AutofillAiPersonalContextAccessManagerImpl::OnEligibilityStateChanged(
    personal_context::PersonalContextEligibilityState new_state) {
  ComputeAndMaybeLogNonEligibilityReason();
  if (!IsPersonalContextEligible(new_state)) {
    WipeCache();
  }
}

void AutofillAiPersonalContextAccessManagerImpl::OnAiSubscriptionTierUpdated(
    int32_t /*new_subscription_tier*/) {
  ComputeAndMaybeLogNonEligibilityReason();
}

void AutofillAiPersonalContextAccessManagerImpl::
    OnPersonalContextSettingsToggleChanged() {
  ComputeAndMaybeLogNonEligibilityReason();
  if (pref_service_ &&
      !pref_service_->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    WipeCache();
  }
}

void AutofillAiPersonalContextAccessManagerImpl::
    ComputeAndMaybeLogNonEligibilityReason() {
  using personal_context::PersonalContextNonEligibilityReason;
  if (!pref_service_ || !is_non_eligibility_startup_delay_elapsed_) {
    return;
  }

  std::optional<PersonalContextNonEligibilityReason> non_eligibility_reason =
      personal_context_eligibility_service_->GetNonEligibilityReason();

  if (non_eligibility_reason ==
          PersonalContextNonEligibilityReason::kEligible &&
      !IsDeviceOrSubscriptionTierEligibleForAmbientAutofill(
          subscription_eligibility_observation_.GetSource())) {
    non_eligibility_reason = PersonalContextNonEligibilityReason::
        kNotG1SubscriberOrAndroidPremiumDevice;
  }

  if (non_eligibility_reason ==
          PersonalContextNonEligibilityReason::kEligible &&
      !pref_service_->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    non_eligibility_reason =
        PersonalContextNonEligibilityReason::kPersonalIntelligencePrefDisabled;
  }

  if (last_non_eligibility_reason_ == non_eligibility_reason) {
    return;
  }
  last_non_eligibility_reason_ = non_eligibility_reason;
  if (last_non_eligibility_reason_) {
    LogPersonalContextNonEligibilityReason(*last_non_eligibility_reason_);
  }
}

PersonalContextPrefetchTriggerResult
AutofillAiPersonalContextAccessManagerImpl::DeterminePrefetchTriggerResult(
    EntityType type) const {
  const RequestState* request_state = base::FindOrNull(prefetch_state_, type);
  if (!request_state) {
    return PersonalContextPrefetchTriggerResult::kInitiated;
  }

  using enum RequestStatus;
  switch (request_state->status) {
    case kPending:
      return PersonalContextPrefetchTriggerResult::kSkippedInFlight;
    case kSuccess:
      if (base::TimeTicks::Now() - request_state->last_update_time >
          features::kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL
              .Get()) {
        return PersonalContextPrefetchTriggerResult::kInitiated;
      }
      return PersonalContextPrefetchTriggerResult::kSkippedFreshCache;
    case kFailure:
      return ShouldRetryAfterFailure(*request_state)
                 ? PersonalContextPrefetchTriggerResult::kInitiated
                 : PersonalContextPrefetchTriggerResult::kSkippedBackoff;
    case kNotStarted:
      return PersonalContextPrefetchTriggerResult::kInitiated;
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

  using enum RequestStatus;
  switch (status) {
    case kPending:
    case kNotStarted:
      break;
    case kSuccess:
      state.backoff_entry->Reset();
      break;
    case kFailure:
      state.backoff_entry->InformOfRequest(/*succeeded=*/false);
      break;
  }
}

void AutofillAiPersonalContextAccessManagerImpl::HandleFailedResponse(
    DenseSet<EntityType> requested_types,
    RequestType request_type) {
  for (EntityType type : requested_types) {
    if (request_type == RequestType::kNonSpiiAndPresence &&
        IsPersonalContextSpiiType(type)) {
      continue;
    }
    SetTypeStatus(type, RequestStatus::kFailure);
  }
  NotifyPrefetchStatusObservers({});
}

void AutofillAiPersonalContextAccessManagerImpl::LogPrefetchTotalLatency(
    EntityType type) {
  if (const RequestState* state = base::FindOrNull(prefetch_state_, type)) {
    if (state->status == RequestStatus::kPending &&
        !state->last_update_time.is_null()) {
      LogPersonalContextPrefetchTotalLatency(
          type, base::TimeTicks::Now() - state->last_update_time);
    }
  }
}

void AutofillAiPersonalContextAccessManagerImpl::NotifyPrefetchStatusObservers(
    std::optional<base::span<const EntityInstance>> entities) {
  observers_.Notify(&AutofillAiPersonalContextAccessManager::Observer::
                        OnPrefetchContextComplete,
                    *this, entities);
}

void AutofillAiPersonalContextAccessManagerImpl::OnEntitySuppressionsChanged() {
  if (prefetched_proto_cache_.empty()) {
    return;
  }

  DenseSet<EntityType> cached_types;
  std::vector<EntityInstance> unsuppressed_entities;
  const EntitySuppressionManager* suppression_manager =
      suppression_observation_.GetSource();
  for (const auto& [id, proto] : prefetched_proto_cache_) {
    std::optional<EntityInstance> converted =
        ConvertProtoToEntityInstance(proto, /*mask_spii=*/true);
    if (!converted) {
      continue;
    }
    EntityInstance entity = converted->CopyWithNewEntityId(id);
    cached_types.insert(entity.type());
    if (!suppression_manager || !suppression_manager->IsSuppressed(entity)) {
      unsuppressed_entities.push_back(std::move(entity));
    }
  }

  for (EntityType type : cached_types) {
    observers_.Notify(&AutofillAiPersonalContextAccessManager::Observer::
                          OnMaskedEntityTypeEvicted,
                      *this, type);
  }

  NotifyPrefetchStatusObservers(unsuppressed_entities);
}

}  // namespace autofill
