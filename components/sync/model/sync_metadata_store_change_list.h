// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_CHANGE_LIST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_metadata_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_pb {
class EntityMetadata;
class ModelTypeState;
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
  // Should typically be bound to ModelTypeChangeProcessor::ReportError().
  SyncMetadataStoreChangeList(SyncMetadataStore* store,
                              ModelType type,
                              ErrorCallback error_callback);
  ~SyncMetadataStoreChangeList() override;

  // MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;

  // Allows querying and manually handling any error, instead of relying on the
  // error callback passed to the constructor.
  // TODO(crbug.com/1356990): Consider removing this method. Callers can use
  // ModelTypeChangeProcessor::GetError() instead.
  absl::optional<ModelError> TakeError();

  const SyncMetadataStore* GetMetadataStoreForTesting() const;

 private:
  void SetError(ModelError error);

  // The metadata store to store metadata in; always outlives |this|.
  raw_ptr<SyncMetadataStore> store_;

  // The sync model type for this metadata.
  ModelType type_;

  // The first error encountered by this object, if any.
  absl::optional<ModelError> error_;

  const ErrorCallback error_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_METADATA_STORE_CHANGE_LIST_H_
