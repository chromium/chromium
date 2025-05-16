// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace installer_downloader {

class InstallerDownloaderModel;

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
class InstallerDownloaderController {
 public:
  // A callback that will be run to show the installer download infobar in
  // `web_contents`.  `on_accept` will be run if the user accepts the prompt.
  // This will show the infobar on the actual tab.
  //
  // TODO(https://crbug.com/417709084): Make the infobar global to the browser.
  using ShowInfobarCallback =
      base::RepeatingCallback<void(content::WebContents* web_contents,
                                   base::RepeatingClosure on_accept)>;

  using GetActiveWebContentsCallback =
      base::RepeatingCallback<content::WebContents*()>;

  explicit InstallerDownloaderController(
      ShowInfobarCallback show_infobar_callback);
  InstallerDownloaderController(
      ShowInfobarCallback show_infobar_callback,
      std::unique_ptr<InstallerDownloaderModel> model);

  InstallerDownloaderController(const InstallerDownloaderController&) = delete;
  InstallerDownloaderController& operator=(
      const InstallerDownloaderController&) = delete;

  ~InstallerDownloaderController();

  // Called early during the browser startup and will show the installer
  // downloader infobar if a set of conditions are met.
  void MaybeShowInfoBar();

  // Trigger when user give an explicit consent through installer download
  // infobar.
  void OnDownloadRequestAccepted();

  void SetActiveWebContentsCallbackForTesting(
      GetActiveWebContentsCallback callback);

 private:
  void OnEligibilityReady(const std::optional<base::FilePath>& destination);
  void OnDownloadCompleted();

  ShowInfobarCallback show_infobar_callback_;
  std::unique_ptr<InstallerDownloaderModel> model_;
  GetActiveWebContentsCallback get_active_web_contents_callback_;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_
