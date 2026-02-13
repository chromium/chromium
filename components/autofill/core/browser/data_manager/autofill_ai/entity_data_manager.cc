// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

EntityDataManager::EntityDataManager(
    PrefService* pref_service,
    const signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    scoped_refptr<AutofillWebDataService> webdata_service,
    history::HistoryService* history_service,
    strike_database::StrikeDatabaseBase* strike_database,
    AccessibilityAnnotatorDataAdapter* accessibility_annotator_data_adapter,
    GeoIpCountryCode variation_country_code)
    : webdata_service_(std::move(webdata_service)),
      entity_instance_cleaner_(this, sync_service, pref_service),
      variation_country_code_(std::move(variation_country_code)) {
  CHECK(webdata_service_);
  webdata_service_observation_.Observe(webdata_service_.get());
  LoadEntities();
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  if (strike_database) {
    save_strike_db_by_host_ =
        std::make_unique<AutofillAiSaveStrikeDatabaseByHost>(strike_database);
  }

  const bool user_is_opted_in =
      GetAutofillAiOptInStatus(pref_service, identity_manager);
  // Initial Autofill AI users have their opt-in pref stored keyed by their
  // gaia-id and not syncable. On the other hand, the new Autofill AI opt-in
  // pref (`prefs::kAutofillAiSyncedOptInStatus`) is a regular syncable pref.
  // The following code block migrates users who opted-in to the old pref to the
  // new syncable pref. For the time being, it does not remove the old pref to
  // allow rollbacks.
  if (base::FeatureList::IsEnabled(
          features::kAutofillAiSetSyncablePrefFromAccountPref)) {
    const PrefService::Preference* synced_pref =
        pref_service->FindPreference(prefs::kAutofillAiSyncedOptInStatus);
    CHECK(synced_pref);
    if (HasSetLocalAutofillAiOptInStatus(pref_service, identity_manager)) {
      if (!synced_pref->HasUserSetting()) {
        pref_service->SetBoolean(prefs::kAutofillAiSyncedOptInStatus,
                                 user_is_opted_in);
        base::UmaHistogramEnumeration(
            "Autofill.Ai.OptIn.PrefMigration",
            user_is_opted_in
                ? AutofillAiPrefMigrationStatus::kPrefMigratedEnabled
                : AutofillAiPrefMigrationStatus::kPrefMigratedDisabled);
      } else {
        base::UmaHistogramEnumeration(
            "Autofill.Ai.OptIn.PrefMigration",
            AutofillAiPrefMigrationStatus::kPrefNotMigratedAlreadySet);
      }
    } else {
      base::UmaHistogramEnumeration(
          "Autofill.Ai.OptIn.PrefMigration",
          AutofillAiPrefMigrationStatus::kPrefNotMigratedAccountPrefNeverSet);
    }
  }

  // This assumes that `EntityDataManager` is created once on profile creation.
  base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Status.Startup",
                                user_is_opted_in
                                    ? AutofillAiOptInStatus::kOptedIn
                                    : AutofillAiOptInStatus::kOptedOut);

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseAccessibilityAnnotator) &&
      accessibility_annotator_data_adapter) {
    accessibility_annotator_data_adapter_observation_.Observe(
        accessibility_annotator_data_adapter);
  }
}

EntityDataManager::~EntityDataManager() {
  if (pending_query_) {
    webdata_service_->CancelRequest(pending_query_);
  }
}

void EntityDataManager::LoadEntities() {
  if (pending_query_) {
    webdata_service_->CancelRequest(pending_query_);
  }
  pending_query_ = webdata_service_->GetEntityInstances(base::BindOnce(
      [](base::WeakPtr<EntityDataManager> self,
         WebDataServiceBase::Handle handle,
         std::unique_ptr<WDTypedResult> typed_result) {
        CHECK_EQ(handle, self->pending_query_);
        self->pending_query_ = {};
        if (typed_result) {
          CHECK_EQ(typed_result->GetType(), AUTOFILL_ENTITY_INSTANCE_RESULT);
          auto& result = static_cast<WDResult<std::vector<EntityInstance>>&>(
              *typed_result);
          self->entities_ =
              base::flat_set<EntityInstance, EntityInstance::CompareByGuid>(
                  std::move(result).GetValue());
          if (!self->entity_data_loaded_) {
            self->entity_data_loaded_ = true;
            LogStoredEntitiesCount(self->entities_);
          }
          self->NotifyEntityInstancesChanged();
        }
      },
      GetWeakPtr()));
}

