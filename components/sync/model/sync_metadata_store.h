// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_
#define COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_

#include <string>

#include "components/sync/base/model_type.h"

namespace sync_pb {
class EntityMetadata;
class ModelTypeState;
}  // namespace sync_pb

namespace syncer {

// SyncMetadataStore defines the interface implemented by model types for
// persisting sync metadata, both per-entity metadata and the overall datatype
// state. It allows model types to use a common implementation of
// MetadataChangeList (SyncMetadataStoreChangeList) instead of implementing
// their own. In their implementation of
// ModelTypeSyncBridge::CreateMetadataChangeList, model types should create an
// instance of SyncMetadataStoreChangeList, passing a pointer to their
// SyncMetadataStore to its constructor. Implementations of SyncMetadataStore
// methods should support add/update/delete operations in the
// model-type-specific sync metadata storage.
class SyncMetadataStore {
 public:
  SyncMetadataStore() = default;
  virtual ~SyncMetadataStore() = default;

  // Updates the metadata row of type |model_type| for the entity identified by
  // |storage_key| to contain the contents of |metadata|.
  // Returns true on success.
  virtual bool UpdateEntityMetadata(
      syncer::ModelType model_type,
      const std::string& storage_key,
      const sync_pb::EntityMetadata& metadata) = 0;

  // Removes the metadata row of type |model_type| for the entity identified by
  // |storage_key|.
  // Returns true on success.
  virtual bool ClearEntityMetadata(syncer::ModelType model_type,
                                   const std::string& storage_key) = 0;

  // Updates the per-type ModelTypeState state for the |model_type|.
  // Returns true on success.
  virtual bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) = 0;

  // Clears the per-type ModelTypeState for |model_type|.
  // Returns true on success.
  virtual bool ClearModelTypeState(syncer::ModelType model_type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_
