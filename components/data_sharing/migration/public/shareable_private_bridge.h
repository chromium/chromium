// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_SHAREABLE_PRIVATE_BRIDGE_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_SHAREABLE_PRIVATE_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/data_sharing/migration/public/sync_entity_identifier.h"

namespace syncer {
class EntityData;
}  // namespace syncer

namespace data_sharing {

// Interface for a feature's PRIVATE data type sync bridge that can participate
// in the migration framework.
class ShareablePrivateBridge {
 public:
  virtual ~ShareablePrivateBridge() = default;

  // Gets all entities associated with the given context ID.
  virtual std::vector<SyncEntityIdentifier> GetEntitiesForContext(
      const ContextId& context_id) = 0;

  // Deletes all entities associated with the given context ID.
  virtual void DeleteEntitiesForContext(const ContextId& context_id) = 0;

  // Adds a new entity to the bridge (e.g., during an unshare migration).
  virtual void AddEntity(std::unique_ptr<syncer::EntityData> entity_data) = 0;

  // Removes a single entity by its ID.
  virtual void RemoveEntity(const std::string& entity_id) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_SHAREABLE_PRIVATE_BRIDGE_H_
