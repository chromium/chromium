// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_uninstall_and_replace_job.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

ExternallyManagedInstallCommand::ExternallyManagedInstallCommand(
    Profile* profile,
    const ExternalInstallOptions& external_install_options,
    InstallAndReplaceCallback callback,
    base::WeakPtr<content::WebContents> contents,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : WebAppCommandTemplate<NoopLock>("ExternallyManagedInstallCommand"),
      profile_(profile),
      noop_lock_description_(std::make_unique<NoopLockDescription>()),
      install_params_(
          ConvertExternalInstallOptionsToParams(external_install_options)),
      install_surface_(ConvertExternalInstallSourceToInstallSource(
          external_install_options.install_source)),
      apps_to_uninstall_(external_install_options.uninstall_and_replace),
      install_callback_(std::move(callback)),
      web_contents_(contents),
      data_retriever_(std::move(data_retriever)),
      install_error_log_entry_(/*background_installation=*/true,
                               install_surface_) {
  debug_value_.Set("external_install_options",
                   external_install_options.AsDebugValue());
  if (!install_params_.locally_installed) {
    DCHECK(!install_params_.add_to_applications_menu);
    DCHECK(!install_params_.add_to_desktop);
    DCHECK(!install_params_.add_to_quick_launch_bar);
  }
  DCHECK_NE(install_surface_, webapps::WebappInstallSource::SYNC);
}

ExternallyManagedInstallCommand::~ExternallyManagedInstallCommand() = default;

const LockDescription& ExternallyManagedInstallCommand::lock_description()
    const {
  DCHECK(noop_lock_description_ || app_lock_description_);

  if (app_lock_description_)
    return *app_lock_description_;

  return *noop_lock_description_;
}

void ExternallyManagedInstallCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  noop_lock_ = std::move(lock);

  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      web_contents_.get(),
      base::BindOnce(
          &ExternallyManagedInstallCommand::OnGetWebAppInstallInfoInCommand,
          weak_factory_.GetWeakPtr()));
}

void ExternallyManagedInstallCommand::OnSyncSourceRemoved() {}

void ExternallyManagedInstallCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

base::Value ExternallyManagedInstallCommand::ToDebugValue() const {
  base::Value::Dict value(debug_value_.Clone());
  value.Set("app_id", app_id_);
  return base::Value(std::move(value));
}

void ExternallyManagedInstallCommand::Abort(webapps::InstallResultCode code) {
  if (!install_callback_)
    return;
  debug_value_.Set("result_code", base::StreamableToString(code));
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), AppId(), code,
                     /*did_uninstall_and_replace=*/false));
}

void ExternallyManagedInstallCommand::OnGetWebAppInstallInfoInCommand(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  web_app_info_ = std::move(web_app_info);
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!web_app_info_) {
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  // Write values from install_params_ to web_app_info.
  bypass_service_worker_check_ = install_params_.bypass_service_worker_check;
  // Set start_url to fallback_start_url as web_contents may have been
  // redirected. Will be overridden by manifest values if present.
  DCHECK(install_params_.fallback_start_url.is_valid());
  web_app_info_->start_url = install_params_.fallback_start_url;

  if (install_params_.fallback_app_name.has_value())
    web_app_info_->title = install_params_.fallback_app_name.value();

  ApplyParamsToWebAppInstallInfo(install_params_, *web_app_info_);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), bypass_service_worker_check_,
      base::BindOnce(
          &ExternallyManagedInstallCommand::OnDidPerformInstallableCheck,
          weak_factory_.GetWeakPtr()));
}

void ExternallyManagedInstallCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (install_params_.require_manifest && !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << manifest_url.spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  // A system app should always have a manifest icon.
  if (install_surface_ == webapps::WebappInstallSource::SYSTEM_DEFAULT) {
    DCHECK(opt_manifest);
    DCHECK(!opt_manifest->icons.empty());
  }

  debug_value_.Set("had_manifest", false);
  if (opt_manifest) {
    debug_value_.Set("had_manifest", true);
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 web_app_info_.get());
  }

  if (install_params_.install_as_shortcut) {
    *web_app_info_ = WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
        web_contents_->GetLastCommittedURL(), *web_app_info_);
  }

  app_id_ = GenerateAppId(web_app_info_->manifest_id, web_app_info_->start_url);

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();

  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info_);
  data_retriever_->GetIcons(
      web_contents_.get(), std::move(icon_urls), skip_page_favicons,
      base::BindOnce(&ExternallyManagedInstallCommand::
                         OnIconsRetrievedUpgradeLockDescription,
                     weak_factory_.GetWeakPtr()));
}

