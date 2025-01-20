// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"

#include "components/webdata/common/web_data_results.h"

namespace autofill {

EntityDataManager::EntityDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {}

EntityDataManager::~EntityDataManager() {
  for (auto& [handle, callback] : std::exchange(pending_queries_, {})) {
    webdata_service_->CancelRequest(handle);
    std::move(callback).Run({});
  }
}

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
  RegisterPendingQuery(webdata_service_->GetEntityInstances(this),
                       std::move(cb));
}

void EntityDataManager::RegisterPendingQuery(WebDataServiceBase::Handle handle,
                                             LoadCallback cb) {
  LoadCallback& slot = pending_queries_[handle];
  CHECK(!slot);
  slot = std::move(cb);
}

void EntityDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  std::vector<EntityInstance> entities;
  if (result) {
    CHECK_EQ(result->GetType(), AUTOFILL_ENTITY_INSTANCE_RESULT);
    entities = static_cast<WDResult<std::vector<EntityInstance>>*>(result.get())
                   ->GetValue();
  }
  auto nh = pending_queries_.extract(handle);
  CHECK(!nh.empty());
  std::move(nh.mapped()).Run(std::move(entities));
}

}  // namespace autofill
