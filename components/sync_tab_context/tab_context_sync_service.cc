// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service.h"

#include "base/functional/callback.h"
#include "base/notreached.h"

namespace sync_tab_context {

TabContextSyncService::TabContextSyncService() = default;

TabContextSyncService::~TabContextSyncService() = default;

void TabContextSyncService::UploadPageContext(
    const ContainerId& container_id,
    const std::string& entry_id,
    std::string page_context,
    base::OnceCallback<void(UploadOutcome)> callback) {
  NOTREACHED();
}

}  // namespace sync_tab_context
