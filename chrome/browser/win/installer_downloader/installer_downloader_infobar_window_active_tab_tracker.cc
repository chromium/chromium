// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_infobar_window_active_tab_tracker.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace installer_downloader {

InstallerDownloaderInfobarWindowActiveTabTracker::
    InstallerDownloaderInfobarWindowActiveTabTracker(
        BrowserWindowInterface* window,
        base::RepeatingClosure show_infobar_callback)
    : window_(window),
      show_infobar_callback_(std::move(show_infobar_callback)) {
  active_tab_subscription_ =
      window_->RegisterActiveTabDidChange(base::BindRepeating(
          &InstallerDownloaderInfobarWindowActiveTabTracker::OnActiveTabChanged,
          base::Unretained(this)));
}

InstallerDownloaderInfobarWindowActiveTabTracker::
    ~InstallerDownloaderInfobarWindowActiveTabTracker() = default;

void InstallerDownloaderInfobarWindowActiveTabTracker::OnActiveTabChanged(
    BrowserWindowInterface*) {
  show_infobar_callback_.Run();
}

}  // namespace installer_downloader
