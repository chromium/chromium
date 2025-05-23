// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_active_browser_window_tracker.h"
#include "components/infobars/core/infobar_manager.h"

class ScopedProfileKeepAlive;
class BrowserWindowInterface;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
class ContentInfoBarManager;
}  // namespace infobars

namespace installer_downloader {

class InstallerDownloaderModel;
class InstallerDownloaderActiveBrowserWindowTracker;
class InstallerDownloaderInfobarWindowActiveTabTracker;

// UI-thread coordinator for the Installer Downloader.
// The controller owns a single InstallerDownloaderModel instance and:
// •  Kicks off eligibility checks at browser startup (via Initialize).
// •  Creates / updates the InstallerDownloaderInfoBar when the model reports
//    that the user is eligible.
// •  Relays user actions (Accept / Dismiss) back to the model.
// •  Forwards download progress callbacks to the InfoBar (If needed).
//
// Only lightweight UI work happens here; blocking I/O and network transfers
// live in the model running on the ThreadPool. The browser local state is used
// tod keep track the infobar show count.
//
// The controller is instantiated a GlobalFeature.
class InstallerDownloaderController final
    : public infobars::InfoBarManager::Observer {
 public:
  // A callback that will be run to show the installer download infobar in
  // `web_contents`.  `on_accept` will be run if the user accepts the prompt.
  // This will show the infobar on the actual tab.
  //
  // TODO(https://crbug.com/417709084): Make the infobar global to the browser.
  using ShowInfobarCallback = base::RepeatingCallback<infobars::InfoBar*(
      infobars::ContentInfoBarManager*,
      base::OnceClosure on_accept,
      base::OnceClosure on_dismiss)>;

  using GetActiveWebContentsCallback =
      base::RepeatingCallback<content::WebContents*()>;

  InstallerDownloaderController(
      ShowInfobarCallback show_infobar_callback,
      base::RepeatingCallback<bool()> is_metrics_enabled_callback);
  InstallerDownloaderController(
      ShowInfobarCallback show_infobar_callback,
      base::RepeatingCallback<bool()> is_metrics_enabled_callback,
      std::unique_ptr<InstallerDownloaderModel> model);

  InstallerDownloaderController(const InstallerDownloaderController&) = delete;
  InstallerDownloaderController& operator=(
      const InstallerDownloaderController&) = delete;

  ~InstallerDownloaderController() override;

  // Called early during the browser startup and will show the installer
  // downloader infobar if a set of conditions are met.
  void MaybeShowInfoBar();

  // Trigger when user give an explicit consent through installer download
  // infobar.
  void OnDownloadRequestAccepted(const base::FilePath& destination);

  // Called when the user dismisses the installer download infobar.
  void OnInfoBarDismissed();

  void SetActiveWebContentsCallbackForTesting(
      GetActiveWebContentsCallback callback);

 private:
  using BrowserAndActiveTabTrackerMap = std::map<
      BrowserWindowInterface*,
      std::unique_ptr<InstallerDownloaderInfobarWindowActiveTabTracker>>;

  void OnEligibilityReady(std::optional<base::FilePath> destination);
  void OnDownloadCompleted(std::unique_ptr<ScopedProfileKeepAlive> keep_alive,
                           bool success);

  void RegisterBrowserWindowEvents();

  void OnActiveBrowserWindowChanged(BrowserWindowInterface* bwi);
  void OnRemovedBrowserWindow(BrowserWindowInterface* bwi);

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  base::RepeatingCallback<bool()> is_metrics_enabled_callback_;

  ShowInfobarCallback show_infobar_callback_;
  std::unique_ptr<InstallerDownloaderModel> model_;
  GetActiveWebContentsCallback get_active_web_contents_callback_;

  // Tracks the last active browser / future browser window and notify the
  // subscriber. This is important for future browser window so that we can
  // subscribe for tab change in that window.
  InstallerDownloaderActiveBrowserWindowTracker window_tracker_;

  // Stores the subscription for the active window change.
  base::CallbackListSubscription active_window_subscription_;

  // Stores the subscription for the removed window change.
  base::CallbackListSubscription removed_window_subscription_;

  // Stores all the active tab tracker across all the window.
  BrowserAndActiveTabTrackerMap bwi_and_active_tab_tracker_map_;

  // Tracks the visible infobars with te associated web contents.
  std::map<content::WebContents*, infobars::InfoBar*>
      visible_infobars_web_contents_;

  // If `true`, that mean the infobar has been closed by the user during the
  // actual browser session. As a result, the infobar should no longer be
  // visible until the next browser session.
  bool infobar_closed_ = false;

  // Accept/Dismiss callbacks are invoked before the infobar get removed. This
  // flag will indicated that the close event has been initiate by the user or
  // not. In the case where the close event has been initiated by the user, all
  // the instance of the infobar should get removed. In all the other cases,
  // only the specific infobar will get removed. Following are some of those
  // cases:
  // 1. Close tab.
  // 2. Close browser window.
  bool user_initiated_info_bar_close_pending_ = false;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_
