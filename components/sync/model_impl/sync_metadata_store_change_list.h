// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_SYNC_METADATA_STORE_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_IMPL_SYNC_METADATA_STORE_CHANGE_LIST_H_

#include <string>

#include "base/optional.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_metadata_store.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

// A thin wrapper around an SyncMetadataStore that implements sync's
// MetadataChangeList interface. Changes are passed directly into the store and
// not stored inside this object. Since the store calls can fail, |TakeError()|
// must be called before this object is destroyed to check whether any
// operations failed.
class SyncMetadataStoreChangeList : public MetadataChangeList {
 public:
  SyncMetadataStoreChangeList(SyncMetadataStore* store, syncer::ModelType type);
  ~SyncMetadataStoreChangeList() override;

  // MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;
  base::Optional<syncer::ModelError> TakeError();

  const SyncMetadataStore* GetMetadataStoreForTesting() const;

 private:
  // The metadata store to store metadata in; always outlives |this|.
  SyncMetadataStore* store_;

  // The sync model type for this metadata.
  syncer::ModelType type_;

  // The first error encountered by this object, if any.
  base::Optional<syncer::ModelError> error_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_SYNC_METADATA_STORE_CHANGE_LIST_H_