void EntityDataManager::AddOrUpdateEntityInstance(EntityInstance entity) {
  webdata_service_->AddOrUpdateEntityInstance(
      std::move(entity),
      base::BindOnce(
          [](base::WeakPtr<EntityDataManager> self, EntityInstanceChange eic) {
            if (!self) {
              return;
            }
            CHECK(eic.type() == EntityInstanceChange::ADD ||
                  eic.type() == EntityInstanceChange::UPDATE);
            auto [it, inserted] = self->entities_.insert(eic.data_model());
            if (!inserted) {
              *it = eic.data_model();
            }
            self->NotifyEntityInstancesChanged();
          },
          GetWeakPtr()));
}

void EntityDataManager::RemoveEntityInstance(EntityInstance::EntityId guid) {
  base::optional_ref<const EntityInstance> entity_instance =
      GetEntityInstance(guid);
  if (!entity_instance) {
    return;
  }
  webdata_service_->RemoveEntityInstance(
      *entity_instance,
      base::BindOnce(
          [](base::WeakPtr<EntityDataManager> self, EntityInstanceChange eic) {
            if (!self) {
              return;
            }
            CHECK_EQ(eic.type(), EntityInstanceChange::REMOVE);
            self->entities_.erase(eic.key());
            self->NotifyEntityInstancesChanged();
          },
          GetWeakPtr()));
}

void EntityDataManager::RemoveEntityInstancesModifiedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  webdata_service_->RemoveEntityInstancesModifiedBetween(delete_begin,
                                                         delete_end);
  // Update the cache.
  LoadEntities();
}

base::optional_ref<const EntityInstance> EntityDataManager::GetEntityInstance(
    const EntityInstance::EntityId& guid) const {
  auto it = entities_.find(guid);
  if (it == entities_.end()) {
    return std::nullopt;
  }
  return *it;
}

base::optional_ref<EntityInstance> EntityDataManager::GetMutableEntityInstance(
    const EntityInstance::EntityId& guid) {
  auto it = entities_.find(guid);
  if (it == entities_.end()) {
    return std::nullopt;
  }
  return *it;
}

bool EntityDataManager::HasPendingQueries() const {
  return pending_query_ != 0;
}

void EntityDataManager::OnAutofillChangedBySync(syncer::DataType data_type) {
  if (data_type == syncer::AUTOFILL_VALUABLE ||
      data_type == syncer::AUTOFILL_VALUABLE_METADATA) {
    LoadEntities();
  }
}

void EntityDataManager::OnHistoryDeletions(
    history::HistoryService*,
    const history::DeletionInfo& deletion_info) {
  if (save_strike_db_by_host_) {
    save_strike_db_by_host_->ClearStrikesWithHistory(deletion_info);
  }
}

void EntityDataManager::OnAccessibilityAnnotatorDataChanged(
    AccessibilityAnnotatorDataAdapter& adapter) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillUseAccessibilityAnnotator));
  // TODO(crbug.com/483403739): Implement.
}

void EntityDataManager::RecordEntityUsed(const EntityInstance::EntityId& guid,
                                         base::Time use_date) {
  base::optional_ref<EntityInstance> entity = GetMutableEntityInstance(guid);
  if (!entity) {
    return;
  }
  entity->RecordEntityUsed(use_date);
  webdata_service_->UpdateEntityMetadata(*entity);
}

void EntityDataManager::NotifyEntityInstancesChanged() {
  for (Observer& observer : observers_) {
    observer.OnEntityInstancesChanged();
  }
}

const GeoIpCountryCode& EntityDataManager::GetVariationCountryCode() const {
  return variation_country_code_;
}

}  // namespace autofill
