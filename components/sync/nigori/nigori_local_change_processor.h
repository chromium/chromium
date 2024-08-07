// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_LOCAL_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_LOCAL_CHANGE_PROCESSOR_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace syncer {

class DataTypeControllerDelegate;
class NigoriSyncBridge;
struct EntityData;

struct NigoriMetadataBatch {
  NigoriMetadataBatch();

  NigoriMetadataBatch(const NigoriMetadataBatch&) = delete;
  NigoriMetadataBatch& operator=(const NigoriMetadataBatch&) = delete;

  NigoriMetadataBatch(NigoriMetadataBatch&& other);

  ~NigoriMetadataBatch();

  sync_pb::DataTypeState data_type_state;
  std::optional<sync_pb::EntityMetadata> entity_metadata;
};

// Interface analogous to DataTypeLocalChangeProcessor for Nigori, used to
// propagate local changes from the bridge to the processor.
class NigoriLocalChangeProcessor {
 public:
  NigoriLocalChangeProcessor() = default;

  NigoriLocalChangeProcessor(const NigoriLocalChangeProcessor&) = delete;
  NigoriLocalChangeProcessor& operator=(const NigoriLocalChangeProcessor&) =
      delete;

  virtual ~NigoriLocalChangeProcessor() = default;

  // The Nigori model is expected to call this method as soon as possible during
  // initialization, and must be called before invoking Put(). |bridge| must not
  // be null and most outlive the NigoriLocalChangeProcessor.
  virtual void ModelReadyToSync(NigoriSyncBridge* bridge,
                                NigoriMetadataBatch nigori_metadata) = 0;

  // Informs the Nigori processor of a new or updated Nigori entity.
  virtual void Put(std::unique_ptr<EntityData> entity_data) = 0;

  // Returns true the Nigori entity as tracked by the processor has local
  // changes. A commit may or may not be in progress at this time.
  virtual bool IsEntityUnsynced() = 0;

  // Returns both the entity metadata and data type state such that the Nigori
  // model takes care of persisting them.
  virtual NigoriMetadataBatch GetMetadata() = 0;

  // Reports an error in the model to sync. Should be called for any persistence
  // or consistency error the bridge encounters outside of a method that allows
  // returning a ModelError directly. Outstanding callbacks are not expected to
  // be called after an error. This will result in sync being temporarily
  // disabled (generally until the next restart).
  virtual void ReportError(const ModelError& error) = 0;

  // Returns the delegate for the controller.
  virtual base::WeakPtr<DataTypeControllerDelegate> GetControllerDelegate() = 0;

  // Returns a boolean representing whether the processor's metadata is
  // currently up to date and accurately tracking the data type's data. If
  // false, and ModelReadyToSync() has already been called, then Put and Delete
  // will no-op and can be omitted by bridge.
  virtual bool IsTrackingMetadata() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_LOCAL_CHANGE_PROCESSOR_H_
