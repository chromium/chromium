// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONNECTOR_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONNECTOR_H_

#include <memory>

#include "components/sync/base/data_type.h"

namespace syncer {

struct DataTypeActivationResponse;

// An interface into the core parts of sync for data types. By adding/removing
// types through methods of this interface, consumers control which types will
// be syncing (receiving updates and committing local changes).
// In addition it handles creating the connection between the DataTypeWorker
// (CommitQueue) on the sync side and the DataTypeProcessor on the data type
// side.
// The real implementation (DataTypeRegistry) lives on the sync sequence, but
// there's a proxy object on the UI thread for use by the SyncEngine.
class DataTypeConnector {
 public:
  DataTypeConnector() = default;
  virtual ~DataTypeConnector() = default;

  // Connect a worker on the sync sequence and |type|'s processor on the model
  // sequence. Note that in production |activation_response| actually
  // owns a processor proxy that forwards calls to the model sequence and is
  // safe to call from the sync sequence.
  virtual void ConnectDataType(
      DataType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) = 0;

  // Disconnects the worker from |type|'s processor and stop syncing the type.
  //
  // This is the sync sequence's chance to clear state associated with the type.
  // It also causes the syncer to stop requesting updates for this type, and to
  // abort any in-progress commit requests.
  //
  // No-op if the type is not connected.
  virtual void DisconnectDataType(DataType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONNECTOR_H_
