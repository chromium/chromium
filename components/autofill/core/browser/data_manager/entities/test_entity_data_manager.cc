// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/entities/test_entity_data_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

TestEntityDataManager::TestEntityDataManager() : EntityDataManager(nullptr) {}

TestEntityDataManager::~TestEntityDataManager() = default;

void TestEntityDataManager::AddEntityInstance(const EntityInstance& entity) {
  CHECK(!entities_.contains(entity.guid()));
  entities_.insert(entity);
}

void TestEntityDataManager::UpdateEntityInstance(const EntityInstance& entity) {
  CHECK(entities_.contains(entity.guid()));
  entities_.erase(entity.guid());
  entities_.insert(entity);
}

void TestEntityDataManager::RemoveEntityInstance(const base::Uuid& guid) {
  CHECK(entities_.contains(guid));
  entities_.erase(guid);
}

void TestEntityDataManager::LoadEntityInstances(LoadCallback cb) {
  static WebDataServiceBase::Handle g_next_handle = 0;
  WebDataServiceBase::Handle handle = g_next_handle++;
  RegisterPendingQuery(handle, std::move(cb));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestEntityDataManager::OnWebDataServiceRequestDone,
          weak_ptr_factory_.GetWeakPtr(), handle,
          std::make_unique<WDResult<std::vector<EntityInstance>>>(
              AUTOFILL_ENTITY_INSTANCE_RESULT, GetCopyOfEentities())));
}

}  // namespace autofill
