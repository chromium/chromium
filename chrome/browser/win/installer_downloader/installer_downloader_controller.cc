// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_infobar_window_active_tab_tracker.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"
#include "chrome/browser/win/installer_downloader/system_info_provider_impl.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
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

  std::string app_locale =
      g_browser_process->GetFeatures()->application_locale_storage()->Get();
  std::string language_code = l10n_util::GetLanguage(app_locale);
  CHECK(!language_code.empty());

  base::ReplaceFirstSubstringAfterOffset(
      &installer_url_template, /*start_offset=*/0, "LANGUAGE", language_code);

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
          base::BindRepeating(&GetActiveWebContents)) {
  RegisterBrowserWindowEvents();
}

InstallerDownloaderController::InstallerDownloaderController(
    ShowInfobarCallback show_infobar_callback,
    base::RepeatingCallback<bool()> is_metrics_enabled_callback,
    std::unique_ptr<InstallerDownloaderModel> model)
    : is_metrics_enabled_callback_(std::move(is_metrics_enabled_callback)),
      show_infobar_callback_(std::move(show_infobar_callback)),
      model_(std::move(model)),
      get_active_web_contents_callback_(
          base::BindRepeating(&GetActiveWebContents)) {
  RegisterBrowserWindowEvents();
}

void InstallerDownloaderController::RegisterBrowserWindowEvents() {
  active_window_subscription_ =
      window_tracker_.RegisterActiveWindowChangedCallback(base::BindRepeating(
          &InstallerDownloaderController::OnActiveBrowserWindowChanged,
          base::Unretained(this)));

  removed_window_subscription_ =
      window_tracker_.RegisterRemovedWindowCallback(base::BindRepeating(
          &InstallerDownloaderController::OnRemovedBrowserWindow,
          base::Unretained(this)));
}

InstallerDownloaderController::~InstallerDownloaderController() = default;

void InstallerDownloaderController::OnActiveBrowserWindowChanged(
    BrowserWindowInterface* bwi) {
  // This can be null during  the startup or when the last window is closed.
  if (!bwi) {
    return;
  }

  if (bwi_and_active_tab_tracker_map_.contains(bwi)) {
    return;
  }

  bwi_and_active_tab_tracker_map_[bwi] =
      std::make_unique<InstallerDownloaderInfobarWindowActiveTabTracker>(
          bwi,
          base::BindRepeating(&InstallerDownloaderController::MaybeShowInfoBar,
                              base::Unretained(this)));
}

void InstallerDownloaderController::OnRemovedBrowserWindow(
    BrowserWindowInterface* bwi) {
  if (!bwi_and_active_tab_tracker_map_.contains(bwi)) {
    return;
  }

  bwi_and_active_tab_tracker_map_.erase(bwi);
}

void InstallerDownloaderController::MaybeShowInfoBar() {
  // The max show count of the infobar have been reached. Eligibility check is
  // no longer needed.
  if (!model_->CanShowInfobar()) {
    return;
  }

  model_->CheckEligibility(
      base::BindOnce(&InstallerDownloaderController::OnEligibilityReady,
                     base::Unretained(this)));
}

void InstallerDownloaderController::OnEligibilityReady(
    std::optional<base::FilePath> destination) {
  if (infobar_closed_) {
    return;
  }

  auto* contents = get_active_web_contents_callback_.Run();

  if (!contents) {
    return;
  }

  if (visible_infobars_web_contents_.contains(contents)) {
    return;
  }

  // Early return when we have no destination and bypass is not allowed.
  if (!destination.has_value() && !model_->ShouldByPassEligibilityCheck()) {
    return;
  }

  // Compute a fallback destination (user’s Desktop) when bypassing eligibility.
  if (!destination) {
    base::FilePath desktop_path;
    if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path)) {
      return;
    }

    destination = std::move(desktop_path);
  }

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);

  // Installer Downloader is a global feature, therefore it's guaranteed that
  // InstallerDownloaderController will be alive at any point during the browser
  // runtime.
  infobars::InfoBar* infobar = show_infobar_callback_.Run(
      infobar_manager,
      base::BindOnce(&InstallerDownloaderController::OnDownloadRequestAccepted,
                     base::Unretained(this), destination.value()),
      base::BindOnce(&InstallerDownloaderController::OnInfoBarDismissed,
                     base::Unretained(this)));

  if (!infobar) {
    return;
  }

  if (infobar_manager) {
    infobar_manager->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }

  visible_infobars_web_contents_[contents] = infobar;

  // This is the first show in this browser session.
  if (visible_infobars_web_contents_.size() == 1u) {
    model_->IncrementShowCount();
    base::UmaHistogramBoolean("Windows.InstallerDownloader.InfobarShown",
                              /*shown=*/true);
  }
}

void InstallerDownloaderController::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                     bool animate) {
  auto it = std::find_if(
      visible_infobars_web_contents_.begin(),
      visible_infobars_web_contents_.end(),
      [infobar](const auto& entry) { return entry.second == infobar; });

  if (it == visible_infobars_web_contents_.end()) {
    return;
  }

  it->second->owner()->RemoveObserver(this);
  visible_infobars_web_contents_.erase(it);

  if (!user_initiated_info_bar_close_pending_) {
    return;
  }

  for (auto [contents, infobar_instance] : visible_infobars_web_contents_) {
    infobar_instance->owner()->RemoveObserver(this);
    infobar_instance->RemoveSelf();
  }

  visible_infobars_web_contents_.clear();
  infobar_closed_ = true;
}

void InstallerDownloaderController::OnDownloadRequestAccepted(
    const base::FilePath& destination) {
  base::UmaHistogramBoolean("Windows.InstallerDownloader.RequestAccepted",
                            true);

  user_initiated_info_bar_close_pending_ = true;
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
      installer_url.value(),
      destination.AppendASCII(kDownloadedInstallerFileName.Get()),
      CHECK_DEREF(profile->GetDownloadManager()),
      base::BindOnce(&InstallerDownloaderController::OnDownloadCompleted,
                     base::Unretained(this), std::move(keep_alive)));
}

void InstallerDownloaderController::OnDownloadCompleted(
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive,
    bool success) {
  base::UmaHistogramBoolean("Windows.InstallerDownloader.DownloadSucceed",
                            success);
  model_->PreventFutureDisplay();
}

void InstallerDownloaderController::SetActiveWebContentsCallbackForTesting(
    GetActiveWebContentsCallback callback) {
  get_active_web_contents_callback_ = std::move(callback);
}

void InstallerDownloaderController::OnInfoBarDismissed() {
  base::UmaHistogramBoolean("Windows.InstallerDownloader.RequestAccepted",
                            false);
  user_initiated_info_bar_close_pending_ = true;
  model_->PreventFutureDisplay();
}

}  // namespace installer_downloader
