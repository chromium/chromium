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

// SyncMetadataStore defines interface implemented by model types for persisting
// sync metadata and datatype state. It allows model type to use common
// implementation of MetadataChangeList (SyncMetadataStoreChangeList) instead of
// implementing their own.
// Model type in implementation of ModelTypeSyncBridge::CreateMetadataChangeList
// should create instance of SyncMetadataStoreChangeList passing pointer to
// SyncMetadataStore to its constructor.
// Implementations of SyncMetadataStore methods should support add/update/delete
// metadata in model type specific sync metadata storage.

class SyncMetadataStore {
 public:
  SyncMetadataStore() {}
  virtual ~SyncMetadataStore() {}

  // Update the metadata row for |model_type|, keyed by |storage_key|, to
  // contain the contents of |metadata|.
  // Return true on success.
  virtual bool UpdateSyncMetadata(syncer::ModelType model_type,
                                  const std::string& storage_key,
                                  const sync_pb::EntityMetadata& metadata) = 0;

  // Remove the metadata row of type |model_type| keyed by |storage_key|.
  // Return true on success.
  virtual bool ClearSyncMetadata(syncer::ModelType model_type,
                                 const std::string& storage_key) = 0;

  // Update the stored sync state for the |model_type|.
  // Return true on success.
  virtual bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) = 0;

  // Clear the stored sync state for |model_type|.
  // Return true on success.
  virtual bool ClearModelTypeState(syncer::ModelType model_type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_
