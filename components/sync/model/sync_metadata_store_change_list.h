// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_CHANGE_LIST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_metadata_store.h"

namespace sync_pb {
class DataTypeState;
class EntityMetadata;
}  // namespace sync_pb

namespace syncer {

// A thin wrapper around an SyncMetadataStore that implements sync's
// MetadataChangeList interface. Changes are passed directly into the store and
// not stored inside this object.
class SyncMetadataStoreChangeList : public MetadataChangeList {
 public:
  using ErrorCallback = base::RepeatingCallback<void(const ModelError&)>;

  // If an error happened during any Update*/Clear* operation, then
  // `error_callback` will be called during destruction and passed the error.
  // Should typically be bound to DataTypeLocalChangeProcessor::ReportError().
  SyncMetadataStoreChangeList(SyncMetadataStore* store,
                              DataType type,
                              ErrorCallback error_callback);
  ~SyncMetadataStoreChangeList() override;

  // MetadataChangeList implementation.
  void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) override;
  void ClearDataTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;

  const SyncMetadataStore* GetMetadataStoreForTesting() const;

 private:
  void SetError(ModelError error);

  // The metadata store to store metadata in; always outlives |this|.
  const raw_ptr<SyncMetadataStore> store_;

  // The sync data type for this metadata.
  DataType type_;

  // Whether this object encountered any error previously. Used to prevent any
  // further changes, and to ensure that only the first error gets passed on
  // (since any subsequent ones likely aren't meaningful).
  bool error_encountered_ = false;

  const ErrorCallback error_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_CHANGE_LIST_H_
