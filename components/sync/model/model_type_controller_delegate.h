// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/model_error.h"

namespace syncer {

struct DataTypeActivationRequest;
struct DataTypeActivationResponse;
struct TypeEntitiesCount;

// The ModelTypeControllerDelegate handles communication of ModelTypeController
// with the data type. Unlike the controller which lives on the UI thread, the
// delegate can assume all its functions are run on the model thread.
class ModelTypeControllerDelegate {
 public:
  using AllNodesCallback =
      base::OnceCallback<void(ModelType, base::Value::List)>;
  using StartCallback =
      base::OnceCallback<void(std::unique_ptr<DataTypeActivationResponse>)>;

  virtual ~ModelTypeControllerDelegate() = default;

  // Gathers additional information needed before the processor can be
  // connected to a sync worker. Once the metadata has been loaded, the info
  // is collected and given to |callback|.
  virtual void OnSyncStarting(const DataTypeActivationRequest& request,
                              StartCallback callback) = 0;

  // Indicates that we no longer want to do any sync-related things for this
  // data type. Severs all ties to the sync thread, and depending on
  // |metadata_fate|, might delete all local sync metadata.
  virtual void OnSyncStopping(SyncStopMetadataFate metadata_fate) = 0;

  // Returns a Value::List representing all nodes for the type to |callback|.
  // Used for populating nodes in Sync Node Browser of chrome://sync-internals.
  virtual void GetAllNodesForDebugging(AllNodesCallback callback) = 0;

  // Returns TypeEntitiesCount for the type to |callback|.
  // Used for updating data type counts in chrome://sync-internals.
  virtual void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback) const = 0;

  // Records entities count and estimated memory usage of the type into
  // histograms.
  virtual void RecordMemoryUsageAndCountsHistograms() = 0;

  // Clear metadata if the model is stopped.
  virtual void ClearMetadataIfStopped() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_CONTROLLER_DELEGATE_H_
