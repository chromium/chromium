// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"

#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"

BrowserSyncedTabDelegate::BrowserSyncedTabDelegate(
    content::WebContents* web_contents) {
  SetWebContents(web_contents);
}

BrowserSyncedTabDelegate::~BrowserSyncedTabDelegate() {}

SessionID BrowserSyncedTabDelegate::GetWindowId() const {
  return SessionTabHelper::FromWebContents(web_contents())->window_id();
}

SessionID BrowserSyncedTabDelegate::GetSessionId() const {
  return SessionTabHelper::FromWebContents(web_contents())->session_id();
}

bool BrowserSyncedTabDelegate::IsPlaceholderTab() const {
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BrowserSyncedTabDelegate)
