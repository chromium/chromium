// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
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
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

ExternalAppResolutionCommand::ExternalAppResolutionCommand(
    Profile& profile,
    const ExternalInstallOptions& install_options,
    std::optional<webapps::AppId> installed_placeholder_app_id,
    InstalledCallback installed_callback)
    : WebAppCommand<SharedWebContentsLock,
                    ExternallyManagedAppManager::InstallResult>(
          "ExternalAppResolutionCommand",
          SharedWebContentsLockDescription(),
          std::move(installed_callback),
          /*args_for_shutdown=*/
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::
                  kCancelledOnWebAppProviderShuttingDown,
              std::nullopt,
              /*did_uninstall_and_replace=*/false)),
      profile_(profile),
      install_options_(install_options),
      installed_placeholder_app_id_(std::move(installed_placeholder_app_id)),
      install_surface_(ConvertExternalInstallSourceToInstallSource(
          install_options_.install_source)),
      install_error_log_entry_(/*background_installation=*/true,
                               install_surface_) {
  GetMutableDebugValue().Set("external_install_options",
                             install_options_.AsDebugValue());
  GetMutableDebugValue().Set(
      "installed_placeholder_app_id",
      installed_placeholder_app_id_.has_value()
          ? base::ToString(installed_placeholder_app_id_.value())
          : "nullopt");
}

ExternalAppResolutionCommand::~ExternalAppResolutionCommand() = default;

void ExternalAppResolutionCommand::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void ExternalAppResolutionCommand::SetOnLockUpgradedCallbackForTesting(
    base::OnceClosure callback) {
  on_lock_upgraded_callback_for_testing_ = std::move(callback);
}

void ExternalAppResolutionCommand::OnShutdown(
    base::PassKey<WebAppCommandManager>) const {
  webapps::InstallableMetrics::TrackInstallResult(false, install_surface_);
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
      webapps::WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnUrlLoadedAndBranchInstallation,
          weak_ptr_factory_.GetWeakPtr()));
}

WebAppProvider& ExternalAppResolutionCommand::provider() const {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile_.get());
  CHECK(provider);
  return *provider;
}

