// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_PROCESS_UPDATES_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_PROCESS_UPDATES_UTIL_H_

#include <stdint.h>

#include <vector>

#include "components/sync/base/model_type.h"

namespace sync_pb {
class SyncEntity;
}

namespace syncer {

class StatusController;
struct UpdateCounters;

namespace syncable {
class ModelNeutralWriteTransaction;
class Directory;
}

using SyncEntityList = std::vector<const sync_pb::SyncEntity*>;

// Processes all the updates associated with a single ModelType.
void ProcessDownloadedUpdates(syncable::Directory* dir,
                              syncable::ModelNeutralWriteTransaction* trans,
                              ModelType type,
                              const SyncEntityList& applicable_updates,
                              bool is_initial_sync,
                              StatusController* status,
                              UpdateCounters* counters);

// Tombstones all entries of |type| whose versions are older than
// |version_watermark| unless they are type root or unsynced/unapplied.
void ExpireEntriesByVersion(syncable::Directory* dir,
                            syncable::ModelNeutralWriteTransaction* trans,
                            ModelType type,
                            int64_t version_watermark);

// Tombstones all entries of |type| whose ages are older than
// |age_watermark_in_days| unless they are type root or unsynced/unapplied.
void ExpireEntriesByAge(syncable::Directory* dir,
                        syncable::ModelNeutralWriteTransaction* trans,
                        ModelType type,
                        int32_t age_watermark_in_days);


}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_PROCESS_UPDATES_UTIL_H_
