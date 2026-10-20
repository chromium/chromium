// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_CLIENT_H_

#include <optional>

#include "components/sync/base/passphrase_enums.h"

namespace sync_tab_context {

// Interface for embedder-specific dependencies of TabContextSyncService.
class TabContextSyncClient {
 public:
  TabContextSyncClient() = default;
  TabContextSyncClient(const TabContextSyncClient&) = delete;
  TabContextSyncClient& operator=(const TabContextSyncClient&) = delete;
  virtual ~TabContextSyncClient() = default;

  // Returns the currently active Sync passphrase type, or `std::nullopt` if
  // unknown (e.g. Sync is not active or engine is not initialized).
  virtual std::optional<syncer::PassphraseType> GetPassphraseType() const = 0;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_CLIENT_H_
