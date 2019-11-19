// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"

namespace syncer {

struct EntityData;

// Interface implemented by Nigori model to receive Nigori updates from sync via
// a ModelTypeChangeProcessor. Provides a way for sync to update the data and
// metadata for Nigori entities, as well as the model type state.
class NigoriSyncBridge {
 public:
  NigoriSyncBridge() = default;

  virtual ~NigoriSyncBridge() = default;

  // Perform the initial merge between local and sync data.
  virtual base::Optional<ModelError> MergeSyncData(
      base::Optional<EntityData> data) = 0;

  // Apply changes from the sync server locally.
  virtual base::Optional<ModelError> ApplySyncChanges(
      base::Optional<EntityData> data) = 0;

  // Retrieve Nigori sync data.
  virtual std::unique_ptr<EntityData> GetData() = 0;

  // Informs the bridge that sync has been disabed. The bridge is responsible
  // for deleting all data and metadata upon disabling sync.
  virtual void ApplyDisableSyncChanges() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NigoriSyncBridge);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_H_
