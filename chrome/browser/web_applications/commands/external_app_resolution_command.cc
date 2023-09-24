// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"

#include "base/barrier_closure.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/install_placeholder_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

ExternalAppResolutionCommand::ExternalAppResolutionCommand(
    Profile& profile,
    const ExternalInstallOptions& install_options,
    absl::optional<AppId> installed_placeholder_app_id,
    InstalledCallback installed_callback)
    : WebAppCommandTemplate<SharedWebContentsLock>(
          "ExternalAppResolutionCommand"),
      web_contents_lock_description_(
          std::make_unique<SharedWebContentsLockDescription>()),
      profile_(profile),
      installed_callback_(std::move(installed_callback)),
      install_options_(install_options),
      installed_placeholder_app_id_(std::move(installed_placeholder_app_id)),
      install_surface_(ConvertExternalInstallSourceToInstallSource(
          install_options_.install_source)),
      install_error_log_entry_(/*background_installation=*/true,
                               install_surface_) {
  debug_value_.Set("external_install_options", install_options_.AsDebugValue());
}

ExternalAppResolutionCommand::~ExternalAppResolutionCommand() = default;

const LockDescription& ExternalAppResolutionCommand::lock_description() const {
  if (!web_contents_lock_ || web_contents_lock_description_) {
    return *web_contents_lock_description_;
  }
  CHECK(apps_lock_description_);
  return *apps_lock_description_;
}

base::Value ExternalAppResolutionCommand::ToDebugValue() const {
  base::Value::Dict dict = debug_value_.Clone();
  dict.Set("error_log", error_log_.Clone());
  dict.Set("app_id", app_id_ ? base::Value(*app_id_) : base::Value());
  dict.Set("install_placeholder_job",
           install_placeholder_job_ ? install_placeholder_job_->ToDebugValue()
                                    : base::Value());
  dict.Set("install_from_info_job", install_from_info_job_
                                        ? install_from_info_job_->ToDebugValue()
                                        : base::Value());
  return base::Value(std::move(dict));
}

void ExternalAppResolutionCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

void ExternalAppResolutionCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);
  web_contents_ = &web_contents_lock_->shared_web_contents();
  url_loader_ = web_contents_lock_->web_contents_manager().CreateUrlLoader();
  if (!data_retriever_) {
    data_retriever_ =
        web_contents_lock_->web_contents_manager().CreateDataRetriever();
  }
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  if (install_options_.only_use_app_info_factory) {
    InstallFromInfo();
    return;
  }

  url_loader_->LoadUrl(
      install_options_.install_url, web_contents_.get(),
      WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnUrlLoadedAndBranchInstallation,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternalAppResolutionCommand::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void ExternalAppResolutionCommand::SetOnLockUpgradedCallbackForTesting(
    base::OnceClosure callback) {
  on_lock_upgraded_callback_for_testing_ = std::move(callback);
}

void ExternalAppResolutionCommand::Abort(webapps::InstallResultCode code) {
  if (!installed_callback_) {
    return;
  }

  debug_value_.Set("result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      (code ==
       webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown)
          ? CommandResult::kShutdown
          : CommandResult::kFailure,
      base::BindOnce(std::move(installed_callback_),
                     ExternallyManagedAppManager::InstallResult(
                         code, absl::nullopt,
                         /*did_uninstall_and_replace=*/false)));
}

