// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace autofill {

// Manages entity suggestion suppression in Autofill AI.
// Enables users to dismiss specific entity suggestions without deleting the
// underlying source data. Entity matching is based on satisfied merge
// constraints.
class EntitySuppressionManager : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEntitySuppressionsChanged() {}
  };

  ~EntitySuppressionManager() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Suppresses an entity by recording its satisfied merge constraints.
  // Returns true if suppression status was modified.
  virtual bool SuppressEntity(const EntityInstance& entity) = 0;

  // Removes suppression for an entity's merge constraints.
  // Returns true if suppression status was modified.
  virtual bool UnsuppressEntity(const EntityInstance& entity) = 0;

  // Removes all recorded entity suppressions.
  // Returns true if any suppression was removed.
  virtual bool ClearAllSuppressions() = 0;

  // Returns true if the entity matches at least one suppressed merge
  // constraint.
  virtual bool IsSuppressed(const EntityInstance& entity) const = 0;

  // Returns the delegate for Chrome Sync.
  // TODO(crbug.com/501036619): Once the sync bridge rollout is complete and
  // InMemoryEntitySuppressionManager is removed, drop this interface and merge
  // EntitySuppressionManagerImpl directly into a concrete
  // EntitySuppressionManager class.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_
