// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~BrowserSyncedWindowDelegate() override;

  // SyncedWindowDelegate:
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const sync_sessions::SyncedTabDelegate* tab) const override;
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSyncedWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