void ExternalAppResolutionCommand::Abort(webapps::InstallResultCode code) {
  GetMutableDebugValue().Set("abort_result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(false, install_surface_);
  CompleteAndSelfDestruct(CommandResult::kFailure,
                          ExternallyManagedAppManager::InstallResult(
                              code, std::nullopt,
                              /*did_uninstall_and_replace=*/false));
}

void ExternalAppResolutionCommand::OnUrlLoadedAndBranchInstallation(
    webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("load_url_result", base::ToString(result));
  if (result == webapps::WebAppUrlLoaderResult::kUrlLoaded) {
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
      GetMutableDebugValue().Set("success", true);
      CompleteAndSelfDestruct(
          CommandResult::kSuccess,
          PrepareResult(
              /*is_offline_install=*/false,
              ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessAlreadyInstalled,
                  *installed_placeholder_app_id_)));
      return;
    }

    CHECK(!apps_lock_);
    apps_lock_ = std::make_unique<SharedWebContentsWithAppLock>();
    command_manager()->lock_manager().UpgradeAndAcquireLock(
        std::move(web_contents_lock_), *apps_lock_,
        {GenerateAppId(/*manifest_id_path=*/std::nullopt,
                       install_options_.install_url)},
        base::BindOnce(
            &ExternalAppResolutionCommand::OnPlaceHolderAppLockAcquired,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (result == webapps::WebAppUrlLoaderResult::kFailedWebContentsDestroyed) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  webapps::InstallResultCode code =
      webapps::InstallResultCode::kInstallURLLoadFailed;

  switch (result) {
    case webapps::WebAppUrlLoaderResult::kUrlLoaded:
    case webapps::WebAppUrlLoaderResult::kFailedWebContentsDestroyed:
      // Handled above.
      NOTREACHED_IN_MIGRATION();
      break;
    case webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded:
      code = webapps::InstallResultCode::kInstallURLRedirected;
      break;
    case webapps::WebAppUrlLoaderResult::kFailedUnknownReason:
      code = webapps::InstallResultCode::kInstallURLLoadFailed;
      break;
    case webapps::WebAppUrlLoaderResult::kFailedPageTookTooLong:
      code = webapps::InstallResultCode::kInstallURLLoadTimeOut;
      break;
    case webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded:
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
  // Set start_url to fallback_start_url as web_contents may have been
  // redirected. Will be overridden by manifest values if present.
  const GURL& start_url = install_params_->fallback_start_url;
  CHECK(start_url.is_valid());
  web_app_info_->SetManifestIdAndStartUrl(
      GenerateManifestIdFromStartUrlOnly(start_url), start_url);

  if (install_params_->fallback_app_name.has_value()) {
    web_app_info_->title = install_params_->fallback_app_name.value();
  }

  ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_info_);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &ExternalAppResolutionCommand::OnDidPerformInstallableCheck,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternalAppResolutionCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  CHECK(install_params_.has_value());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  if (install_params_->require_manifest && !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << web_app_info_->manifest_id().spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  // A system app should always have a manifest icon.
  if (install_surface_ == webapps::WebappInstallSource::SYSTEM_DEFAULT) {
    CHECK(opt_manifest);
    CHECK(!opt_manifest->icons.empty());
  }

  GetMutableDebugValue().Set("had_manifest", false);
  if (opt_manifest) {
    GetMutableDebugValue().Set("had_manifest", true);
    UpdateWebAppInfoFromManifest(*opt_manifest, web_app_info_.get());
  }

  if (install_params_->install_as_shortcut) {
    *web_app_info_ = WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
        web_contents_->GetLastCommittedURL(), web_contents_->GetTitle(),
        *web_app_info_);
  }

  // TODO(b/300878868): Reject installation if the manifest id provided in the
  // WebAppInstallForceList does not match the final manifest id.
  app_id_ = GenerateAppIdFromManifestId(web_app_info_->manifest_id());
  GetMutableDebugValue().Set("app_id", app_id_);

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();

  IconUrlSizeSet icon_urls = GetValidIconUrlsToDownload(*web_app_info_);

  if (!web_contents_->GetVisibleURL().EqualsIgnoringRef(
          GURL(url::kAboutBlankURL))) {
    // Navigate to about:blank. This ensure that no further
    // navigation/redirection is still running that could interrupt icon
    // fetching.
    const GURL kAboutBlankURL = GURL(url::kAboutBlankURL);
    url_loader_->LoadUrl(
        kAboutBlankURL, web_contents_.get(),
        webapps::WebAppUrlLoader::UrlComparison::kExact,
        base::BindOnce(
            &ExternalAppResolutionCommand::OnPreparedForIconRetrieving,
            weak_ptr_factory_.GetWeakPtr(), std::move(icon_urls),
            skip_page_favicons));
    return;
  }
  OnPreparedForIconRetrieving(std::move(icon_urls), skip_page_favicons,
                              webapps::WebAppUrlLoaderResult::kUrlLoaded);
}

void ExternalAppResolutionCommand::OnPreparedForIconRetrieving(
    IconUrlSizeSet icon_urls,
    bool skip_page_favicons,
    webapps::WebAppUrlLoaderResult result) {
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
  CHECK(install_params_.has_value() && !app_id_.empty());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  PopulateOtherIcons(web_app_info_.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);

  install_error_log_entry_.LogDownloadedIconsErrors(
      *web_app_info_, result, icons_map, icons_http_results);

  apps_lock_ = std::make_unique<SharedWebContentsWithAppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock_), *apps_lock_, {app_id_},
      base::BindOnce(
          &ExternalAppResolutionCommand::OnLockUpgradedFinalizeInstall,
          weak_ptr_factory_.GetWeakPtr(),
          result != IconsDownloadedResult::kCompleted));
}

