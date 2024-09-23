// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_TAB_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_TAB_DELEGATE_H_

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace sync_sessions {
class SyncedTabDelegate;
}

// A BrowserSyncedTabDelegate is the desktop implementation for
// SyncedTabDelegate, which essentially reads session IDs from SessionTabHelper.
class BrowserSyncedTabDelegate
    : public TabContentsSyncedTabDelegate,
      public content::WebContentsUserData<BrowserSyncedTabDelegate> {
 public:
  BrowserSyncedTabDelegate(const BrowserSyncedTabDelegate&) = delete;
  BrowserSyncedTabDelegate& operator=(const BrowserSyncedTabDelegate&) = delete;

  ~BrowserSyncedTabDelegate() override;

  // SyncedTabDelegate:
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsPlaceholderTab() const override;
  std::unique_ptr<SyncedTabDelegate> ReadPlaceholderTabSnapshotIfItShouldSync(
      sync_sessions::SyncSessionsClient* sessions_client) override;

 private:
  explicit BrowserSyncedTabDelegate(content::WebContents* web_contents);
  friend class content::WebContentsUserData<BrowserSyncedTabDelegate>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_TAB_DELEGATE_H_
