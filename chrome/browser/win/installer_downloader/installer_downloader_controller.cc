// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"
#include "chrome/browser/win/installer_downloader/system_info_provider_impl.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace installer_downloader {

namespace {

content::WebContents* GetActiveWebContents() {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (!browser->IsActive() ||
        browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
      continue;
    }

    return CHECK_DEREF(browser->GetActiveTabInterface()).GetContents();
  }

  return nullptr;
}

}  // namespace

InstallerDownloaderController::InstallerDownloaderController(
    ShowInfobarCallback show_infobar_callback)
    : show_infobar_callback_(std::move(show_infobar_callback)),
      model_(std::make_unique<InstallerDownloaderModelImpl>(
          std::make_unique<SystemInfoProviderImpl>())),
      get_active_web_contents_callback_(
          base::BindRepeating(&GetActiveWebContents)) {}

InstallerDownloaderController::InstallerDownloaderController(
    ShowInfobarCallback show_infobar_callback,
    std::unique_ptr<InstallerDownloaderModel> model)
    : show_infobar_callback_(std::move(show_infobar_callback)),
      model_(std::move(model)),
      get_active_web_contents_callback_(
          base::BindRepeating(&GetActiveWebContents)) {}

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
  if (!destination.has_value()) {
    return;
  }

  // TODO(https://crbug.com/417708652): Ensure that the infobar is visible on
  // the last activated windows.
  auto* contents = get_active_web_contents_callback_.Run();

  if (!contents) {
    return;
  }

  // Installer Downloader is a global feature, therefore it's guaranteed that
  // InstallerDownloaderController will be alive at any point during the browser
  // runtime.
  show_infobar_callback_.Run(
      contents, base::BindRepeating(
                    &InstallerDownloaderController::OnDownloadRequestAccepted,
                    base::Unretained(this)));
}

void InstallerDownloaderController::OnDownloadRequestAccepted() {
  // User have explicitly gave download consent. Therefore, a background
  // download should be issued.
  //
  // TODO(https://crbug.com/417784931): Ensure that profile is not destroyed
  // during download.
}

void InstallerDownloaderController::OnDownloadCompleted() {
  // Update local state to indicated that downloaded have been successfully
  // completed.
}

void InstallerDownloaderController::SetActiveWebContentsCallbackForTesting(
    GetActiveWebContentsCallback callback) {
  get_active_web_contents_callback_ = std::move(callback);
}

}  // namespace installer_downloader
