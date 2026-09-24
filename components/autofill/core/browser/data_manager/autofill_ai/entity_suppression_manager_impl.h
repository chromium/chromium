// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

namespace autofill {

class EntityInstance;

// Implementation of `EntitySuppressionManager` backed by
// `EntitySuppressionSyncBridge`.
class EntitySuppressionManagerImpl
    : public EntitySuppressionManager,
      public EntitySuppressionSyncBridge::Observer {
 public:
  explicit EntitySuppressionManagerImpl(
      std::unique_ptr<EntitySuppressionSyncBridge> sync_bridge);
  EntitySuppressionManagerImpl(const EntitySuppressionManagerImpl&) = delete;
  EntitySuppressionManagerImpl& operator=(const EntitySuppressionManagerImpl&) =
      delete;
  ~EntitySuppressionManagerImpl() override;

  // EntitySuppressionManager:
  void AddObserver(EntitySuppressionManager::Observer* observer) override;
  void RemoveObserver(EntitySuppressionManager::Observer* observer) override;
  bool SuppressEntity(const EntityInstance& entity) override;
  bool UnsuppressEntity(const EntityInstance& entity) override;
  bool ClearAllSuppressions() override;
  bool IsSuppressed(const EntityInstance& entity) const override;

  // EntitySuppressionSyncBridge::Observer:
  void OnSuppressionsChanged() override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate()
      override;

 private:
  base::ObserverList<EntitySuppressionManager::Observer> observers_;

  std::unique_ptr<EntitySuppressionSyncBridge> sync_bridge_;
  base::ScopedObservation<EntitySuppressionSyncBridge,
                          EntitySuppressionSyncBridge::Observer>
      bridge_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_IMPL_H_
