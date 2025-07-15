// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_window_delegate.h"

#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

bool SyncedWindowDelegate::IsPlaceholderTabAt(int index) const {
  return GetTabAt(index)->IsPlaceholderTab();
}

}  // namespace sync_sessions