void ExternalAppResolutionCommand::OnUrlLoadedAndBranchInstallation(
    WebAppUrlLoader::Result result) {
  if (result == WebAppUrlLoader::Result::kUrlLoaded) {
    data_retriever_->GetWebAppInstallInfo(
        web_contents_.get(),
        base::BindOnce(
            &ExternalAppResolutionCommand::OnGetWebAppInstallInfoInCommand,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If we could not load the URL and we are supposed to install a placeholder
  // as fallback, acquire with the app id derived from the install url and lock
  // this app id to see if this placeholder is already installed.
  if (install_options_.install_placeholder) {
    if (installed_placeholder_app_id_.has_value() &&
        !install_options_.force_reinstall) {
      // No need to install a placeholder app again.
      OnInstallationJobsCompleted(
          /*success=*/true,
          base::BindOnce(
              std::move(installed_callback_),
              PrepareResult(
                  /*is_offline_install=*/false,
                  ExternallyManagedAppManager::InstallResult(
                      webapps::InstallResultCode::kSuccessAlreadyInstalled,
                      *installed_placeholder_app_id_))));
      return;
    }

    apps_lock_description_ =
        command_manager()->lock_manager().UpgradeAndAcquireLock(
            std::move(web_contents_lock_),
            {GenerateAppId(/*manifest_id_path=*/absl::nullopt,
                           install_options_.install_url)},
            base::BindOnce(
                &ExternalAppResolutionCommand::OnPlaceHolderAppLockAcquired,
                weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (result == WebAppUrlLoader::Result::kFailedWebContentsDestroyed) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  webapps::InstallResultCode code =
      webapps::InstallResultCode::kInstallURLLoadFailed;

  switch (result) {
    case WebAppUrlLoader::Result::kUrlLoaded:
    case WebAppUrlLoader::Result::kFailedWebContentsDestroyed:
      // Handled above.
      NOTREACHED();
      break;
    case WebAppUrlLoader::Result::kRedirectedUrlLoaded:
      code = webapps::InstallResultCode::kInstallURLRedirected;
      break;
    case WebAppUrlLoader::Result::kFailedUnknownReason:
      code = webapps::InstallResultCode::kInstallURLLoadFailed;
      break;
    case WebAppUrlLoader::Result::kFailedPageTookTooLong:
      code = webapps::InstallResultCode::kInstallURLLoadTimeOut;
      break;
    case WebAppUrlLoader::Result::kFailedErrorPageLoaded:
      code = webapps::InstallResultCode::kInstallURLLoadFailed;
      break;
  }

  TryAppInfoFactoryOnFailure(ExternallyManagedAppManager::InstallResult(code));
}

void ExternalAppResolutionCommand::OnGetWebAppInstallInfoInCommand(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  install_params_ = ConvertExternalInstallOptionsToParams(install_options_);
  CHECK(install_params_.has_value());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  web_app_info_ = std::move(web_app_info);
  if (!web_app_info_) {
    // TODO(b/297340562): Fallback to install from info.
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  // Write values from install_params_ to web_app_info.
  bypass_service_worker_check_ = install_params_->bypass_service_worker_check;
  // Set start_url to fallback_start_url as web_contents may have been
  // redirected. Will be overridden by manifest values if present.
  CHECK(install_params_->fallback_start_url.is_valid());
  web_app_info_->start_url = install_params_->fallback_start_url;
  web_app_info_->manifest_id =
      GenerateManifestIdFromStartUrlOnly(web_app_info_->start_url);

  if (install_params_->fallback_app_name.has_value()) {
    web_app_info_->title = install_params_->fallback_app_name.value();
  }

  ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_info_);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), bypass_service_worker_check_,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnDidPerformInstallableCheck,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternalAppResolutionCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  CHECK(install_params_.has_value());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  if (install_params_->require_manifest && !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << manifest_url.spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  // A system app should always have a manifest icon.
  if (install_surface_ == webapps::WebappInstallSource::SYSTEM_DEFAULT) {
    CHECK(opt_manifest);
    CHECK(!opt_manifest->icons.empty());
  }

  debug_value_.Set("had_manifest", false);
  if (opt_manifest) {
    debug_value_.Set("had_manifest", true);
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 web_app_info_.get());
  }

  if (install_params_->install_as_shortcut) {
    *web_app_info_ = WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
        web_contents_->GetLastCommittedURL(), web_contents_->GetTitle(),
        *web_app_info_);
  }

  // TODO(b/300878868): Reject installation if the manifest id provided in the
  // WebAppInstallForceList does not match the final manifest id.
  app_id_ = GenerateAppIdFromManifestId(web_app_info_->manifest_id);

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();

  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info_);

  if (!web_contents_->GetVisibleURL().EqualsIgnoringRef(
          GURL(url::kAboutBlankURL))) {
    // Navigate to about:blank. This ensure that no further
    // navigation/redirection is still running that could interrupt icon
    // fetching.
    const GURL kAboutBlankURL = GURL(url::kAboutBlankURL);
    url_loader_->LoadUrl(
        kAboutBlankURL, web_contents_.get(),
        WebAppUrlLoader::UrlComparison::kExact,
        base::BindOnce(
            &ExternalAppResolutionCommand::OnPreparedForIconRetrieving,
            weak_ptr_factory_.GetWeakPtr(), std::move(icon_urls),
            skip_page_favicons));
    return;
  }
  OnPreparedForIconRetrieving(std::move(icon_urls), skip_page_favicons,
                              WebAppUrlLoaderResult::kUrlLoaded);
}

void ExternalAppResolutionCommand::OnPreparedForIconRetrieving(
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons,
    WebAppUrlLoaderResult result) {
  data_retriever_->GetIcons(
      web_contents_.get(), std::move(icon_urls), skip_page_favicons,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnIconsRetrievedUpgradeLockDescription,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternalAppResolutionCommand::OnIconsRetrievedUpgradeLockDescription(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  CHECK(install_params_.has_value() && app_id_.has_value());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  PopulateOtherIcons(web_app_info_.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);

  install_error_log_entry_.LogDownloadedIconsErrors(
      *web_app_info_, result, icons_map, icons_http_results);

  apps_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(web_contents_lock_), {*app_id_},
          base::BindOnce(
              &ExternalAppResolutionCommand::OnLockUpgradedFinalizeInstall,
              weak_ptr_factory_.GetWeakPtr(),
              result != IconsDownloadedResult::kCompleted));
}

void ExternalAppResolutionCommand::OnLockUpgradedFinalizeInstall(
    bool icon_download_failed,
    std::unique_ptr<SharedWebContentsWithAppLock> apps_lock) {
  apps_lock_ = std::move(apps_lock);
  CHECK(install_params_.has_value() && app_id_.has_value());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  if (on_lock_upgraded_callback_for_testing_) {
    std::move(on_lock_upgraded_callback_for_testing_).Run();
  }

  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_surface_);

  finalize_options.locally_installed = install_params_->locally_installed;
  finalize_options.overwrite_existing_manifest_fields =
      install_params_->force_reinstall;

  ApplyParamsToFinalizeOptions(*install_params_, finalize_options);

  if (install_params_->user_display_mode.has_value()) {
    web_app_info_->user_display_mode = install_params_->user_display_mode;
  }
  finalize_options.add_to_applications_menu =
      install_params_->add_to_applications_menu;
  finalize_options.add_to_desktop = install_params_->add_to_desktop;
  finalize_options.add_to_quick_launch_bar =
      install_params_->add_to_quick_launch_bar;

  if (apps_lock_->registrar().IsInstalled(*app_id_)) {
    // If an installation is triggered for the same app but with a
    // different install_url, then we overwrite the manifest fields.
    // If icon downloads fail, then we would not overwrite the icon
    // in the web_app DB.
    finalize_options.overwrite_existing_manifest_fields = true;
    finalize_options.skip_icon_writes_on_download_failure =
        icon_download_failed;
  }
  apps_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(&ExternalAppResolutionCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));

  // Check that the finalizer hasn't called OnInstallFinalizedMaybeReparentTab
  // synchronously:
  CHECK(installed_callback_);
}

void ExternalAppResolutionCommand::OnInstallFinalized(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  debug_value_.Set("result_code", base::ToString(code));
  debug_value_.Set("install_url", install_options_.install_url.spec());
  if (!webapps::IsSuccess(code)) {
    TryAppInfoFactoryOnFailure(
        ExternallyManagedAppManager::InstallResult(code));
    return;
  }

  RecordWebAppInstallationTimestamp(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs(),
      app_id, install_surface_);

  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    if (install_error_log_entry_.HasErrorDict()) {
      command_manager()->LogToInstallManager(
          install_error_log_entry_.TakeErrorDict());
    }
  }

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));

  CHECK(apps_lock_);

  uninstall_and_replace_job_.emplace(
      &profile_.get(), *apps_lock_, install_options_.uninstall_and_replace,
      app_id,
      base::BindOnce(&ExternalAppResolutionCommand::
                         OnUninstallAndReplaceCompletedUninstallPlaceholder,
                     weak_ptr_factory_.GetWeakPtr(), app_id, std::move(code)));
  uninstall_and_replace_job_->Start();
}

