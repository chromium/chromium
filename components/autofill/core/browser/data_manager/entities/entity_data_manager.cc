// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"

#include "base/containers/contains.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

EntityDataManager::EntityDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {
  CHECK(webdata_service_);
  LoadEntities();
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
         std::unique_ptr<WDTypedResult> result) {
        CHECK_EQ(handle, self->pending_query_);
        self->pending_query_ = {};
        if (result) {
          CHECK_EQ(result->GetType(), AUTOFILL_ENTITY_INSTANCE_RESULT);
          self->entities_ =
              static_cast<WDResult<std::vector<EntityInstance>>*>(result.get())
                  ->GetValue();
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
            auto it = std::ranges::find(self->entities_, eic.key(),
                                        &EntityInstance::guid);
            if (it != self->entities_.end()) {
              *it = *eic.data_model();
            } else {
              self->entities_.push_back(*eic.data_model());
            }
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
            auto it = std::ranges::find(self->entities_, eic.key(),
                                        &EntityInstance::guid);
            if (it != self->entities_.end()) {
              self->entities_.erase(it);
            }
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

const std::vector<EntityInstance>& EntityDataManager::GetEntityInstances()
    const {
  return entities_;
}

}  // namespace autofill
