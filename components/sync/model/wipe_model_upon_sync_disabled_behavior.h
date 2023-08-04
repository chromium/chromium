// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_WIPE_MODEL_UPON_SYNC_DISABLED_BEHAVIOR_H_
#define COMPONENTS_SYNC_MODEL_WIPE_MODEL_UPON_SYNC_DISABLED_BEHAVIOR_H_

namespace syncer {

// Determines whether and under which circumstances the sync code dealing with a
// specific local model should wipe all data stored in the local model.
enum class WipeModelUponSyncDisabledBehavior {
  // Local data is never deleted. In this case, the lifetime of local data is
  // fully decoupled from sync metadata's.
  kNever,
  // Local data is deleted every time sync is permanently disabled (useful for
  // account storage). In this case, the lifetime of local data is fully coupled
  // with sync metadata's.
  kAlways,
  // Local data is deleted at most once, the next time sync is permanently
  // disabled. In practice, this is currently used on iOS only, in advanced
  // cases involving restored-from-backup data.
  kOnceIfTrackingMetadata,
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_WIPE_MODEL_UPON_SYNC_DISABLED_BEHAVIOR_H_
