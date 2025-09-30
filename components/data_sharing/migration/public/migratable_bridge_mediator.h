// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_BRIDGE_MEDIATOR_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_BRIDGE_MEDIATOR_H_

#include <memory>
#include <vector>

#include "base/uuid.h"

namespace syncer {
class EntityData;
}  // namespace syncer

namespace data_sharing {

// A mediator responsible for handling the core migration business logic for a
// specific feature, acting as the intermediary between the
// MigratableSyncService and the two bridges.
class MigratableBridgeMediator {
 public:
  virtual ~MigratableBridgeMediator() = default;

  // Instructs the mediator to begin staging the migration for `context_id`.
  virtual void StageMigration(const ContextId& context_id) = 0;

  // Instructs the mediator to commit the migration for `context_id`.
  virtual void CommitMigration(const ContextId& context_id) = 0;

  // Called by the bridges when an entity is received from sync.
  virtual void OnEntityReceived(const syncer::EntityData* entity_data) = 0;

  // Called by the bridges on startup once all local data has been loaded.
  virtual void OnSyncBridgeInitialized(
      std::vector<std::unique_ptr<syncer::EntityData>> data) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_BRIDGE_MEDIATOR_H_
