// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_active_browser_window_tracker.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

namespace installer_downloader {

InstallerDownloaderActiveBrowserWindowTracker::
    InstallerDownloaderActiveBrowserWindowTracker() {
  BrowserList::GetInstance()->AddObserver(this);
  MaybeUpdateLastActiveWindow(
      GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

InstallerDownloaderActiveBrowserWindowTracker::
    ~InstallerDownloaderActiveBrowserWindowTracker() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

base::CallbackListSubscription InstallerDownloaderActiveBrowserWindowTracker::
    RegisterActiveWindowChangedCallback(WindowChangedCallback callback) {
  auto subscription = active_window_change_callbacks_.Add(callback);
  callback.Run(last_active_window_);
  return subscription;
}

base::CallbackListSubscription
InstallerDownloaderActiveBrowserWindowTracker::RegisterRemovedWindowCallback(
    WindowChangedCallback callback) {
  return window_remove_callbacks_.Add(callback);
}

void InstallerDownloaderActiveBrowserWindowTracker::OnBrowserSetLastActive(
    Browser* browser) {
  MaybeUpdateLastActiveWindow(browser);
}

void InstallerDownloaderActiveBrowserWindowTracker::OnBrowserRemoved(
    Browser* browser) {
  window_remove_callbacks_.Notify(
      static_cast<BrowserWindowInterface*>(browser));

  if (!last_active_window_ ||
      static_cast<BrowserWindowInterface*>(browser) != last_active_window_) {
    return;
  }

  MaybeUpdateLastActiveWindow(
      GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

void InstallerDownloaderActiveBrowserWindowTracker::MaybeUpdateLastActiveWindow(
    BrowserWindowInterface* bwi) {
  if (bwi && bwi->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    bwi = nullptr;
  }

  if (last_active_window_ == bwi) {
    return;
  }

  last_active_window_ = bwi;
  active_window_change_callbacks_.Notify(last_active_window_);
}

}  // namespace installer_downloader
