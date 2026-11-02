// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace autofill {

struct EntitySuppressionEntry;

// Manages personal context entity metadata (e.g., suggestion suppressions) in
// Autofill AI. Enables users to dismiss specific entity suggestions without
// deleting the underlying source data. Entity matching is based on satisfied
// merge constraints.
class PersonalContextEntityMetadataManager final
    : public KeyedService,
      public PersonalContextEntityMetadataSyncBridge::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEntitySuppressionsChanged() {}
  };

  explicit PersonalContextEntityMetadataManager(
      std::unique_ptr<PersonalContextEntityMetadataSyncBridge> sync_bridge);
  PersonalContextEntityMetadataManager(
      const PersonalContextEntityMetadataManager&) = delete;
  PersonalContextEntityMetadataManager& operator=(
      const PersonalContextEntityMetadataManager&) = delete;
  ~PersonalContextEntityMetadataManager() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Suppresses an entity by recording its satisfied merge constraints.
  // Returns true if suppression status was modified.
  bool SuppressEntity(EntityInstance entity);

  // Removes suppression for the merge constraints of the entity with
  // `entity_id` that was suppressed in the current browser session.
  // Returns true if suppression status was modified.
  bool UndoInSessionSuppressedEntity(const EntityInstance::EntityId& entity_id);

  // Removes recorded entity suppressions whose creation timestamp falls within
  // the inclusive range `[delete_begin, delete_end]` (use `base::Time()` and
  // `base::Time::Max()` to clear all suppressions). Deletes matching entries
  // from local storage and Sync, prunes any corresponding in-session undo
  // state, and notifies observers if any entry was removed.
  // Returns `true` if at least one suppression was removed, `false` otherwise.
  bool ClearSuppressions(base::Time delete_begin, base::Time delete_end);

  // Returns true if the entity matches at least one suppressed merge
  // constraint.
  bool IsSuppressed(const EntityInstance& entity) const;

  // Returns the number of recorded entity suppressions whose creation
  // timestamp falls within the inclusive range `[begin, end]` (use
  // `base::Time()` and `base::Time::Max()` to cover all suppressions). This
  // mirrors the range semantics of `ClearSuppressions()`, i.e. it returns the
  // number of suppressions that `ClearSuppressions(begin, end)` would remove.
  size_t CountSuppressionsCreatedBetween(base::Time begin,
                                         base::Time end) const;

  // Returns the delegate for Chrome Sync.
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate();

 private:
  // PersonalContextEntityMetadataSyncBridge::Observer:
  void OnSuppressionsChanged() override;

  base::ObserverList<Observer> observers_;

  std::unique_ptr<PersonalContextEntityMetadataSyncBridge> sync_bridge_;
  base::ScopedObservation<PersonalContextEntityMetadataSyncBridge,
                          PersonalContextEntityMetadataSyncBridge::Observer>
      bridge_observation_{this};

  // Maps entity IDs suppressed in the current browser session to their
  // suppression entries so that `UndoInSessionSuppressedEntity()` can look
  // them up by ID even after the entity has been evicted from
  // `EntityDataManager`.
  absl::flat_hash_map<EntityInstance::EntityId,
                      std::vector<EntitySuppressionEntry>>
      suppressed_entries_by_entity_id_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_H_
