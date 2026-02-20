// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/install_placeholder_job.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

namespace {
// How often we retry to download a custom icon, not counting the first attempt.
const int MAX_ICON_DOWNLOAD_RETRIES = 1;
const base::TimeDelta ICON_DOWNLOAD_RETRY_DELAY = base::Seconds(5);
}  // namespace

InstallPlaceholderJob::InstallPlaceholderJob(
    Profile* profile,
    base::DictValue& debug_value,
    const ExternalInstallOptions& install_options,
    InstallAndReplaceCallback callback,
    SharedWebContentsWithAppLock& lock)
    : profile_(*profile),
      debug_value_(debug_value),
      // For placeholder installs, the install_url is treated as the start_url.
      app_id_(GenerateAppId(/*manifest_id_path=*/std::nullopt,
                            install_options.install_url)),
      lock_(lock),
      install_options_(install_options),
      callback_(std::move(callback)),
      web_contents_(&lock_->shared_web_contents()),
      url_loader_(WebAppProvider::GetForWebApps(&profile_.get())
                      ->web_contents_manager()
                      .CreateUrlLoader()),
      data_retriever_(WebAppProvider::GetForWebApps(&profile_.get())
                          ->web_contents_manager()
                          .CreateDataRetriever()) {
  debug_value_->Set("external_install_options", install_options.AsDebugValue());
  debug_value_->Set("app_id", app_id_);
}

InstallPlaceholderJob::~InstallPlaceholderJob() = default;

void InstallPlaceholderJob::Start() {
  url_loader_->LoadUrl(install_options_.install_url, web_contents_,
                       webapps::WebAppUrlLoader::UrlComparison::kSameOrigin,
                       base::BindOnce(&InstallPlaceholderJob::OnUrlLoaded,
                                      weak_factory_.GetWeakPtr()));
}

void InstallPlaceholderJob::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void InstallPlaceholderJob::SetUrlLoaderForTesting(
    std::unique_ptr<webapps::WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void InstallPlaceholderJob::Abort(webapps::InstallResultCode code) {
  debug_value_->Set("result_code", base::ToString(code));
  if (!callback_) {
    return;
  }
  webapps::InstallableMetrics::TrackInstallResult(
      false, ConvertExternalInstallSourceToInstallSource(
                 install_options_.install_source));
  std::move(callback_).Run(code, std::move(app_id_));
}

void InstallPlaceholderJob::OnUrlLoaded(
    webapps::WebAppUrlLoaderResult load_url_result) {
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  if (install_options_.override_icon_url) {
    FetchCustomIcon(install_options_.override_icon_url.value(),
                    MAX_ICON_DOWNLOAD_RETRIES);
    return;
  }

  FinalizeInstall(/*bitmaps=*/std::nullopt);
}

void InstallPlaceholderJob::FetchCustomIcon(const GURL& url, int retries_left) {
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());
  // Navigate the web contents to the icon origin such that the content security
  // policy of the web app page can't prevent a cross-origin download.
  url_loader_->LoadUrl(
      url, web_contents_, webapps::WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(&InstallPlaceholderJob::OnIconNavigationCompleted,
                     weak_factory_.GetWeakPtr(), url, retries_left));
}

void InstallPlaceholderJob::OnIconNavigationCompleted(
    const GURL& url,
    int retries_left,
    webapps::WebAppUrlLoaderResult load_url_result) {
  if (load_url_result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    MaybeRetryFetchCustomIcon(url, retries_left);
    return;
  }

  data_retriever_->GetIcons(
      web_contents_.get(), {IconUrlWithSize::CreateForUnspecifiedSize(url)},
      /*download_page_favicons=*/false,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(&InstallPlaceholderJob::OnCustomIconFetched,
                     weak_factory_.GetWeakPtr(), url, retries_left));
}

void InstallPlaceholderJob::MaybeRetryFetchCustomIcon(const GURL& url,
                                                      int retries_left) {
  if (retries_left <= 0) {
    // Download failed.
    debug_value_->Set("custom_icon_download_success", false);
    FinalizeInstall(std::nullopt);
    return;
  }

  // Retry download.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InstallPlaceholderJob::FetchCustomIcon,
                     weak_factory_.GetWeakPtr(), url, retries_left - 1),
      ICON_DOWNLOAD_RETRY_DELAY);
}

void InstallPlaceholderJob::OnCustomIconFetched(
    const GURL& image_url,
    int retries_left,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  auto bitmaps_it = icons_map.find(image_url);
  if (bitmaps_it != icons_map.end() && !bitmaps_it->second.empty()) {
    // Download succeeded.
    debug_value_->Set("custom_icon_download_success", true);
    FinalizeInstall(bitmaps_it->second);
    return;
  }

  MaybeRetryFetchCustomIcon(image_url, retries_left);
}

void InstallPlaceholderJob::FinalizeInstall(
    std::optional<std::reference_wrapper<const std::vector<SkBitmap>>>
        bitmaps) {
  // For placeholder installs, the install_url is treated as the start_url.
  GURL start_url = install_options_.install_url;
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  auto web_app_info =
      std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
  web_app_info->title =
      install_options_.override_name
          ? base::UTF8ToUTF16(install_options_.override_name.value())
      : install_options_.fallback_app_name
          ? base::UTF8ToUTF16(install_options_.fallback_app_name.value())
          : base::UTF8ToUTF16(install_options_.install_url.spec());

  if (bitmaps) {
    IconsMap icons_map;
    icons_map.emplace(GURL(install_options_.override_icon_url.value()),
                      bitmaps.value());
    PopulateProductIcons(web_app_info.get(), &icons_map);

    // For icon overrides, the app icon needs to be used as the trusted icon,
    // since it's guaranteed that the icon is coming from a trusted source, as
    // it is externally installed.
    apps::IconInfo trusted_bitmap;
    trusted_bitmap.url = GURL(install_options_.override_icon_url.value());
    web_app_info->trusted_icons = {trusted_bitmap};
    PopulateTrustedIconBitmaps(*web_app_info.get(), icons_map);
  }

  web_app_info->install_url = install_options_.install_url;

  web_app_info->user_display_mode = install_options_.user_display_mode;

  FinalizeJobOptions options(ConvertExternalInstallSourceToInstallSource(
      install_options_.install_source));
  // Overwrite fields if we are doing a forced reinstall, because some
  // values (custom name or icon) might have changed.
  options.overwrite_existing_manifest_fields = install_options_.force_reinstall;

  options.add_to_applications_menu = install_options_.add_to_applications_menu;
  options.add_to_desktop = install_options_.add_to_desktop;
  options.add_to_quick_launch_bar = install_options_.add_to_quick_launch_bar;

  web_app_info->is_placeholder = true;

  install_job_ = std::make_unique<FinalizeInstallJob>(
      profile_.get(), &lock_.get(), &lock_.get(), *web_app_info, options);

  install_job_->Start(base::BindOnce(&InstallPlaceholderJob::OnInstallFinalized,
                                     weak_factory_.GetWeakPtr()));
}

void InstallPlaceholderJob::OnInstallFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  install_job_.reset();
  debug_value_->Set("result_code", base::ToString(code));

  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

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

  webapps::InstallableMetrics::TrackInstallResult(
      webapps::IsSuccess(code), ConvertExternalInstallSourceToInstallSource(
                                    install_options_.install_source));

  std::move(callback_).Run(code, std::move(app_id));
}

}  // namespace web_app
