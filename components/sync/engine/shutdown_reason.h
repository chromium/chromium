// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SHUTDOWN_REASON_H_
#define COMPONENTS_SYNC_ENGINE_SHUTDOWN_REASON_H_

#include "components/sync/base/sync_stop_metadata_fate.h"

namespace syncer {

// Reason for shutting down the sync engine.
enum class ShutdownReason {
  // The Sync engine is being stopped with the expectation that it will be
  // started again for the same user before too long, so any Sync metadata
  // should be kept. An example is content-area signout while Sync-the-feature
  // is enabled.
  STOP_SYNC_AND_KEEP_DATA,
  // The Sync engine is being stopped with the expectation that it will *not* be
  // started again for the same user soon, or with the explicit intention of
  // clearing Sync metadata. Examples include signout while only
  // Sync-the-transport is active, the "Turn off" button in settings for
  // Sync-the-feature users, and clearing data via the Sync dashboard.
  DISABLE_SYNC_AND_CLEAR_DATA,
  // The browser is being shut down; any Sync metadata should be kept.
  BROWSER_SHUTDOWN_AND_KEEP_DATA,
};

const char* ShutdownReasonToString(ShutdownReason reason);

SyncStopMetadataFate ShutdownReasonToSyncStopMetadataFate(
    ShutdownReason shutdown_reason);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SHUTDOWN_REASON_H_
