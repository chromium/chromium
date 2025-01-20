// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_TEST_ENTITY_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_TEST_ENTITY_DATA_MANAGER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"

namespace autofill {

class TestEntityDataManager : public EntityDataManager {
 public:
  explicit TestEntityDataManager();
  TestEntityDataManager(const TestEntityDataManager&) = delete;
  TestEntityDataManager& operator=(const TestEntityDataManager&) = delete;
  ~TestEntityDataManager() override;

  void set_entities(std::vector<EntityInstance> entities) {
    entities_ = std::move(entities);
  }

  void AddEntityInstance(const EntityInstance& entity) override;
  void UpdateEntityInstance(const EntityInstance& entity) override;
  void RemoveEntityInstance(const base::Uuid& guid) override;
  void LoadEntityInstances(LoadCallback cb) override;

 private:
  struct CompareByGuid {
    using is_transparent = void;

    bool operator()(const EntityInstance& lhs, const base::Uuid& rhs) const {
      return lhs.guid() < rhs;
    }

    bool operator()(const base::Uuid& lhs, const EntityInstance& rhs) const {
      return lhs < rhs.guid();
    }

    bool operator()(const EntityInstance& lhs,
                    const EntityInstance& rhs) const {
      return lhs.guid() < rhs.guid();
    }
  };

  std::vector<EntityInstance> GetCopyOfEentities() const {
    return std::vector<EntityInstance>(entities_.begin(), entities_.end());
  }

  base::flat_set<EntityInstance, CompareByGuid> entities_;
  base::WeakPtrFactory<TestEntityDataManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_TEST_ENTITY_DATA_MANAGER_H_
