// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_CONNECTOR_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_CONNECTOR_H_

#include <memory>

#include "components/sync/base/model_type.h"

namespace syncer {

struct DataTypeActivationResponse;

// An interface into the core parts of sync for model types. By adding/removing
// types through methods of this interface consumer controls which types will be
// syncing (receiving updates and committing local changes).
// In addition it handles creating the connection between the ModelTypeWorker
// (CommitQueue) on the sync side and the (Shared)ModelTypeProcessor on the
// model type side.
class ModelTypeConnector {
 public:
  ModelTypeConnector() = default;
  virtual ~ModelTypeConnector() = default;

  // Connect a worker on the sync thread and |type|'s processor on the model
  // thread. Note that in production |activation_response| actually
  // owns a processor proxy that forwards calls to the model thread and is safe
  // to call from the sync thread.
  virtual void ConnectDataType(
      ModelType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) = 0;

  // Disconnects the worker from |type|'s processor and stop syncing the type.
  //
  // This is the sync thread's chance to clear state associated with the type.
  // It also causes the syncer to stop requesting updates for this type, and to
  // abort any in-progress commit requests.
  //
  // No-op if the type is not connected.
  virtual void DisconnectDataType(ModelType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_CONNECTOR_H_
