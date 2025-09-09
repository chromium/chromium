// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

// Determines whether cleanups should be deferred because the latest data wasn't
// synced down yet.
bool ShouldWaitForSync(syncer::SyncService* sync_service) {
  // No need to wait if the user is not syncing payments data.
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
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

    for (size_t i = 0; i < entities.size(); i++) {
      const EntityInstance& entity_a = entities[i];
      if (to_be_removed.contains(entity_a.guid())) {
        continue;
      }
      for (size_t j = i + 1; j < entities.size(); j++) {
        const EntityInstance& entity_b = entities[j];
        if (entity_a.IsSubsetOf(entity_b) &&
            entity_a.record_type() == EntityInstance::RecordType::kLocal) {
          to_be_removed.insert(entity_a.guid());
        } else if (entity_b.IsSubsetOf(entity_a) &&
                   entity_b.record_type() ==
                       EntityInstance::RecordType::kLocal) {
          to_be_removed.insert(entity_b.guid());
        }
      }
    }

    for (const auto& guid : to_be_removed) {
      entity_data_manager_->RemoveEntityInstance(guid);
    }
  }
}

void EntityInstanceCleaner::OnStateChanged(syncer::SyncService* sync_service) {
  MaybeCleanupLocalEntityInstancesData();
}

}  // namespace autofill
