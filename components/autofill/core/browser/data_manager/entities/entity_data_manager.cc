// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"

#include "components/webdata/common/web_data_results.h"

namespace autofill {

EntityDataManager::EntityDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {}

EntityDataManager::~EntityDataManager() = default;

void EntityDataManager::AddEntityInstance(const EntityInstance& entity) {
  webdata_service_->AddEntityInstance(entity);
}

void EntityDataManager::UpdateEntityInstance(const EntityInstance& entity) {
  webdata_service_->UpdateEntityInstance(entity);
}

void EntityDataManager::RemoveEntityInstance(const base::Uuid& guid) {
  webdata_service_->RemoveEntityInstance(guid);
}

void EntityDataManager::LoadEntityInstances(LoadCallback cb) {
  webdata_service_->GetEntityInstances(base::BindOnce(
      [](LoadCallback cb, WebDataServiceBase::Handle handle,
         std::unique_ptr<WDTypedResult> result) {
        std::vector<EntityInstance> entities;
        if (result) {
          CHECK_EQ(result->GetType(), AUTOFILL_ENTITY_INSTANCE_RESULT);
          entities =
              static_cast<WDResult<std::vector<EntityInstance>>*>(result.get())
                  ->GetValue();
        }
        std::move(cb).Run(std::move(entities));
      },
      std::move(cb)));
}

}  // namespace autofill