void ExternalAppResolutionCommand::OnLockUpgradedFinalizeInstall(
    bool icon_download_failed) {
  CHECK(apps_lock_->IsGranted());
  CHECK(install_params_.has_value() && !app_id_.empty());
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());

  if (on_lock_upgraded_callback_for_testing_) {
    std::move(on_lock_upgraded_callback_for_testing_).Run();
  }

  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_surface_);
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

  if (apps_lock_->registrar().IsInstalled(app_id_)) {
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
}

void ExternalAppResolutionCommand::OnInstallFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  CHECK(web_contents_ && !web_contents_->IsBeingDestroyed());
  install_code_ = code;

  GetMutableDebugValue().Set("install_code", base::ToString(code));
  if (!webapps::IsSuccess(code)) {
    TryAppInfoFactoryOnFailure(
        ExternallyManagedAppManager::InstallResult(code));
    return;
  }

  CHECK_EQ(app_id, app_id_);
  RecordWebAppInstallationTimestamp(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs(),
      app_id_, install_surface_);

  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    if (install_error_log_entry_.HasErrorDict()) {
      command_manager()->LogToInstallManager(
          install_error_log_entry_.TakeErrorDict());
    }
  }

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code),
                                                  install_surface_);

  CHECK(apps_lock_);

  uninstall_and_replace_job_.emplace(
      &profile_.get(),
      *GetMutableDebugValue().EnsureDict("uninstall_and_replace_job"),
      *apps_lock_, install_options_.uninstall_and_replace, app_id_,
      base::BindOnce(&ExternalAppResolutionCommand::
                         OnUninstallAndReplaceCompletedUninstallPlaceholder,
                     weak_ptr_factory_.GetWeakPtr()));
  uninstall_and_replace_job_->Start();
}

void ExternalAppResolutionCommand::
    OnUninstallAndReplaceCompletedUninstallPlaceholder(
        bool uninstall_triggered) {
  CHECK(apps_lock_);
  uninstalled_for_replace_ = uninstall_triggered;
  GetMutableDebugValue().Set("uninstalled_for_replace", uninstall_triggered);
  uninstall_and_replace_job_ = std::nullopt;

  const bool uninstall_placeholder =
      installed_placeholder_app_id_.has_value() &&
      *installed_placeholder_app_id_ != app_id_;
  GetMutableDebugValue().Set("uninstall_placeholder", uninstall_placeholder);

  if (!uninstall_placeholder) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        PrepareResult(
            /*is_offline_install=*/false,
            ExternallyManagedAppManager::InstallResult(
                install_code_, app_id_, uninstalled_for_replace_)));
    return;
  }

  const bool is_placeholder_running =
      installed_placeholder_app_id_.has_value() &&
      (provider().ui_manager().GetNumWindowsForApp(
           *installed_placeholder_app_id_) > 0);
  GetMutableDebugValue().Set("is_placeholder_running", is_placeholder_running);

  if (is_placeholder_running) {
    provider().ui_manager().NotifyAppRelaunchState(
        *installed_placeholder_app_id_, app_id_, web_app_info_->title,
        profile_->GetWeakPtr(), AppRelaunchState::kAppClosingForRelaunch);
  }

  relaunch_app_after_placeholder_uninstall_ =
      install_options_.placeholder_resolution_behavior ==
          PlaceholderResolutionBehavior::kCloseAndRelaunch &&
      is_placeholder_running;
  GetMutableDebugValue().Set("relaunch_app_after_placeholder_uninstall",
                             relaunch_app_after_placeholder_uninstall_);

  // Note: This practice of releasing the app lock and requesting a whole new
  // lock is highly discouraged & very selectively OK for this one case.
  all_apps_lock_description_ = std::make_unique<AllAppsLockDescription>();
  all_apps_lock_ = std::make_unique<AllAppsLock>();
  command_manager()->lock_manager().AcquireLock(
      *all_apps_lock_description_, *all_apps_lock_,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnAllAppsLockGrantedRemovePlaceholder,
          weak_ptr_factory_.GetWeakPtr()),
      FROM_HERE);
  web_contents_ = nullptr;
  apps_lock_.reset();
}

