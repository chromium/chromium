// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
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
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile_) {
          return true;  // continue iterating
        }
        auto* const delegate = browser->GetFeatures().synced_window_delegate();
        synced_window_delegates[delegate->GetSessionId()] = delegate;
        return true;  // continue iterating
      });
  return synced_window_delegates;
}

const sync_sessions::SyncedWindowDelegate*
BrowserSyncedWindowDelegatesGetter::FindById(SessionID id) {
  auto* browser = BrowserWindowInterface::FromSessionID(id);
  return browser ? browser->GetFeatures().synced_window_delegate() : nullptr;
}

}  // namespace browser_sync
