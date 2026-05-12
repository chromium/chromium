// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/dense_set.h"
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
    GeoIpCountryCode variation_country_code)
    : webdata_service_(std::move(webdata_service)),
      entity_instance_cleaner_(this, sync_service, pref_service),
      variation_country_code_(std::move(variation_country_code)) {
  CHECK(webdata_service_);
  webdata_service_observation_.Observe(webdata_service_.get());
  LoadEntitiesFromDatabase();
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  if (strike_database) {
    save_strike_db_by_host_ =
        std::make_unique<AutofillAiSaveStrikeDatabaseByHost>(strike_database);
  }

  // This assumes that `EntityDataManager` is created once on profile creation.
  base::UmaHistogramEnumeration(
      "Autofill.Ai.OptIn.Status.Startup",
      GetAutofillAiOptInStatus(pref_service, identity_manager)
          ? AutofillAiOptInStatus::kOptedIn
          : AutofillAiOptInStatus::kOptedOut);
}

EntityDataManager::~EntityDataManager() {
  if (pending_query_) {
    webdata_service_->CancelRequest(pending_query_);
  }
}

void EntityDataManager::Shutdown() {
  history_service_observation_.Reset();
}

void EntityDataManager::LoadEntitiesFromDatabase() {
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
          std::vector<EntityInstance> entities = std::move(result).GetValue();

          // `self->entities_` may contain entities stored by Autofill AI and
          // entities from Accessibility Annotator.
          //
          // LoadEntitiesFromDatabase() replaces all entities stored by Autofill
          // AI.
          //
          // So we first need to purge them from `self->entities_`.
          auto is_stored_by_autofill_ai = [](const EntityInstance& entity) {
            switch (entity.record_type()) {
              case EntityInstance::RecordType::kLocal:
              case EntityInstance::RecordType::kServerWallet:
                return true;
              case EntityInstance::RecordType::kAccessibilityAnnotator:
                return false;
            }
            NOTREACHED();
          };
          DCHECK(std::ranges::all_of(entities, is_stored_by_autofill_ai));
          base::EraseIf(self->entities_, is_stored_by_autofill_ai);
          self->entities_.insert(std::make_move_iterator(entities.begin()),
                                 std::make_move_iterator(entities.end()));
          self->EnforceEntityReauthRequirements();
          self->NotifyEntityInstancesChanged();

          if (!self->database_loaded_) {
            self->database_loaded_ = true;
            // TODO(crbug.com/495779639): `EnforceEntityReauthRequirements()`
            // might asynchronously remove some of the entities, causing
            // `LogStoredEntitiesCount()` to over count.
            LogStoredEntitiesCount(self->entities_);
            MaybeImportEntitiesForTesting(self->GetWeakPtr());
          }
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
            EntityInstance entity = eic.data_model();

            // Erase first because insert() does not replace existing entries.
            self->entities_.erase(entity.guid());
            self->entities_.insert(std::move(entity));
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
  LoadEntitiesFromDatabase();
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
    LoadEntitiesFromDatabase();
  }
}

void EntityDataManager::OnHistoryDeletions(
    history::HistoryService*,
    const history::DeletionInfo& deletion_info) {
  if (save_strike_db_by_host_) {
    save_strike_db_by_host_->ClearStrikesWithHistory(deletion_info);
  }
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

void EntityDataManager::SetReauthAvailability(bool reauth_available) {
  if (reauth_availability_ == reauth_available) {
    return;
  }
  reauth_availability_ = reauth_available;
  EnforceEntityReauthRequirements();
}

void EntityDataManager::EnforceEntityReauthRequirements() {
  // If the re-auth state is unknown, assume that re-auth is supported. This
  // prevents removing data during transient inavailability on start-up.
  if (!reauth_availability_ || reauth_availability_.value() ||
      base::FeatureList::IsEnabled(
          features::debug::kAutofillAiDisableReauthRequirement)) {
    return;
  }
  // The device doesn't support re-auth. Remove all Wallet private passes.
  std::vector<EntityInstance::EntityId> entities_to_remove;
  for (const EntityInstance& entity : GetEntityInstances()) {
    if (IsMaskedStorageSupported(entity.type(), entity.record_type())) {
      entities_to_remove.push_back(entity.guid());
    }
  }
  for (const EntityInstance::EntityId& id : entities_to_remove) {
    RemoveEntityInstance(id);
  }
}

const GeoIpCountryCode& EntityDataManager::GetVariationCountryCode() const {
  return variation_country_code_;
}

}  // namespace autofill
