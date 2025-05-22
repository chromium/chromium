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
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"
#include "chrome/browser/win/installer_downloader/system_info_provider_impl.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

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

std::optional<GURL> BuildInstallerDownloadUrl(bool is_metrics_enabled) {
  std::string installer_url_template = kInstallerUrlTemplateParam.Get();

  base::ReplaceFirstSubstringAfterOffset(
      &installer_url_template, /*start_offset=*/0, "IIDGUID",
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  base::ReplaceFirstSubstringAfterOffset(&installer_url_template,
                                         /*start_offset=*/0, "STATS",
                                         is_metrics_enabled ? "1" : "0");

  GURL installer_url(installer_url_template);

  return installer_url.is_valid()
             ? std::optional<GURL>(std::move(installer_url))
             : std::nullopt;
}

}  // namespace

InstallerDownloaderController::InstallerDownloaderController(
    ShowInfobarCallback show_infobar_callback,
    base::RepeatingCallback<bool()> is_metrics_enabled_callback)
    : is_metrics_enabled_callback_(std::move(is_metrics_enabled_callback)),
      show_infobar_callback_(std::move(show_infobar_callback)),
      model_(std::make_unique<InstallerDownloaderModelImpl>(
          std::make_unique<SystemInfoProviderImpl>())),
      get_active_web_contents_callback_(
          base::BindRepeating(&GetActiveWebContents)) {}

InstallerDownloaderController::InstallerDownloaderController(
    ShowInfobarCallback show_infobar_callback,
    base::RepeatingCallback<bool()> is_metrics_enabled_callback,
    std::unique_ptr<InstallerDownloaderModel> model)
    : is_metrics_enabled_callback_(std::move(is_metrics_enabled_callback)),
      show_infobar_callback_(std::move(show_infobar_callback)),
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
      contents,
      base::BindOnce(&InstallerDownloaderController::OnDownloadRequestAccepted,
                     base::Unretained(this), destination.value()),
      base::BindOnce(&InstallerDownloaderController::OnInfoBarDismissed,
                     base::Unretained(this)));
}

void InstallerDownloaderController::OnDownloadRequestAccepted(
    const base::FilePath& destination) {
  // User have explicitly gave download consent. Therefore, a background
  // download should be issued.
  auto* contents = get_active_web_contents_callback_.Run();

  if (!contents) {
    return;
  }

  std::optional<GURL> installer_url =
      BuildInstallerDownloadUrl(is_metrics_enabled_callback_.Run());

  if (!installer_url.has_value()) {
    return;
  }

  // Keep the profile alive until the download completes.
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kDownloadInProgress);

  model_->StartDownload(
      installer_url.value(), destination,
      CHECK_DEREF(profile->GetDownloadManager()),
      base::BindOnce(&InstallerDownloaderController::OnDownloadCompleted,
                     base::Unretained(this), std::move(keep_alive)));
}

void InstallerDownloaderController::OnDownloadCompleted(
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive,
    bool success) {
  // Update local state to indicated that downloaded have been successfully
  // completed.
}

void InstallerDownloaderController::SetActiveWebContentsCallbackForTesting(
    GetActiveWebContentsCallback callback) {
  get_active_web_contents_callback_ = std::move(callback);
}

void InstallerDownloaderController::OnInfoBarDismissed() {
  // TODO(crbug.com/417709084):Dismisses all installer Downloader infobars
  // since this infobar is global.
}

}  // namespace installer_downloader
