// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_H_

#include <optional>

#include "base/functional/callback_forward.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {
class DownloadManager;
}

namespace installer_downloader {

using CompletionCallback = base::OnceCallback<void(bool succeeded)>;
using EligibilityCheckCallback =
    base::OnceCallback<void(std::optional<base::FilePath>)>;

class InstallerDownloaderModel {
 public:
  virtual ~InstallerDownloaderModel() = default;

  // Posts the OS / OneDrive probe to the ThreadPool and returns the installer
  // download destination file path asynchronously on the calling sequence. The
  // destination file path will be null if the user is not eligible.
  virtual void CheckEligibility(EligibilityCheckCallback callback) = 0;

  // Kicks off a **transient** download with DownloadManager. Completion is
  // reported through `completion_callback`.
  //
  // TODO(crbug.com/412976021): Download payload.
  virtual void StartDownload(const GURL& url,
                             const base::FilePath& destination,
                             content::DownloadManager& download_manager,
                             CompletionCallback completion_callback) = 0;

  // Returns true if the infobar can be displayed, false otherwise.
  virtual bool CanShowInfobar() const = 0;

  // Increments the "show" counter. Called exactly once whenever the
  // controller actually displays the infobar.
  virtual void IncrementShowCount() = 0;

  // Set a flag to prevent any future infobar display.
  virtual void PreventFutureDisplay() = 0;

  // Returns true if eligibility check should be overridden for manual testing
  // purpose.
  virtual bool ShouldByPassEligibilityCheck() const = 0;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_MODEL_H_
