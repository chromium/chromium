// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

// Determines whether cleanups should be deferred because the latest data wasn't
// synced down yet.
bool ShouldWaitForSync(const syncer::SyncService* sync_service) {
  // No need to wait if the user is not syncing payments data.
  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPayments)) {
    return false;
  }

  switch (
      sync_service->GetDownloadStatusFor(syncer::DataType::AUTOFILL_VALUABLE)) {
    case syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates:
      return true;
    case syncer::SyncService::DataTypeDownloadStatus::kUpToDate:
    // If the download status is kError, it will likely not become available
    // anytime soon. In this case, don't defer the cleanups.
    case syncer::SyncService::DataTypeDownloadStatus::kError:
      return false;
  }
  NOTREACHED();
}

}  // namespace

EntityInstanceCleaner::EntityInstanceCleaner(
    EntityDataManager* entity_data_manager,
    syncer::SyncService* sync_service,
    PrefService* pref_service)
    : entity_data_manager_(CHECK_DEREF(entity_data_manager)),
      sync_service_(sync_service),
      pref_service_(CHECK_DEREF(pref_service)) {
  if (sync_service_) {
    sync_observer_.Observe(sync_service_);
  }
}

EntityInstanceCleaner::~EntityInstanceCleaner() = default;

void EntityInstanceCleaner::MaybeCleanupLocalEntityInstancesData() {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiDedupeEntities)) {
    return;
  }
  if (!are_cleanups_pending_ || ShouldWaitForSync(sync_service_)) {
    return;
  }
  are_cleanups_pending_ = false;

  int chrome_version_major = version_info::GetMajorVersionNumberAsInt();
  // Ensure that deduplication is only run one per milestone.
  if (pref_service_->GetInteger(prefs::kAutofillAiLastVersionDeduped) <
      chrome_version_major) {
    pref_service_->SetInteger(prefs::kAutofillAiLastVersionDeduped,
                              chrome_version_major);
    base::span<const EntityInstance> entities =
        entity_data_manager_->GetEntityInstances();
    base::flat_set<EntityInstance::EntityId> to_be_removed;

    base::flat_map<EntityType, size_t> n_local_entities_per_type;
    base::flat_map<EntityType, size_t> n_local_entities_removed_per_type;
    // Adds `entity_a` to `to_be_removed` if `entity_a` is a subset of
    // `entity_b` and is a local entity.
    // Returns true if `entity_a` was newly added to `to_be_removed`, false
    // otherwise.
    auto maybe_add_duplicate_entity_to_to_be_removed =
        [&](const EntityInstance& entity_a, const EntityInstance entity_b) {
          if (to_be_removed.contains(entity_a.guid())) {
            return false;
          }
          return entity_a.record_type() == EntityInstance::RecordType::kLocal &&
                 entity_a.IsSubsetOf(entity_b) &&
                 to_be_removed.insert(entity_a.guid()).second;
        };

    for (size_t i = 0; i < entities.size(); i++) {
      const EntityInstance& entity_a = entities[i];
      if (entity_a.record_type() == EntityInstance::RecordType::kLocal) {
        ++n_local_entities_per_type[entity_a.type()];
      }
      for (size_t j = i + 1; j < entities.size(); j++) {
        const EntityInstance& entity_b = entities[j];
        if (maybe_add_duplicate_entity_to_to_be_removed(entity_a, entity_b)) {
          ++n_local_entities_removed_per_type[entity_a.type()];
        } else if (maybe_add_duplicate_entity_to_to_be_removed(entity_b,
                                                               entity_a)) {
          ++n_local_entities_removed_per_type[entity_b.type()];
        }
      }
    }

    for (const auto& guid : to_be_removed) {
      entity_data_manager_->RemoveEntityInstance(guid);
    }

    LogLocalEntitiesDeduplicationMetrics(n_local_entities_per_type,
                                         n_local_entities_removed_per_type);
  }
}

void EntityInstanceCleaner::OnStateChanged(syncer::SyncService* sync_service) {
  MaybeCleanupLocalEntityInstancesData();
}

void EntityInstanceCleaner::OnSyncShutdown(syncer::SyncService* sync_service) {
  sync_observer_.Reset();
}

}  // namespace autofill
