
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
}  // namespace

namespace web_app {
WebInstallFromUrlCommand::WebInstallFromUrlCommand(
    Profile& profile,
    const GURL& manifest_id,
    const GURL& install_url,
    WebInstallFromUrlCommandCallback installed_callback)
    : WebAppCommand<SharedWebContentsLock,
                    const GURL&,
                    webapps::InstallResultCode>(
          "WebInstallFromUrlCommand",
          SharedWebContentsLockDescription(),
          std::move(installed_callback),
          /*args_for_shutdown=*/
          std::make_tuple(GURL(),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      profile_(profile),
      manifest_id_(manifest_id),
      install_url_(install_url),
      install_error_log_entry_(/*background_installation=*/false,
                               kInstallSource) {
  GetMutableDebugValue().Set("manifest_id_param", manifest_id_.spec());
  GetMutableDebugValue().Set("install_url_param", install_url_.spec());
}

WebInstallFromUrlCommand::~WebInstallFromUrlCommand() = default;

void WebInstallFromUrlCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  // This metric is recorded regardless of the installation result.
  webapps::InstallableMetrics::TrackInstallEvent(kInstallSource);

  data_retriever_ =
      web_contents_lock_->web_contents_manager().CreateDataRetriever();
  CHECK(shared_web_contents() && !shared_web_contents()->IsBeingDestroyed());

  url_loader_ = web_contents_lock_->web_contents_manager().CreateUrlLoader();
  url_loader_->LoadUrl(
      install_url_, shared_web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&WebInstallFromUrlCommand::OnUrlLoadedFetchManifest,
                     weak_ptr_factory_.GetWeakPtr()));
}

content::WebContents* WebInstallFromUrlCommand::shared_web_contents() {
  CHECK(web_contents_lock_ || shared_web_contents_with_app_lock_);
  if (web_contents_lock_) {
    return &web_contents_lock_->shared_web_contents();
  }

  return &shared_web_contents_with_app_lock_->shared_web_contents();
}

void WebInstallFromUrlCommand::Abort(webapps::InstallResultCode code) {
  GetMutableDebugValue().Set("result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(/*result=*/false,
                                                  kInstallSource);
  MeasureUserInstalledAppHistogram(code);
  CompleteAndSelfDestruct(CommandResult::kFailure, GURL(), code);
}

void WebInstallFromUrlCommand::OnUrlLoadedFetchManifest(
    webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("url_loading_result",
                             ConvertUrlLoaderResultToString(result));

  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    install_error_log_entry_.LogUrlLoaderError("OnUrlLoadedFetchManifest",
                                               install_url_.spec(), result);

    webapps::InstallResultCode install_result =
        (result == webapps::WebAppUrlLoaderResult::kFailedPageTookTooLong)
            ? webapps::InstallResultCode::kInstallURLLoadTimeOut
            : webapps::InstallResultCode::kInstallURLLoadFailed;
    Abort(install_result);
    return;
  }

  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.check_eligibility = true;
  params.fetch_screenshots = true;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestWithIcons;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      shared_web_contents(),
      base::BindOnce(&WebInstallFromUrlCommand::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      params);
}

void WebInstallFromUrlCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  GetMutableDebugValue().Set("valid_manifest_for_web_app",
                             valid_manifest_for_web_app);
  GetMutableDebugValue().Set("installable_error_code",
                             base::ToString(error_code));
  // A manifest should always be returned unless an irrecoverable error occurs.
  if (!opt_manifest) {
    Abort(webapps::InstallResultCode::kNotInstallable);
    return;
  }

  if (!valid_manifest_for_web_app) {
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  web_app_info_ = std::make_unique<WebAppInstallInfo>(opt_manifest->id,
                                                      opt_manifest->start_url);
  CHECK(opt_manifest->start_url.is_valid());
  CHECK(opt_manifest->id.is_valid());
  UpdateWebAppInfoFromManifest(*opt_manifest, web_app_info_.get());
  GetMutableDebugValue().Set("manifest_id",
                             web_app_info_->manifest_id().spec());
  GetMutableDebugValue().Set("start_url", web_app_info_->start_url().spec());
  GetMutableDebugValue().Set("name", web_app_info_->title);

  if (web_app_info_->manifest_id() != manifest_id_) {
    // TODO(crbug.com/333795265): Add custom WebInstallFromUrlCommand error
    // types for additional granularity.
    Abort(webapps::InstallResultCode::kNotInstallable);
    return;
  }

  icons_from_manifest_ = GetValidIconUrlsToDownload(*web_app_info_);
  for (const IconUrlWithSize& icon_with_size : icons_from_manifest_) {
    GetMutableDebugValue()
        .EnsureList("icon_urls_from_manifest")
        ->Append(icon_with_size.ToString());
  }

  opt_manifest_ = std::move(opt_manifest);
  if (opt_manifest_->icons.empty()) {
    Abort(webapps::InstallResultCode::kNotInstallable);
    return;
  }

  CHECK(!shared_web_contents_with_app_lock_);
  shared_web_contents_with_app_lock_ =
      std::make_unique<SharedWebContentsWithAppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock_), *shared_web_contents_with_app_lock_,
      {GenerateAppIdFromManifestId(web_app_info_->manifest_id())},
      base::BindOnce(&WebInstallFromUrlCommand::GetIcons,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromUrlCommand::GetIcons() {
  CHECK(shared_web_contents_with_app_lock_->IsGranted());

  data_retriever_->GetIcons(
      shared_web_contents(), icons_from_manifest_,
      /*skip_page_favicons*/ true,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(&WebInstallFromUrlCommand::OnIconsRetrievedShowDialog,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromUrlCommand::OnIconsRetrievedShowDialog(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  base::Value::Dict* icons_downloaded =
      GetMutableDebugValue().EnsureDict("icons_retrieved");
  for (const auto& [url, bitmap_vector] : icons_map) {
    base::Value::List* sizes = icons_downloaded->EnsureList(url.spec());
    for (const SkBitmap& bitmap : bitmap_vector) {
      sizes->Append(bitmap.width());
    }
  }

  CHECK(web_app_info_);
  PopulateProductIcons(web_app_info_.get(), &icons_map);
  PopulateOtherIcons(web_app_info_.get(), icons_map);
  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  install_error_log_entry_.LogDownloadedIconsErrors(
      *web_app_info_, result, icons_map, icons_http_results);

  // TODO(crbug.com/333795265): Show install dialog.
  OnInstallDialogCompleted(/*user_accepted=*/true);
}

void WebInstallFromUrlCommand::OnInstallDialogCompleted(bool user_accepted) {
  if (!user_accepted) {
    Abort(webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  web_app_info_->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  WebAppInstallFinalizer::FinalizeOptions finalize_options(kInstallSource);
  finalize_options.install_state =
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
  finalize_options.overwrite_existing_manifest_fields = true;
  finalize_options.add_to_applications_menu = true;
  finalize_options.add_to_desktop = true;
  shared_web_contents_with_app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(&WebInstallFromUrlCommand::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromUrlCommand::OnAppInstalled(const webapps::AppId& app_id,
                                              webapps::InstallResultCode code) {
  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    Abort(code);
    return;
  }

  app_id_ = app_id;
  install_result_code_ = code;
  RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                    kInstallSource);
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code),
                                                  kInstallSource);
  MeasureUserInstalledAppHistogram(code);

  LaunchApp();
}

void WebInstallFromUrlCommand::LaunchApp() {
  apps::AppLaunchParams params = apps::AppLaunchParams(
      app_id_, apps::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::UNKNOWN, apps::LaunchSource::kFromWebInstallApi);

  shared_web_contents_with_app_lock_->ui_manager().LaunchWebApp(
      std::move(params), LaunchWebAppWindowSetting::kOverrideWithWebAppConfig,
      profile_.get(),
      base::IgnoreArgs<base::WeakPtr<Browser>,
                       base::WeakPtr<content::WebContents>,
                       apps::LaunchContainer>(
          base::BindOnce(&WebInstallFromUrlCommand::OnAppLaunched,
                         weak_ptr_factory_.GetWeakPtr())),
      *shared_web_contents_with_app_lock_);
}

void WebInstallFromUrlCommand::OnAppLaunched(base::Value launch_debug_value) {
  GetMutableDebugValue().Set("launch", std::move(launch_debug_value));

  const GURL manifest_id =
      shared_web_contents_with_app_lock_->registrar().GetComputedManifestId(
          app_id_);
  CHECK(opt_manifest_->id == manifest_id);
  CompleteAndSelfDestruct(CommandResult::kSuccess, manifest_id,
                          install_result_code_);
}

void WebInstallFromUrlCommand::MeasureUserInstalledAppHistogram(
    webapps::InstallResultCode code) {
  if (!web_app_info_) {
    return;
  }

  bool is_new_success_install = webapps::IsNewInstall(code);
  base::UmaHistogramBoolean("WebApp.NewCraftedAppInstalled.ByUser",
                            is_new_success_install);
}

}  // namespace web_app
