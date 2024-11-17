// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATE_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATE_H_

#include "components/sessions/core/session_id.h"

namespace sync_sessions {

class SyncedTabDelegate;

// A SyncedWindowDelegate is used to insulate the sync code from depending
// directly on Browser and BrowserList.
class SyncedWindowDelegate {
 public:
  // Methods originating from Browser.

  // Returns true iff this browser has a visible window representation
  // associated with it. Sometimes, if a window is being created/removed the
  // model object may exist without its UI counterpart.
  virtual bool HasWindow() const = 0;

  // see Browser::session_id
  virtual SessionID GetSessionId() const = 0;

  // see Browser::tab_count
  virtual int GetTabCount() const = 0;

  // see Browser::is_type_normal
  virtual bool IsTypeNormal() const = 0;

  // see Browser::is_type_popup
  virtual bool IsTypePopup() const = 0;

  // Methods derivated from Browser

  // Returns true iff the provided tab is currently "pinned" in the tab strip.
  virtual bool IsTabPinned(const SyncedTabDelegate* tab) const = 0;

  // see TabStripModel::GetWebContentsAt
  virtual SyncedTabDelegate* GetTabAt(int index) const = 0;

  // Return the tab id for the tab at |index|.
  virtual SessionID GetTabIdAt(int index) const = 0;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const = 0;

  // Helper methods.

  // Return true if this window should be considered for syncing.
  virtual bool ShouldSync() const = 0;

 protected:
  virtual ~SyncedWindowDelegate() = default;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATE_H_