void ExternalAppResolutionCommand::
    OnUninstallAndReplaceCompletedUninstallPlaceholder(
        AppId app_id,
        webapps::InstallResultCode code,
        bool uninstall_triggered) {
  CHECK(apps_lock_);

  const bool uninstall_placeholder =
      installed_placeholder_app_id_.has_value() &&
      *installed_placeholder_app_id_ != app_id;

  // If the placeholder should be uninstalled, the number of callbacks will be
  // one callback for the parent placeholder + one callback for the finishing of
  // this command.
  const size_t number_of_expected_callbacks = uninstall_placeholder ? 2 : 1;

  debug_value_.Set("number_of_expected_callbacks",
                   base::ToString(number_of_expected_callbacks));

  base::RepeatingClosure barrier = base::BarrierClosure(
      number_of_expected_callbacks,
      base::BindOnce(std::move(installed_callback_),
                     PrepareResult(/*is_offline_install=*/false,
                                   ExternallyManagedAppManager::InstallResult(
                                       std::move(code), std::move(app_id),
                                       uninstall_triggered))));

  if (uninstall_placeholder) {
    auto& scheduler =
        WebAppProvider::GetForWebApps(&profile_.get())->scheduler();
    scheduler.RemoveInstallSource(
        *installed_placeholder_app_id_,
        ConvertExternalInstallSourceToSource(install_options_.install_source),
        webapps::WebappUninstallSource::kPlaceholderReplacement,
        base::IgnoreArgs<webapps::UninstallResultCode>(barrier));
  }

  OnInstallationJobsCompleted(webapps::IsSuccess(code), barrier);
}

