// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_MODEL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_error.h"

namespace sync_pb {
class DataTypeState;
class GarbageCollectionDirective;
}  // namespace sync_pb

namespace syncer {

class DataTypeSyncBridge;
class ProcessorEntityTracker;
class ProcessorEntity;

// A sync component that performs updates from sync server.
class ClientTagBasedRemoteUpdateHandler {
 public:
  // All parameters must not be nullptr and they must outlive this object.
  ClientTagBasedRemoteUpdateHandler(DataType type,
                                    DataTypeSyncBridge* bridge,
                                    ProcessorEntityTracker* entities);

  // Processes incremental updates from the sync server.
  std::optional<ModelError> ProcessIncrementalUpdate(
      const sync_pb::DataTypeState& data_type_state,
      UpdateResponseDataList updates,
      std::optional<sync_pb::GarbageCollectionDirective> gc_directive);

  ClientTagBasedRemoteUpdateHandler(const ClientTagBasedRemoteUpdateHandler&) =
      delete;
  ClientTagBasedRemoteUpdateHandler& operator=(
      const ClientTagBasedRemoteUpdateHandler&) = delete;

 private:
  // Helper function to process the update for a single entity. If a local data
  // change is required, it will be added to |entity_changes|. The return value
  // is the tracked entity, or nullptr if the update should be ignored.
  // |storage_key_to_clear| must not be null and allows the implementation to
  // indicate that a certain storage key is now obsolete and should be cleared,
  // which is leveraged in certain conflict resolution scenarios.
  ProcessorEntity* ProcessUpdate(UpdateResponseData update,
                                 EntityChangeList* entity_changes,
                                 std::string* storage_key_to_clear);

  // Resolve a conflict between |update| and the pending commit in |entity|.
  void ResolveConflict(UpdateResponseData update,
                       ProcessorEntity* entity,
                       EntityChangeList* changes,
                       std::string* storage_key_to_clear);

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntity* GetEntityForTagHash(const ClientTagHash& tag_hash);

  // Creates an entity in the entity tracker for |storage_key| queried from the
  // bridge for the given |update|. Provided |storage_key| (if any, i.e. if
  // non-empty) must not exist in the entity tracker.
  ProcessorEntity* CreateEntity(const UpdateResponseData& update);

  // The data type this object syncs.
  const DataType type_;

  // DataTypeSyncBridge linked to associated processor.
  const raw_ptr<DataTypeSyncBridge> bridge_;

  // A map of client tag hash to sync entities known to the processor.
  // Should be replaced with new interface.
  const raw_ptr<ProcessorEntityTracker> entity_tracker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_
