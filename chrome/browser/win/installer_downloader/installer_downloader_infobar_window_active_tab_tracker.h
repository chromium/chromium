// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_INFOBAR_WINDOW_ACTIVE_TAB_TRACKER_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_INFOBAR_WINDOW_ACTIVE_TAB_TRACKER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;

namespace installer_downloader {

// It listens for `BrowserWindowInterface::RegisterActiveTabDidChange`
// notifications and injects the banner into the currently active tab.
class InstallerDownloaderInfobarWindowActiveTabTracker {
 public:
  InstallerDownloaderInfobarWindowActiveTabTracker(
      BrowserWindowInterface* window,
      base::RepeatingClosure show_infobar_callback);

  InstallerDownloaderInfobarWindowActiveTabTracker(
      const InstallerDownloaderInfobarWindowActiveTabTracker&) = delete;
  InstallerDownloaderInfobarWindowActiveTabTracker& operator=(
      const InstallerDownloaderInfobarWindowActiveTabTracker&) = delete;

  ~InstallerDownloaderInfobarWindowActiveTabTracker();

 private:
  void OnActiveTabChanged(BrowserWindowInterface* window);

  const raw_ptr<BrowserWindowInterface> window_;
  base::RepeatingClosure show_infobar_callback_;

  base::CallbackListSubscription active_tab_subscription_;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_INFOBAR_WINDOW_ACTIVE_TAB_TRACKER_H_
