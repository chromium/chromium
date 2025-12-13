// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
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
#include "chrome/common/channel_info.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace installer_downloader {

namespace {

constexpr const char kUrlPrefix[] =
    "https://dl.google.com/tag/s/appguid%3D%7B";
constexpr const char kUrlSuffix[] =
    "%7D%26iid%3D%7BIIDGUID%7D%26lang%3DLANGUAGE%26browser%3D4%26usagestats%"
    "3DSTATS%26appname%3DGoogle%2520Chrome%26needsadmin%3Dprefers%26ap%"
    "3Dx64-statsdef_1%26brand%3DLMFN%26installdataindex%3Dempty/update2/"
    "installers/ChromeSetup.exe";

constexpr const char kStableGuid[] = "8A69D345-D564-463C-AFF1-A69D9E530F96";
constexpr const char kBetaGuid[] = "8237E44A-0054-442C-B6B6-EA0509993955";
constexpr const char kDevGuid[] = "401C381F-E0DE-4B85-8BD8-3F3F14FBDA57";
constexpr const char kCanaryGuid[] = "4EA16AC7-FD5A-47C3-875B-DBF4A2008C20";

std::string MakeInstallerTemplate(const std::string& appGuid) {
  return base::StrCat({kUrlPrefix, appGuid, kUrlSuffix});
}

std::string GetDefaultInstallerDownloadUrlTemplate() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
      return MakeInstallerTemplate(kCanaryGuid);
    case version_info::Channel::DEV:
      return MakeInstallerTemplate(kDevGuid);
    case version_info::Channel::BETA:
      return MakeInstallerTemplate(kBetaGuid);
    default:
      return MakeInstallerTemplate(kStableGuid);
  }
  NOTREACHED();
}

std::optional<GURL> BuildInstallerDownloadUrl(bool is_metrics_enabled) {
  std::string installer_url_template = kInstallerUrlTemplateParam.Get();
  if (installer_url_template.empty()) {
    installer_url_template = GetDefaultInstallerDownloadUrlTemplate();
  }

  base::ReplaceFirstSubstringAfterOffset(
      &installer_url_template, /*start_offset=*/0, "IIDGUID",
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  base::ReplaceFirstSubstringAfterOffset(&installer_url_template,
                                         /*start_offset=*/0, "STATS",
                                         is_metrics_enabled ? "1" : "0");

  std::string_view language_code = l10n_util::GetLanguage(
      g_browser_process->GetFeatures()->application_locale_storage()->Get());
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
      get_active_web_contents_callback_(base::BindRepeating(
          &InstallerDownloaderController::GetActiveWebContents,
          base::Unretained(this))),
      should_show_infobar_for_profile_callback_(base::BindRepeating(
          &InstallerDownloaderController::ShouldShowInfobarForCurrentProfile,
          base::Unretained(this))) {
  RegisterBrowserWindowEvents();
}

InstallerDownloaderController::InstallerDownloaderController(
    ShowInfobarCallback show_infobar_callback,
    base::RepeatingCallback<bool()> is_metrics_enabled_callback,
    std::unique_ptr<InstallerDownloaderModel> model)
    : is_metrics_enabled_callback_(std::move(is_metrics_enabled_callback)),
      show_infobar_callback_(std::move(show_infobar_callback)),
      model_(std::move(model)),
      get_active_web_contents_callback_(base::BindRepeating(
          &InstallerDownloaderController::GetActiveWebContents,
          base::Unretained(this))),
      should_show_infobar_for_profile_callback_(base::BindRepeating(
          &InstallerDownloaderController::ShouldShowInfobarForCurrentProfile,
          base::Unretained(this))) {
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

content::WebContents* InstallerDownloaderController::GetActiveWebContents() {
  BrowserWindowInterface* last_active_window =
      window_tracker_.get_last_active_window();
  if (!last_active_window) {
    return nullptr;
  }

  tabs::TabInterface* active_tab = last_active_window->GetActiveTabInterface();
  if (!active_tab) {
    return nullptr;
  }

  content::WebContents* web_contents = active_tab->GetContents();
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    return nullptr;
  }
  return web_contents;
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

bool InstallerDownloaderController::ShouldShowInfobarForCurrentProfile() {
  // The infobar should not be shown on guest profiles.
  BrowserWindowInterface* last_active_window =
      window_tracker_.get_last_active_window();
  if (!last_active_window ||
      last_active_window->GetProfile()->IsGuestSession()) {
    return false;
  }

  return true;
}

void InstallerDownloaderController::MaybeShowInfoBar() {
  // The max show count of the infobar have been reached. Eligibility check is
  // no longer needed.
  if (!model_->CanShowInfobar()) {
    return;
  }

  if (!should_show_infobar_for_profile_callback_.Run()) {
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

  // Early return when we have no destination and bypass is not allowed.
  if (!destination.has_value() && !model_->ShouldByPassEligibilityCheck()) {
    return;
  }

  auto* contents = get_active_web_contents_callback_.Run();

  if (!contents) {
    return;
  }

  if (visible_infobars_web_contents_.contains(contents)) {
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
                              /*sample=*/true);
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

void InstallerDownloaderController::
    SetShouldShowInfobarForProfileCallbackForTesting(
        ShouldShowInfobarForProfileCallback callback) {
  should_show_infobar_for_profile_callback_ = std::move(callback);
}

void InstallerDownloaderController::OnInfoBarDismissed() {
  base::UmaHistogramBoolean("Windows.InstallerDownloader.RequestAccepted",
                            false);
  user_initiated_info_bar_close_pending_ = true;
  model_->PreventFutureDisplay();
}

}  // namespace installer_downloader
