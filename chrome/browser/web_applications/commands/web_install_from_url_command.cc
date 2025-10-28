
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_metrics.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
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
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
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
    const GURL& install_url,
    const std::optional<GURL>& manifest_id,
    base::WeakPtr<content::WebContents> web_contents,
    const GURL& last_committed_url,
    WebAppInstallDialogCallback dialog_callback,
    WebInstallFromUrlCommandCallback installed_callback)
    : WebAppCommand<SharedWebContentsLock,
                    const webapps::AppId&,
                    webapps::InstallResultCode>(
          "WebInstallFromUrlCommand",
          SharedWebContentsLockDescription(),
          std::move(installed_callback),
          /*args_for_shutdown=*/
          std::make_tuple(webapps::AppId(),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      profile_(profile),
      manifest_id_(manifest_id),
      install_url_(install_url),
      web_contents_(web_contents),
      last_committed_url_(last_committed_url),
      dialog_callback_(std::move(dialog_callback)),
      install_error_log_entry_(/*background_installation=*/false,
                               kInstallSource) {
  if (manifest_id_.has_value()) {
    GetMutableDebugValue().Set("manifest_id_param", manifest_id_->spec());
  }
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
  RecordInstallMetrics(InstallCommand::kWebAppInstallFromUrl,
                       WebAppType::kCraftedApp, code, kInstallSource);
  CompleteAndSelfDestruct(CommandResult::kFailure, webapps::AppId(), code);
}

void WebInstallFromUrlCommand::OnUrlLoadedFetchManifest(
    webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("url_loading_result", base::ToString(result));

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

  CHECK(opt_manifest->start_url.is_valid());
  CHECK(opt_manifest->id.is_valid());

  // If navigator.install was invoked with only an `install_url` (1 parameter
  // version), the manifest must have a developer-specified, or "custom", id.
  if (!manifest_id_.has_value() && !opt_manifest->has_custom_id) {
    Abort(webapps::InstallResultCode::kNoCustomManifestId);
    return;
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
      {GenerateAppIdFromManifestId(opt_manifest_->id)},
      base::BindOnce(
          &WebInstallFromUrlCommand::CreateWebAppInstallInfoFromManifest,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromUrlCommand::CreateWebAppInstallInfoFromManifest() {
  CHECK(shared_web_contents_with_app_lock_->IsGranted());

  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *opt_manifest_, *data_retriever_.get(),
          /*background_installation=*/true, kInstallSource,
          shared_web_contents_with_app_lock_->shared_web_contents()
              .GetWeakPtr(),
          [](IconUrlSizeSet& icon_url_size_set) {}, GetMutableDebugValue(),
          base::BindOnce(
              &WebInstallFromUrlCommand::OnWebAppInstallInfoCreatedShowDialog,
              weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromUrlCommand::OnWebAppInstallInfoCreatedShowDialog(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  CHECK(install_info);
  web_app_info_ = std::move(install_info);

  // If navigator.install was invoked with both `install_url` and `manifest_id`
  // (2 param version), the given `manifest_id` must match the computed id of
  // the manifest we just fetched.
  if (manifest_id_.has_value() &&
      manifest_id_ != web_app_info_->manifest_id()) {
    Abort(webapps::InstallResultCode::kManifestIdMismatch);
    return;
  }

  // TODO(crbug.com/415825168): Support detailed install dialog for background
  // installs. For now, pass `nullptr` to the screenshot_fetcher which will
  // always show the simple dialog.
  std::move(dialog_callback_)
      .Run(
          /*screenshot_fetcher=*/nullptr, web_contents_.get(),
          std::move(web_app_info_),
          base::BindOnce(&WebInstallFromUrlCommand::OnInstallDialogCompleted,
                         weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromUrlCommand::OnInstallDialogCompleted(
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (!user_accepted) {
    Abort(webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  web_app_info_ = std::move(web_app_info);

  web_app_info_->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info_->installed_by = last_committed_url_;
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
  RecordInstallMetrics(InstallCommand::kWebAppInstallFromUrl,
                       WebAppType::kCraftedApp, code, kInstallSource);

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
  CompleteAndSelfDestruct(CommandResult::kSuccess, app_id_,
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
