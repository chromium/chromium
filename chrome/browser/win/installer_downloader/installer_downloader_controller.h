// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_

#include <memory>
#include <optional>

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
  explicit InstallerDownloaderController(
      std::unique_ptr<InstallerDownloaderModel> model = nullptr);
  InstallerDownloaderController(const InstallerDownloaderController&) = delete;
  InstallerDownloaderController& operator=(
      const InstallerDownloaderController&) = delete;

  ~InstallerDownloaderController();

  // Called early during the browser startup and will show the installer
  // downloader infobar if a set of conditions are met.
  void MaybeShowInfoBar();

  // Trigger when user give an explicit consent through installer download
  // infobar.
  void OnDownloadRequestAccepted(content::WebContents* web_contents);

 private:
  void OnEligibilityReady(const std::optional<base::FilePath>& destination);
  void OnDownloadCompleted();

  std::unique_ptr<InstallerDownloaderModel> model_;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONTROLLER_H_
