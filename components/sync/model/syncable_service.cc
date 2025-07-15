// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/syncable_service.h"

namespace syncer {

void SyncableService::WillStartInitialSync() {}

void SyncableService::OnBrowserShutdown(DataType type) {
  // Stop the syncable service to make sure instances of LocalChangeProcessor
  // are not continued to be used.
  // TODO(crbug.com/40883731): This is a temporary workaround.
  // OnBrowserShutdown() should ideally have a default empty implementation. If
  // not feasible, a better long-term approach should be to pass an
  // `is_browser_shutdown` flag to StopSyncing() instead of using this method.
  StopSyncing(type);
}

void SyncableService::StayStoppedAndMaybeClearData(DataType type) {
  // TODO(crbug.com/401453180): Either add a default implementation to start the
  // syncable service followed by a call to StopSyncing(), or implement this
  // method for the relevant syncable services.
}

bool SyncableService::SupportsGetClientTag() const {
  return true;
}

}  // namespace syncer