void ExternallyManagedInstallCommand::OnIconsRetrievedUpgradeLockDescription(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  PopulateOtherIcons(web_app_info_.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);

  install_error_log_entry_.LogDownloadedIconsErrors(
      *web_app_info_, result, icons_map, icons_http_results);

  if (result != IconsDownloadedResult::kCompleted) {
    icon_download_failed_ = true;
  }

  app_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(noop_lock_), {app_id_},
          base::BindOnce(
              &ExternallyManagedInstallCommand::OnLockUpgradedFinalizeInstall,
              weak_factory_.GetWeakPtr()));
}

void ExternallyManagedInstallCommand::OnLockUpgradedFinalizeInstall(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  if (on_lock_upgraded_callback_for_testing_)
    std::move(on_lock_upgraded_callback_for_testing_).Run();

  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_surface_);

  finalize_options.locally_installed = install_params_.locally_installed;
  finalize_options.overwrite_existing_manifest_fields =
      install_params_.force_reinstall;
  finalize_options.parent_app_id = install_params_.parent_app_id;

  ApplyParamsToFinalizeOptions(install_params_, finalize_options);

  if (install_params_.user_display_mode.has_value())
    web_app_info_->user_display_mode = install_params_.user_display_mode;
  finalize_options.add_to_applications_menu =
      install_params_.add_to_applications_menu;
  finalize_options.add_to_desktop = install_params_.add_to_desktop;
  finalize_options.add_to_quick_launch_bar =
      install_params_.add_to_quick_launch_bar;

  if (app_lock_->registrar().IsInstalled(app_id_)) {
    // If an installation is triggered for the same app but with a
    // different install_url, then we overwrite the manifest fields.
    // If icon downloads fail, then we would not overwrite the icon
    // in the web_app DB.
    finalize_options.overwrite_existing_manifest_fields = true;
    finalize_options.skip_icon_writes_on_download_failure =
        icon_download_failed_;
  }
  app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(&ExternallyManagedInstallCommand::OnInstallFinalized,
                     weak_factory_.GetWeakPtr()));

  // Check that the finalizer hasn't called OnInstallFinalizedMaybeReparentTab
  // synchronously:
  DCHECK(install_callback_);
}

void ExternallyManagedInstallCommand::OnInstallFinalized(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    Abort(code);
    return;
  }
  debug_value_.Set("result_code", base::StreamableToString(code));

  RecordWebAppInstallationTimestamp(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs(),
      app_id, install_surface_);

  if (install_params_.locally_installed) {
    RecordAppBanner(web_contents_.get(), web_app_info_->start_url);
  }

  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    base::Value task_error_dict = install_error_log_entry_.TakeErrorDict();
    if (!task_error_dict.DictEmpty())
      command_manager()->LogToInstallManager(std::move(task_error_dict));
  }

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));

  DCHECK(app_lock_);
  uninstall_and_replace_job_.emplace(
      profile_, app_lock_->AsWeakPtr(), apps_to_uninstall_, app_id,
      base::BindOnce(&ExternallyManagedInstallCommand::OnUninstallAndReplaced,
                     weak_factory_.GetWeakPtr(), app_id, code));
  uninstall_and_replace_job_->Start();
}

void ExternallyManagedInstallCommand::OnUninstallAndReplaced(
    const AppId& app_id,
    webapps::InstallResultCode code,
    bool did_uninstall_and_replace) {
  debug_value_.Set("did_uninstall_and_replace", did_uninstall_and_replace);
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(install_callback_), app_id, code,
                     did_uninstall_and_replace));
}

void ExternallyManagedInstallCommand::SetOnLockUpgradedCallbackForTesting(
    base::OnceClosure callback) {
  on_lock_upgraded_callback_for_testing_ = std::move(callback);
}

}  // namespace web_app
