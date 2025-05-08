// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_IMPL_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_IMPL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"

class GURL;

namespace base {
class FilePath;
}

namespace installer_downloader {

// Non-UI service that:
//   •  Checks whether the current machine is a Win 10 device **not**
//      eligible for in-place upgrade *and* has a OneDrive-synced Desktop.
//   •  When asked, downloads the installer in the background and
//      streams progress to the controller.
//   •  Persists a token for partial downloads: TODO(crbug.com/412976021):
//   Download payload.
//
// All expensive work executes on the ThreadPool; the class itself is
// constructed and destroyed on the UI thread.
class InstallerDownloaderModelImpl final : public InstallerDownloaderModel {
 public:
  // This represents the maximum number of times that the info bar will be
  // shown.
  static constexpr int kMaxShowCount = 3;

  InstallerDownloaderModelImpl();
  InstallerDownloaderModelImpl(const InstallerDownloaderModelImpl&) = delete;
  InstallerDownloaderModelImpl& operator=(const InstallerDownloaderModelImpl&) =
      delete;

  ~InstallerDownloaderModelImpl() override;

  // InstallerDownloaderModelInterface:
  void CheckEligibility(
      base::OnceCallback<void(const std::optional<base::FilePath>&)> callback)
      override;
  void StartDownload(const GURL& url,
                     const base::FilePath& destination,
                     CompletionCallback completion_callback) override;
  bool IsMaxShowCountReached() const override;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_IMPL_H_