void ExternalAppResolutionCommand::OnAllAppsLockGrantedRemovePlaceholder() {
  CHECK(all_apps_lock_->IsGranted());
  CHECK(installed_placeholder_app_id_);

  remove_placeholder_job_.emplace(
      webapps::WebappUninstallSource::kPlaceholderReplacement, *profile_,
      *GetMutableDebugValue().EnsureDict("remove_placeholder_job"),
      *installed_placeholder_app_id_,
      WebAppManagementTypes({ConvertExternalInstallSourceToSource(
          install_options_.install_source)}));

  remove_placeholder_job_->Start(
      *all_apps_lock_,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnPlaceholderUninstalledMaybeRelaunch,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternalAppResolutionCommand::OnPlaceholderUninstalledMaybeRelaunch(
    webapps::UninstallResultCode result) {
  remove_placeholder_job_ = std::nullopt;
  GetMutableDebugValue().Set("placeholder_uninstall_result",
                             base::ToString(result));
  if (!relaunch_app_after_placeholder_uninstall_) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        PrepareResult(/*is_offline_install=*/false,
                      ExternallyManagedAppManager::InstallResult(
                          install_code_, app_id_, uninstalled_for_replace_)));
    return;
  }

  provider().ui_manager().NotifyAppRelaunchState(
      *installed_placeholder_app_id_, app_id_, web_app_info_->title,
      profile_->GetWeakPtr(), AppRelaunchState::kAppAboutToRelaunch);
  provider().ui_manager().LaunchWebApp(
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          app_id_, *base::CommandLine::ForCurrentProcess(),
          /*current_directory=*/base::FilePath(),
          /*url_handler_launch_url=*/std::nullopt,
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{}),
      LaunchWebAppWindowSetting::kOverrideWithWebAppConfig, *profile_,
      base::BindOnce(&ExternalAppResolutionCommand::OnLaunch,
                     weak_ptr_factory_.GetWeakPtr()),
      *all_apps_lock_);
}

void ExternalAppResolutionCommand::OnLaunch(base::WeakPtr<Browser>,
                                            base::WeakPtr<content::WebContents>,
                                            apps::LaunchContainer,
                                            base::Value debug_value) {
  GetMutableDebugValue().Set("launch", std::move(debug_value));
  provider().ui_manager().NotifyAppRelaunchState(
      *installed_placeholder_app_id_, app_id_, web_app_info_->title,
      profile_->GetWeakPtr(), AppRelaunchState::kAppRelaunched);

  CompleteAndSelfDestruct(
      CommandResult::kSuccess,
      PrepareResult(/*is_offline_install=*/false,
                    ExternallyManagedAppManager::InstallResult(
                        install_code_, app_id_, uninstalled_for_replace_)));
}

void ExternalAppResolutionCommand::OnPlaceHolderAppLockAcquired() {
  CHECK(apps_lock_);
  CHECK(apps_lock_->IsGranted());
  if (on_lock_upgraded_callback_for_testing_) {
    std::move(on_lock_upgraded_callback_for_testing_).Run();
  }

  // TODO(b/300878868): Use the manifest id specified in the
  // `WebAppInstallForceList` to generate the placeholder app id. This is needed
  // to make sure an in-place installation can be done.
  install_placeholder_job_.emplace(
      &profile_.get(),
      *GetMutableDebugValue().EnsureDict("install_placeholder_job"),
      install_options_,
      base::BindOnce(&ExternalAppResolutionCommand::OnPlaceHolderInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      *apps_lock_);
  install_placeholder_job_->Start();
}

void ExternalAppResolutionCommand::OnPlaceHolderInstalled(
    webapps::InstallResultCode code,
    webapps::AppId app_id) {
  install_placeholder_job_ = std::nullopt;
  app_id_ = app_id;
  install_code_ = code;
  GetMutableDebugValue().Set("new_placeholder_app_id", app_id_);
  GetMutableDebugValue().Set("placeholder_install_code",
                             base::ToString(install_code_));

  uninstall_and_replace_job_.emplace(
      &profile_.get(),
      *GetMutableDebugValue().EnsureDict("uninstall_and_replace_job"),
      *apps_lock_, install_options_.uninstall_and_replace, app_id,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnUninstallAndReplaceCompleted,
          weak_ptr_factory_.GetWeakPtr(), /*is_offline_install=*/false));
  uninstall_and_replace_job_->Start();
}

