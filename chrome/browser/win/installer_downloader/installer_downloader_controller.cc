// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

namespace installer_downloader {

InstallerDownloaderController::InstallerDownloaderController(
    std::unique_ptr<InstallerDownloaderModel> model)
    : model_(model ? std::move(model)
                   : std::make_unique<InstallerDownloaderModelImpl>()) {
  CHECK(base::FeatureList::IsEnabled(kInstallerDownloader));
}

InstallerDownloaderController::~InstallerDownloaderController() = default;

void InstallerDownloaderController::MaybeShowInfoBar() {
  // At this point, the decision to show the infobar should be taken.
  // 1. Check local state whether shown limit has been reached or not.
  // 2. Check eligibility.

  // The max show count of the infobar have been reached. Eligibility check is
  // no longer needed.
  if (model_->IsMaxShowCountReached()) {
    return;
  }

  model_->CheckEligibility(
      base::BindOnce(&InstallerDownloaderController::OnEligibilityReady,
                     base::Unretained(this)));
}

void InstallerDownloaderController::OnEligibilityReady(
    const std::optional<base::FilePath>& destination) {
  // At this point, eligibility is the last item to take the decision to show
  // the infobar. Trigger infobar display based on `result`.
}

void InstallerDownloaderController::OnDownloadRequestAccepted(
    content::WebContents* /*web_contents*/) {
  // User have explicitly gave download consent. Therefore, a background
  // download should be issued.
}

void InstallerDownloaderController::OnDownloadCompleted() {
  // Update local state to indicated that downloaded have been successfully
  // completed.
}

}  // namespace installer_downloader
