// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"

namespace browser_sync {

BrowserSyncedWindowDelegatesGetter::BrowserSyncedWindowDelegatesGetter(
    Profile* profile)
    : profile_(profile) {}
BrowserSyncedWindowDelegatesGetter::~BrowserSyncedWindowDelegatesGetter() =
    default;

BrowserSyncedWindowDelegatesGetter::SyncedWindowDelegateMap
BrowserSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  SyncedWindowDelegateMap synced_window_delegates;
  // Add all the browser windows.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile_) {
      continue;
    }
    synced_window_delegates[browser->synced_window_delegate()->GetSessionId()] =
        browser->synced_window_delegate();
  }
  return synced_window_delegates;
}

const sync_sessions::SyncedWindowDelegate*
BrowserSyncedWindowDelegatesGetter::FindById(SessionID id) {
  Browser* browser = chrome::FindBrowserWithID(id);
  return (browser != nullptr) ? browser->synced_window_delegate() : nullptr;
}

}  // namespace browser_sync
