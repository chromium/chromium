// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IN_MEMORY_METADATA_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_IN_MEMORY_METADATA_CHANGE_LIST_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

// A MetadataChangeList base class that stores changes in member fields.
// There are no accessors; TransferChangesTo() can be used to forward all
// changes to another instance.
class InMemoryMetadataChangeList : public MetadataChangeList {
 public:
  InMemoryMetadataChangeList();
  ~InMemoryMetadataChangeList() override;

  // Moves all currently accumulated changes into |*other|, resetting the state
  // of |*this| to the default, empty state.
  void TransferChangesTo(MetadataChangeList* other);

  // MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;

 private:
  enum ChangeType { UPDATE, CLEAR };

  struct MetadataChange {
    ChangeType type;
    sync_pb::EntityMetadata metadata;
  };

  struct ModelTypeStateChange {
    ChangeType type;
    sync_pb::ModelTypeState state;
  };

  std::map<std::string, MetadataChange> metadata_changes_;
  std::unique_ptr<ModelTypeStateChange> state_change_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IN_MEMORY_METADATA_CHANGE_LIST_H_
