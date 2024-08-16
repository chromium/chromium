// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_
#define COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_

#include "components/sync/base/data_type.h"

namespace syncer {

// An enum to represent the resolution of a data conflict. We either:
// 1) Use the local client data and update the server.
// 2) Use the remote server data and update the client.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncConflictResolution)
enum class ConflictResolution {
  kChangesMatch = 0,
  kUseLocal = 1,
  kUseRemote = 2,
  kIgnoreLocalEncryption = 3,
  kIgnoreRemoteEncryption = 4,

  kMaxValue = kIgnoreRemoteEncryption
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncConflictResolution)

// Records the conflict resolution outcome if there is any during applying
// remote updates.
void RecordDataTypeEntityConflictResolution(DataType data_type,
                                            ConflictResolution resolution_type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_