void ExternalAppResolutionCommand::InstallFromInfo() {
  install_params_ = ConvertExternalInstallOptionsToParams(install_options_);
  CHECK(install_params_.has_value());

  // Do not fetch web_app_origin_association data over network.
  if (install_options_.only_use_app_info_factory) {
    install_params_->skip_origin_association_validation = true;
  }

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

  if (!apps_lock_) {
    apps_lock_ = std::make_unique<SharedWebContentsWithAppLock>();
    command_manager()->lock_manager().UpgradeAndAcquireLock(
        std::move(web_contents_lock_), *apps_lock_,
        {GenerateAppIdFromManifestId(web_app_info_->manifest_id())},
        base::BindOnce(
            &ExternalAppResolutionCommand::OnInstallFromInfoAppLockAcquired,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  CHECK(apps_lock_->IsGranted());
  OnInstallFromInfoAppLockAcquired();
}

void ExternalAppResolutionCommand::OnInstallFromInfoAppLockAcquired() {
  CHECK(apps_lock_);
  CHECK(apps_lock_->IsGranted());

  install_from_info_job_.emplace(
      &profile_.get(),
      *GetMutableDebugValue().EnsureDict("install_from_info_job"),
      std::move(web_app_info_),
      /*overwrite_existing_manifest_fields=*/install_params_->force_reinstall,
      install_surface_, *install_params_,
      base::BindOnce(&ExternalAppResolutionCommand::OnInstallFromInfoCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  install_from_info_job_->Start(apps_lock_.get());
}

void ExternalAppResolutionCommand::OnInstallFromInfoCompleted(
    webapps::AppId app_id,
    webapps::InstallResultCode code) {
  install_from_info_job_ = std::nullopt;
  app_id_ = app_id;
  install_code_ = code;
  GetMutableDebugValue().Set("install_from_info_app_id", app_id_);
  GetMutableDebugValue().Set("install_from_info_install_code",
                             base::ToString(install_code_));

  bool successful_install_from_info = webapps::IsSuccess(code);
  GetMutableDebugValue().Set("successful_install_from_info",
                             successful_install_from_info);
  if (!successful_install_from_info) {
    Abort(code);
    return;
  }

  webapps::InstallableMetrics::TrackInstallResult(successful_install_from_info,
                                                  install_surface_);

  uninstall_and_replace_job_.emplace(
      &profile_.get(),
      *GetMutableDebugValue().EnsureDict("uninstall_and_replace_job"),
      *apps_lock_, install_options_.uninstall_and_replace, app_id,
      base::BindOnce(
          &ExternalAppResolutionCommand::OnUninstallAndReplaceCompleted,
          weak_ptr_factory_.GetWeakPtr(), /*is_offline_install=*/true));
  uninstall_and_replace_job_->Start();
}

void ExternalAppResolutionCommand::OnUninstallAndReplaceCompleted(
    bool is_offline_install,
    bool uninstall_triggered) {
  uninstall_and_replace_job_ = std::nullopt;
  GetMutableDebugValue().Set("uninstalled_for_replace", uninstall_triggered);
  CompleteAndSelfDestruct(
      webapps::IsSuccess(install_code_) ? CommandResult::kSuccess
                                        : CommandResult::kFailure,
      PrepareResult(is_offline_install,
                    ExternallyManagedAppManager::InstallResult(
                        install_code_, app_id_, uninstall_triggered)));
}

void ExternalAppResolutionCommand::TryAppInfoFactoryOnFailure(
    ExternallyManagedAppManager::InstallResult result) {
  GetMutableDebugValue().Set("retry_app_info_factory_on_failure",
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
  GetMutableDebugValue().Set("is_offline_install", is_offline_install);
  GetMutableDebugValue().Set("result_code", base::ToString(result.code));
  if (!IsSuccess(result.code)) {
    result.app_id = std::nullopt;
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

}  // namespace web_app
