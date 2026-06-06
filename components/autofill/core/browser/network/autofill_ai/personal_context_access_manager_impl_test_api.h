// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_TEST_API_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

class PersonalContextAccessManagerImplTestApi {
 public:
  explicit PersonalContextAccessManagerImplTestApi(
      PersonalContextAccessManagerImpl* manager)
      : manager_(*manager) {}

  void ResetCacheForType(EntityTypeName type_name) {
    manager_->ResetCacheForType(type_name);
  }

  void CachePrefetchedEntities(std::vector<EntityInstance> entities) {
    absl::flat_hash_map<EntityTypeName, std::vector<EntityInstance>>
        grouped_entities;
    for (EntityInstance& entity : entities) {
      grouped_entities[entity.type().name()].push_back(std::move(entity));
    }
    manager_->CachePrefetchedEntities(std::move(grouped_entities));
  }

  void CacheUnmaskedSpiiEntity(EntityInstance entity) {
    manager_->CacheUnmaskedSpiiEntity(std::move(entity));
  }

 private:
  raw_ref<PersonalContextAccessManagerImpl> manager_;
};

inline PersonalContextAccessManagerImplTestApi test_api(
    PersonalContextAccessManagerImpl& manager) {
  return PersonalContextAccessManagerImplTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_TEST_API_H_
