// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_ACTIVE_BROWSER_WINDOW_TRACKER_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_ACTIVE_BROWSER_WINDOW_TRACKER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"

class BrowserWindowInterface;

namespace installer_downloader {

// Lightweight helper that always knows which *normal* browser window was last
// activated. Observers can register for change notifications.
// - The tracker **does not** own the window.
// - When the last window closes, `active_window()` returns `nullptr`.
class InstallerDownloaderActiveBrowserWindowTracker final
    : public BrowserListObserver {
 public:
  using WindowChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;

  InstallerDownloaderActiveBrowserWindowTracker();

  InstallerDownloaderActiveBrowserWindowTracker(
      const InstallerDownloaderActiveBrowserWindowTracker&) = delete;
  InstallerDownloaderActiveBrowserWindowTracker& operator=(
      const InstallerDownloaderActiveBrowserWindowTracker&) = delete;

  ~InstallerDownloaderActiveBrowserWindowTracker() override;

  BrowserWindowInterface* get_last_active_window() {
    return last_active_window_;
  }

  // Register for active window changes. The callback is invoked immediately
  // with the current active window, then on every subsequent change.
  base::CallbackListSubscription RegisterActiveWindowChangedCallback(
      WindowChangedCallback callback);

  // Register for window remove changes. The callback is invoked immediately
  // when a browser window is removed.
  base::CallbackListSubscription RegisterRemovedWindowCallback(
      WindowChangedCallback callback);

 private:
  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Helps to update `last_active_window_` when the active browser changes or
  // when a browser removed.
  void MaybeUpdateLastActiveWindow(BrowserWindowInterface* bwi);

  // Track the last active window. It can be null if all the browser window type
  // are not normal.
  raw_ptr<BrowserWindowInterface> last_active_window_;

  // Stores the list of callback listening for active browser window change.
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      active_window_change_callbacks_;

  // Stores the list of callback listening for browser window remove event.
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      window_remove_callbacks_;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_ACTIVE_BROWSER_WINDOW_TRACKER_H_
