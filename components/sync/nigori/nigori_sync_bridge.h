// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_

#include <memory>

#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {

struct EntityData;

// Interface implemented by Nigori model to receive Nigori updates from sync via
// a ModelTypeChangeProcessor. Provides a way for sync to update the data and
// metadata for Nigori entities, as well as the model type state.
class NigoriSyncBridge {
 public:
  NigoriSyncBridge() = default;

  NigoriSyncBridge(const NigoriSyncBridge&) = delete;
  NigoriSyncBridge& operator=(const NigoriSyncBridge&) = delete;

  virtual ~NigoriSyncBridge() = default;

  // Perform the initial merge between local and sync data.
  virtual absl::optional<ModelError> MergeFullSyncData(
      absl::optional<EntityData> data) = 0;

  // Apply changes from the sync server locally.
  virtual absl::optional<ModelError> ApplyIncrementalSyncChanges(
      absl::optional<EntityData> data) = 0;

  // Retrieve Nigori sync data.
  virtual std::unique_ptr<EntityData> GetData() = 0;

  // Informs the bridge that sync has been disabed. The bridge is responsible
  // for deleting all data and metadata upon disabling sync.
  virtual void ApplyDisableSyncChanges() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_
