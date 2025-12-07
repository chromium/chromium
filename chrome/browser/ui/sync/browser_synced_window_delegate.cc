// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sync/base/features.h"

BrowserSyncedWindowDelegate::BrowserSyncedWindowDelegate(
    BrowserWindowInterface* browser,
    TabStripModel* tab_strip_model,
    SessionID session_id,
    BrowserWindowInterface::Type type)
    : browser_(CHECK_DEREF(browser)),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)),
      session_id_(session_id),
      type_(type) {
  // There should be a window in the browser.
  CHECK(browser->GetWindow());
  tab_strip_model_->AddObserver(this);
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
  for (int i = 0; i < tab_strip_model_->count(); ++i) {
    sync_sessions::SyncedTabDelegate* current = GetTabAt(i);
    if (tab == current) {
      return tab_strip_model_->IsTabPinned(i);
    }
  }
  // The window and tab are not always updated atomically, so it's possible
  // one of the values was stale. We'll retry later, just ignore for now.
  return false;
}

sync_sessions::SyncedTabDelegate* BrowserSyncedWindowDelegate::GetTabAt(
    int index) const {
  return BrowserSyncedTabDelegate::FromWebContents(
      tab_strip_model_->GetWebContentsAt(index));
}

SessionID BrowserSyncedWindowDelegate::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}

bool BrowserSyncedWindowDelegate::HasWindow() const {
  return true;
}

SessionID BrowserSyncedWindowDelegate::GetSessionId() const {
  return session_id_;
}

int BrowserSyncedWindowDelegate::GetTabCount() const {
  return tab_strip_model_->count();
}

bool BrowserSyncedWindowDelegate::IsTypeNormal() const {
  return type_ == BrowserWindowInterface::TYPE_NORMAL;
}

bool BrowserSyncedWindowDelegate::IsTypePopup() const {
  return type_ == BrowserWindowInterface::TYPE_POPUP;
}

bool BrowserSyncedWindowDelegate::IsSessionRestoreInProgress() const {
  return false;
}

bool BrowserSyncedWindowDelegate::ShouldSync() const {
  if (!IsTypeNormal() && !IsTypePopup()) {
    return false;
  }

  // Do not sync windows which are about to be closed.
  return !browser_->capabilities()->IsAttemptingToCloseBrowser();
}
