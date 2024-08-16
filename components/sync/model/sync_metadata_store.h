// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_
#define COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_

#include <string>

#include "components/sync/base/data_type.h"

namespace sync_pb {
class DataTypeState;
class EntityMetadata;
}  // namespace sync_pb

namespace syncer {

// SyncMetadataStore defines the interface implemented by data types for
// persisting sync metadata, both per-entity metadata and the overall datatype
// state. It allows data types to use a common implementation of
// MetadataChangeList (SyncMetadataStoreChangeList) instead of implementing
// their own. In their implementation of
// DataTypeSyncBridge::CreateMetadataChangeList, data types should create an
// instance of SyncMetadataStoreChangeList, passing a pointer to their
// SyncMetadataStore to its constructor. Implementations of SyncMetadataStore
// methods should support add/update/delete operations in the
// data-type-specific sync metadata storage.
class SyncMetadataStore {
 public:
  SyncMetadataStore() = default;
  virtual ~SyncMetadataStore() = default;

  // Updates the metadata row of type |data_type| for the entity identified by
  // |storage_key| to contain the contents of |metadata|.
  // Returns true on success.
  virtual bool UpdateEntityMetadata(
      syncer::DataType data_type,
      const std::string& storage_key,
      const sync_pb::EntityMetadata& metadata) = 0;

  // Removes the metadata row of type |data_type| for the entity identified by
  // |storage_key|.
  // Returns true on success.
  virtual bool ClearEntityMetadata(syncer::DataType data_type,
                                   const std::string& storage_key) = 0;

  // Updates the per-type DataTypeState state for the |data_type|.
  // Returns true on success.
  virtual bool UpdateDataTypeState(
      syncer::DataType data_type,
      const sync_pb::DataTypeState& data_type_state) = 0;

  // Clears the per-type DataTypeState for |data_type|.
  // Returns true on success.
  virtual bool ClearDataTypeState(syncer::DataType data_type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_H_
