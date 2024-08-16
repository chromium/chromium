// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_

#include <memory>
#include <optional>

#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"

namespace syncer {

struct EntityData;

// Interface implemented by Nigori model to receive Nigori updates from sync via
// a DataTypeLocalChangeProcessor. Provides a way for sync to update the data
// and metadata for Nigori entities, as well as the data type state.
class NigoriSyncBridge {
 public:
  NigoriSyncBridge() = default;

  NigoriSyncBridge(const NigoriSyncBridge&) = delete;
  NigoriSyncBridge& operator=(const NigoriSyncBridge&) = delete;

  virtual ~NigoriSyncBridge() = default;

  // Perform the initial merge between local and sync data.
  virtual std::optional<ModelError> MergeFullSyncData(
      std::optional<EntityData> data) = 0;

  // Apply changes from the sync server locally.
  virtual std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::optional<EntityData> data) = 0;

  // Retrieve Nigori sync data. Used only to commit the data.
  virtual std::unique_ptr<EntityData> GetDataForCommit() = 0;

  // Retrieve Nigori sync data. Used for getting data in Sync Node Browser of
  // chrome://sync-internals.
  virtual std::unique_ptr<EntityData> GetDataForDebugging() = 0;

  // Informs the bridge that sync has been disabed. The bridge is responsible
  // for deleting all data and metadata upon disabling sync.
  virtual void ApplyDisableSyncChanges() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_
