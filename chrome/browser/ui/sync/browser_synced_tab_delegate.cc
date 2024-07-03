// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"

#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "components/sessions/content/session_tab_helper.h"

BrowserSyncedTabDelegate::BrowserSyncedTabDelegate(
    content::WebContents* web_contents)
    : content::WebContentsUserData<BrowserSyncedTabDelegate>(*web_contents) {
  SetWebContents(web_contents);
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
  NOTREACHED_IN_MIGRATION()
      << "ReadPlaceholderTabSnapshotIfItShouldSync is not supported on "
         "desktop platforms.";
  return nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BrowserSyncedTabDelegate);
