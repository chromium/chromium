// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_TAB_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_TAB_DELEGATE_H_

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace sync_sessions {
class SyncedTabDelegate;
}

namespace tabs {
class TabInterface;
}

// A BrowserSyncedTabDelegate is the desktop implementation for
// SyncedTabDelegate, which essentially reads session IDs from
// SessionTabHelper. It is owned by the tab's TabFeatures.
class BrowserSyncedTabDelegate : public TabContentsSyncedTabDelegate {
 public:
  DECLARE_USER_DATA(BrowserSyncedTabDelegate);

  // `web_contents` is passed explicitly because during a discard the helper
  // is recreated for the incoming WebContents before `tab` swaps its
  // contents.
  BrowserSyncedTabDelegate(tabs::TabInterface& tab,
                           content::WebContents* web_contents);

  BrowserSyncedTabDelegate(const BrowserSyncedTabDelegate&) = delete;
  BrowserSyncedTabDelegate& operator=(const BrowserSyncedTabDelegate&) = delete;

  ~BrowserSyncedTabDelegate() override;

  static BrowserSyncedTabDelegate* From(tabs::TabInterface* tab);

  // SyncedTabDelegate:
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsPlaceholderTab() const override;
  std::unique_ptr<SyncedTabDelegate> ReadPlaceholderTabSnapshotIfItShouldSync(
      sync_sessions::SyncSessionsClient* sessions_client) override;

 private:
  ui::ScopedUnownedUserData<BrowserSyncedTabDelegate> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_TAB_DELEGATE_H_
