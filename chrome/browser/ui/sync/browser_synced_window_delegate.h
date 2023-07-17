// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegate.h"

class Browser;

namespace browser_sync {
class SyncedTabDelegate;
}

// A BrowserSyncedWindowDelegate is the desktop implementation for
// SyncedWindowDelegate, representing the window corresponding to |browser|,
// and listing all its tabs.
class BrowserSyncedWindowDelegate : public sync_sessions::SyncedWindowDelegate {
 public:
  explicit BrowserSyncedWindowDelegate(Browser* browser);

  BrowserSyncedWindowDelegate(const BrowserSyncedWindowDelegate&) = delete;
  BrowserSyncedWindowDelegate& operator=(const BrowserSyncedWindowDelegate&) =
      delete;

  ~BrowserSyncedWindowDelegate() override;

  // SyncedWindowDelegate:
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const sync_sessions::SyncedTabDelegate* tab) const override;
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
