// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_METADATA_BATCH_H_
#define COMPONENTS_SYNC_MODEL_METADATA_BATCH_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/protocol/data_type_state.pb.h"

namespace sync_pb {
class EntityMetadata;
}  // namespace sync_pb

namespace syncer {

// Map of storage keys to EntityMetadata proto.
using EntityMetadataMap =
    std::map<std::string, std::unique_ptr<sync_pb::EntityMetadata>>;

// Container used to pass sync metadata from services to their processor.
class MetadataBatch {
 public:
  MetadataBatch();
  MetadataBatch(MetadataBatch&&);
  ~MetadataBatch();

  MetadataBatch(const MetadataBatch&) = delete;

  // Read-only access to the entire metadata map.
  const EntityMetadataMap& GetAllMetadata() const;

  // Allows the caller to take ownership of the entire metadata map. This is
  // done because the caller will probably swap out all the EntityMetadata
  // protos from the map for performance reasons.
  EntityMetadataMap TakeAllMetadata();

  // Add |metadata| for |storage_key| to the batch.
  void AddMetadata(const std::string& storage_key,
                   std::unique_ptr<sync_pb::EntityMetadata> metadata);

  // Get the DataTypeState for this batch.
  const sync_pb::DataTypeState& GetDataTypeState() const;

  // Set the DataTypeState for this batch.
  void SetDataTypeState(const sync_pb::DataTypeState& state);

 private:
  EntityMetadataMap metadata_map_;
  sync_pb::DataTypeState state_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_METADATA_BATCH_H_