void ExternalAppResolutionCommand::OnPlaceHolderAppLockAcquired(
    std::unique_ptr<SharedWebContentsWithAppLock> apps_lock) {
  apps_lock_ = std::move(apps_lock);
  if (on_lock_upgraded_callback_for_testing_) {
    std::move(on_lock_upgraded_callback_for_testing_).Run();
  }

  // TODO(b/300878868): Use the manifest id specified in the
  // `WebAppInstallForceList` to generate the placeholder app id. This is needed
  // to make sure an in-place installation can be done.
  install_placeholder_job_.emplace(
      &profile_.get(), install_options_,
      base::BindOnce(&ExternalAppResolutionCommand::OnPlaceHolderInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      *apps_lock_);
  install_placeholder_job_->Start();
}

void ExternalAppResolutionCommand::OnPlaceHolderInstalled(
    webapps::InstallResultCode code,
    AppId app_id) {
  uninstall_and_replace_job_.emplace(
      &profile_.get(), *apps_lock_, install_options_.uninstall_and_replace,
      app_id,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnUninstallAndReplaceCompleted,
          weak_ptr_factory_.GetWeakPtr(), /*is_offline_install=*/false, app_id,
          std::move(code)));
  uninstall_and_replace_job_->Start();
}

void ExternalAppResolutionCommand::InstallFromInfo() {
  install_params_ = ConvertExternalInstallOptionsToParams(install_options_);
  CHECK(install_params_.has_value());

  // Do not fetch web_app_origin_association data over network.
  if (install_options_.only_use_app_info_factory) {
    install_params_->skip_origin_association_validation = true;
  }

  install_params_->bypass_os_hooks = true;

  if (!install_options_.app_info_factory) {
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  web_app_info_ = install_options_.app_info_factory.Run();

  if (!web_app_info_) {
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  base::Extend(web_app_info_->additional_search_terms,
               std::move(install_params_->additional_search_terms));
  web_app_info_->install_url = install_params_->install_url;

  if (web_app_info_->manifest_id.is_empty() ||
      !web_app_info_->manifest_id.is_valid()) {
    web_app_info_->manifest_id =
        GenerateManifestIdFromStartUrlOnly(web_app_info_->start_url);
  }

  if (!apps_lock_) {
    apps_lock_description_ =
        command_manager()->lock_manager().UpgradeAndAcquireLock(
            std::move(web_contents_lock_),
            {GenerateAppIdFromManifestId(web_app_info_->manifest_id)},
            base::BindOnce(
                &ExternalAppResolutionCommand::OnInstallFromInfoAppLockAcquired,
                weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  OnInstallFromInfoAppLockAcquired(std::move(apps_lock_));
}

void ExternalAppResolutionCommand::OnInstallFromInfoAppLockAcquired(
    std::unique_ptr<SharedWebContentsWithAppLock> apps_lock) {
  apps_lock_ = std::move(apps_lock);

  install_from_info_job_.emplace(
      &profile_.get(), std::move(web_app_info_),
      /*overwrite_existing_manifest_fields=*/install_params_->force_reinstall,
      install_surface_, *install_params_,
      base::BindOnce(&ExternalAppResolutionCommand::OnInstallFromInfoCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  install_from_info_job_->Start(apps_lock_.get());
}

void ExternalAppResolutionCommand::OnInstallFromInfoCompleted(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hook_errors) {
  if (!webapps::IsSuccess(code)) {
    Abort(code);
    return;
  }

  uninstall_and_replace_job_.emplace(
      &profile_.get(), *apps_lock_, install_options_.uninstall_and_replace,
      app_id,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnUninstallAndReplaceCompleted,
          weak_ptr_factory_.GetWeakPtr(), /*is_offline_install=*/true, app_id,
          std::move(code)));
  uninstall_and_replace_job_->Start();
}

void ExternalAppResolutionCommand::OnUninstallAndReplaceCompleted(
    bool is_offline_install,
    AppId app_id,
    webapps::InstallResultCode code,
    bool uninstall_triggered) {
  OnInstallationJobsCompleted(
      webapps::IsSuccess(code),
      base::BindOnce(std::move(installed_callback_),
                     PrepareResult(is_offline_install,
                                   ExternallyManagedAppManager::InstallResult(
                                       code, app_id, uninstall_triggered))));
}

void ExternalAppResolutionCommand::TryAppInfoFactoryOnFailure(
    ExternallyManagedAppManager::InstallResult result) {
  debug_value_.Set("retry_app_info_factory_on_failure",
                   base::ToString(result.code));

  if (!webapps::IsSuccess(result.code) && install_options_.app_info_factory) {
    InstallFromInfo();
    return;
  }

  Abort(result.code);
}

ExternallyManagedAppManager::InstallResult
ExternalAppResolutionCommand::PrepareResult(
    bool is_offline_install,
    ExternallyManagedAppManager::InstallResult result) {
  debug_value_.Set("result_code", base::ToString(result.code));
  if (!IsSuccess(result.code)) {
    result.app_id = absl::nullopt;
    return result;
  }

  if (is_offline_install) {
    result.code =
        install_options_.only_use_app_info_factory
            ? webapps::InstallResultCode::kSuccessOfflineOnlyInstall
            : webapps::InstallResultCode::kSuccessOfflineFallbackInstall;
  }
  return result;
}

void ExternalAppResolutionCommand::OnInstallationJobsCompleted(
    bool success,
    base::OnceClosure result_closure) {
  debug_value_.Set("installation_jobs_complete_success",
                   base::ToString(success));
  SignalCompletionAndSelfDestruct(
      success ? CommandResult::kSuccess : CommandResult::kFailure,
      std::move(result_closure));
}

}  // namespace web_app
