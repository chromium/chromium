// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"

#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(BrowserSyncedTabDelegate);

BrowserSyncedTabDelegate::BrowserSyncedTabDelegate(
    tabs::TabInterface& tab,
    content::WebContents* web_contents)
    : scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  SetWebContents(web_contents);
}

// static
BrowserSyncedTabDelegate* BrowserSyncedTabDelegate::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

BrowserSyncedTabDelegate::~BrowserSyncedTabDelegate() = default;

SessionID BrowserSyncedTabDelegate::GetWindowId() const {
  return sessions::SessionTabHelper::FromWebContents(web_contents())
      ->window_id();
}

SessionID BrowserSyncedTabDelegate::GetSessionId() const {
  return sessions::SessionTabHelper::FromWebContents(web_contents())
      ->session_id();
}

bool BrowserSyncedTabDelegate::IsPlaceholderTab() const {
  return false;
}

std::unique_ptr<sync_sessions::SyncedTabDelegate>
BrowserSyncedTabDelegate::ReadPlaceholderTabSnapshotIfItShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  NOTREACHED()
      << "ReadPlaceholderTabSnapshotIfItShouldSync is not supported on "
         "desktop platforms.";
}
