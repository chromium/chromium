// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_placeholder_command.h"

#include <memory>
#include <utility>

#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {
// How often we retry to download a custom icon, not counting the first attempt.
const int MAX_ICON_DOWNLOAD_RETRIES = 1;
const base::TimeDelta ICON_DOWNLOAD_RETRY_DELAY = base::Seconds(5);
}  // namespace

InstallPlaceholderCommand::InstallPlaceholderCommand(
    Profile* profile,
    const ExternalInstallOptions& install_options,
    InstallAndReplaceCallback callback,
    base::WeakPtr<content::WebContents> web_contents,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : WebAppCommandTemplate<AppLock>("InstallPlaceholderCommand"),
      profile_(profile),
      // For placeholder installs, the install_url is treated as the start_url.
      app_id_(GenerateAppIdFromManifestId(
          GenerateManifestIdFromStartUrlOnly(install_options.install_url))),
      lock_description_(std::make_unique<AppLockDescription>(app_id_)),
      install_options_(install_options),
      callback_(std::move(callback)),
      web_contents_(web_contents),
      data_retriever_(std::move(data_retriever)) {
  debug_value_.Set("external_install_options", install_options.AsDebugValue());
  debug_value_.Set("app_id", app_id_);
}

InstallPlaceholderCommand::~InstallPlaceholderCommand() = default;

void InstallPlaceholderCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  if (install_options_.override_icon_url) {
    FetchCustomIcon(install_options_.override_icon_url.value(),
                    MAX_ICON_DOWNLOAD_RETRIES);
    return;
  }

  FinalizeInstall(/*bitmaps=*/absl::nullopt);
}

const LockDescription& InstallPlaceholderCommand::lock_description() const {
  return *lock_description_;
}

base::Value InstallPlaceholderCommand::ToDebugValue() const {
  return base::Value(debug_value_.Clone());
}

void InstallPlaceholderCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

void InstallPlaceholderCommand::Abort(webapps::InstallResultCode code) {
  if (!callback_) {
    return;
  }
  debug_value_.Set("result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_), app_id_, code,
                     /*did_uninstall_and_replace=*/false));
}

void InstallPlaceholderCommand::FetchCustomIcon(const GURL& url,
                                                int retries_left) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  data_retriever_->GetIcons(
      web_contents_.get(), {url}, /*skip_page_favicons=*/true,
      base::BindOnce(&InstallPlaceholderCommand::OnCustomIconFetched,
                     weak_factory_.GetWeakPtr(), url, retries_left));
}

void InstallPlaceholderCommand::OnCustomIconFetched(
    const GURL& image_url,
    int retries_left,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  auto bitmaps_it = icons_map.find(image_url);
  if (bitmaps_it != icons_map.end() && !bitmaps_it->second.empty()) {
    // Download succeeded.
    debug_value_.Set("custom_icon_download_success", true);
    FinalizeInstall(bitmaps_it->second);
    return;
  }
  if (retries_left <= 0) {
    // Download failed.
    debug_value_.Set("custom_icon_download_success", false);
    FinalizeInstall(absl::nullopt);
    return;
  }
  // Retry download.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InstallPlaceholderCommand::FetchCustomIcon,
                     weak_factory_.GetWeakPtr(), image_url, retries_left - 1),
      ICON_DOWNLOAD_RETRY_DELAY);
}

void InstallPlaceholderCommand::FinalizeInstall(
    absl::optional<std::reference_wrapper<const std::vector<SkBitmap>>>
        bitmaps) {
  // For placeholder installs, the install_url is treated as the start_url.
  WebAppInstallInfo web_app_info(
      GenerateManifestIdFromStartUrlOnly(install_options_.install_url));
  web_app_info.title =
      install_options_.override_name
          ? base::UTF8ToUTF16(install_options_.override_name.value())
      : install_options_.fallback_app_name
          ? base::UTF8ToUTF16(install_options_.fallback_app_name.value())
          : base::UTF8ToUTF16(install_options_.install_url.spec());

  if (bitmaps) {
    IconsMap icons_map;
    icons_map.emplace(GURL(install_options_.override_icon_url.value()),
                      bitmaps.value());
    PopulateProductIcons(&web_app_info, &icons_map);
  }

  web_app_info.start_url = install_options_.install_url;
  web_app_info.install_url = install_options_.install_url;

  web_app_info.user_display_mode = install_options_.user_display_mode;

  WebAppInstallFinalizer::FinalizeOptions options(
      ConvertExternalInstallSourceToInstallSource(
          install_options_.install_source));
  // Overwrite fields if we are doing a forced reinstall, because some
  // values (custom name or icon) might have changed.
  options.overwrite_existing_manifest_fields = install_options_.force_reinstall;

  options.add_to_applications_menu = install_options_.add_to_applications_menu;
  options.add_to_desktop = install_options_.add_to_desktop;
  options.add_to_quick_launch_bar = install_options_.add_to_quick_launch_bar;

  web_app_info.is_placeholder = true;

  lock_->install_finalizer().FinalizeInstall(
      web_app_info, options,
      base::BindOnce(&InstallPlaceholderCommand::OnInstallFinalized,
                     weak_factory_.GetWeakPtr()));
}

void InstallPlaceholderCommand::OnInstallFinalized(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  debug_value_.Set("result_code", base::ToString(code));

  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    Abort(code);
    return;
  }

  RecordWebAppInstallationTimestamp(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs(),
      app_id,
      ConvertExternalInstallSourceToInstallSource(
          install_options_.install_source));

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));

  DCHECK(lock_);
  uninstall_and_replace_job_.emplace(
      profile_, lock_->AsWeakPtr(), install_options_.uninstall_and_replace,
      app_id,
      base::BindOnce(&InstallPlaceholderCommand::OnUninstallAndReplaced,
                     weak_factory_.GetWeakPtr(), app_id, std::move(code)));
  uninstall_and_replace_job_->Start();
}

void InstallPlaceholderCommand::OnUninstallAndReplaced(
    const AppId& app_id,
    webapps::InstallResultCode code,
    bool did_uninstall_and_replace) {
  debug_value_.Set("did_uninstall_and_replace", did_uninstall_and_replace);
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(callback_), app_id, std::move(code),
                     did_uninstall_and_replace));
}

}  // namespace web_app
