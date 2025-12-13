// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_IMPL_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "components/download/public/common/download_interrupt_reasons.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {
class DownloadManager;
}

namespace download {
class DownloadItem;
}

namespace installer_downloader {

class SystemInfoProvider;
class InstallerDownloaderObserver;

// Non-UI service that:
//   •  Checks whether the current machine is a Win 10 device **not**
//      eligible for in-place upgrade *and* has a OneDrive-synced Desktop.
//   •  When asked, downloads the installer in the background and
//      streams progress to the controller.
//   •  Persists a token for partial downloads.
//
// All expensive work executes on the ThreadPool; the class itself is
// constructed and destroyed on the UI thread.
class InstallerDownloaderModelImpl : public InstallerDownloaderModel {
 public:
  // This represents the maximum number of times that the info bar will be
  // shown.
  static constexpr int kMaxShowCount = 3;

  explicit InstallerDownloaderModelImpl(
      std::unique_ptr<SystemInfoProvider> system_info_provider);
  InstallerDownloaderModelImpl(const InstallerDownloaderModelImpl&) = delete;
  InstallerDownloaderModelImpl& operator=(const InstallerDownloaderModelImpl&) =
      delete;

  ~InstallerDownloaderModelImpl() override;

  // InstallerDownloaderModel:
  void CheckEligibility(EligibilityCheckCallback callback) override;
  void StartDownload(const GURL& url,
                     const base::FilePath& destination,
                     content::DownloadManager& download_manager,
                     CompletionCallback completion_callback) override;
  bool CanShowInfobar() const override;
  void IncrementShowCount() override;
  void PreventFutureDisplay() override;
  bool ShouldByPassEligibilityCheck() const override;

 private:
  std::optional<base::FilePath> GetInstallerDestination() const;

  // Invoked when the installer download started.
  void OnInstallerDownloadCreated(const base::FilePath& expected_path,
                                  CompletionCallback completion_callback,
                                  download::DownloadItem* item,
                                  download::DownloadInterruptReason reason);

  void OnInstallerDownloadFinished(CompletionCallback completion_callback,
                                   bool succeeded);

  const std::unique_ptr<SystemInfoProvider> system_info_provider_;

  // This is instantiated when a download start and is reset when the
  // download is finished or stopped. It should be null at any point when no
  // download is in progress.
  std::unique_ptr<InstallerDownloaderObserver> installer_downloader_observer_;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_IMPL_H_
