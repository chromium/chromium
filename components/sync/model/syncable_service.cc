// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/syncable_service.h"

namespace syncer {
void SyncableService::OnBrowserShutdown(DataType type) {
  // Stop the syncable service to make sure instances of LocalChangeProcessor
  // are not continued to be used.
  // TODO(crbug.com/40883731): This is a temporary workaround.
  // OnBrowserShutdown() should ideally have a default empty implementation. If
  // not feasible, a better long-term approach should be to pass an
  // `is_browser_shutdown` flag to StopSyncing() instead of using this method.
  StopSyncing(type);
}
}  // namespace syncer
