// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"

#include <set>

#include "base/feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sync/base/features.h"

BrowserSyncedWindowDelegate::BrowserSyncedWindowDelegate(Browser* browser)
    : browser_(browser) {
  browser->tab_strip_model()->AddObserver(this);
}

BrowserSyncedWindowDelegate::~BrowserSyncedWindowDelegate() = default;

void BrowserSyncedWindowDelegate::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    if (selection.old_contents &&
        BrowserSyncedTabDelegate::FromWebContents(selection.old_contents)) {
      BrowserSyncedTabDelegate::FromWebContents(selection.old_contents)
          ->ResetCachedLastActiveTime();
    }

    if (selection.new_contents &&
        BrowserSyncedTabDelegate::FromWebContents(selection.new_contents)) {
      BrowserSyncedTabDelegate::FromWebContents(selection.new_contents)
          ->ResetCachedLastActiveTime();
    }
  }
}

bool BrowserSyncedWindowDelegate::IsTabPinned(
    const sync_sessions::SyncedTabDelegate* tab) const {
  for (int i = 0; i < browser_->tab_strip_model()->count(); i++) {
    sync_sessions::SyncedTabDelegate* current = GetTabAt(i);
    if (tab == current) {
      return browser_->tab_strip_model()->IsTabPinned(i);
    }
  }
  // The window and tab are not always updated atomically, so it's possible
  // one of the values was stale. We'll retry later, just ignore for now.
  return false;
}

sync_sessions::SyncedTabDelegate* BrowserSyncedWindowDelegate::GetTabAt(
    int index) const {
  return BrowserSyncedTabDelegate::FromWebContents(
      browser_->tab_strip_model()->GetWebContentsAt(index));
}

SessionID BrowserSyncedWindowDelegate::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}

bool BrowserSyncedWindowDelegate::HasWindow() const {
  return browser_->window() != nullptr;
}

SessionID BrowserSyncedWindowDelegate::GetSessionId() const {
  return browser_->session_id();
}

int BrowserSyncedWindowDelegate::GetTabCount() const {
  return browser_->tab_strip_model()->count();
}

bool BrowserSyncedWindowDelegate::IsTypeNormal() const {
  return browser_->is_type_normal();
}

bool BrowserSyncedWindowDelegate::IsTypePopup() const {
  return browser_->is_type_popup();
}

bool BrowserSyncedWindowDelegate::IsSessionRestoreInProgress() const {
  return false;
}

bool BrowserSyncedWindowDelegate::ShouldSync() const {
  if (!IsTypeNormal() && !IsTypePopup()) {
    return false;
  }

  // Do not sync windows which are about to be closed.
  return !browser_->IsAttemptingToCloseBrowser();
}
