// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

EntityDataManager::EntityDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service,
    history::HistoryService* history_service,
    StrikeDatabaseBase* strike_database)
    : webdata_service_(std::move(webdata_service)) {
  CHECK(webdata_service_);
  LoadEntities();
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  if (strike_database) {
    save_strike_db_by_host_ =
        std::make_unique<AutofillAiSaveStrikeDatabaseByHost>(strike_database);
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
          if (!self->entities_.empty()) {
            self->NotifyEntityInstancesChanged();
          }
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void EntityDataManager::AddOrUpdateEntityInstance(EntityInstance entity) {
  webdata_service_->AddOrUpdateEntityInstance(
      std::move(entity),
      base::BindOnce(
          [](base::WeakPtr<EntityDataManager> self, EntityInstanceChange eic) {
            if (!self) {
              return;
            }
            CHECK_EQ(eic.type(), EntityInstanceChange::UPDATE);
            auto [it, inserted] = self->entities_.insert(*eic.data_model());
            if (!inserted) {
              *it = *eic.data_model();
            }
            self->NotifyEntityInstancesChanged();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void EntityDataManager::RemoveEntityInstance(base::Uuid guid) {
  webdata_service_->RemoveEntityInstance(
      std::move(guid),
      base::BindOnce(
          [](base::WeakPtr<EntityDataManager> self, EntityInstanceChange eic) {
            if (!self) {
              return;
            }
            CHECK_EQ(eic.type(), EntityInstanceChange::REMOVE);
            self->entities_.erase(eic.key());
            self->NotifyEntityInstancesChanged();
          },
          weak_ptr_factory_.GetWeakPtr()));
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
    const base::Uuid& guid) const {
  auto it = entities_.find(guid);
  if (it == entities_.end()) {
    return std::nullopt;
  }
  return *it;
}

base::optional_ref<EntityInstance> EntityDataManager::GetMutableEntityInstance(
    const base::Uuid& guid) {
  auto it = entities_.find(guid);
  if (it == entities_.end()) {
    return std::nullopt;
  }
  return *it;
}

void EntityDataManager::OnHistoryDeletions(
    history::HistoryService*,
    const history::DeletionInfo& deletion_info) {
  if (save_strike_db_by_host_) {
    save_strike_db_by_host_->ClearStrikesWithHistory(deletion_info);
  }
}

void EntityDataManager::RecordEntityUsed(const base::Uuid& guid,
                                         base::Time use_date) {
  base::optional_ref<EntityInstance> entity = GetMutableEntityInstance(guid);
  if (!entity) {
    return;
  }
  entity->RecordEntityUsed(use_date);
  AddOrUpdateEntityInstance(*entity);
}

void EntityDataManager::NotifyEntityInstancesChanged() {
  for (Observer& observer : observers_) {
    observer.OnEntityInstancesChanged();
  }
}

}  // namespace autofill
