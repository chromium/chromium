// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/sub_app_install_command.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom-shared.h"

namespace web_app {

namespace {

blink::mojom::SubAppsServiceResultCode InstallResultCodeToMojo(
    webapps::InstallResultCode install_result_code) {
  switch (install_result_code) {
    // Success result codes.
    case webapps::InstallResultCode::kSuccessNewInstall:
    case webapps::InstallResultCode::kSuccessAlreadyInstalled:
      return blink::mojom::SubAppsServiceResultCode::kSuccess;
    // Failure result codes.
    case webapps::InstallResultCode::kUserInstallDeclined:
    case webapps::InstallResultCode::kExpectedAppIdCheckFailed:
    case webapps::InstallResultCode::kInstallURLRedirected:
    case webapps::InstallResultCode::kInstallURLLoadTimeOut:
    case webapps::InstallResultCode::kInstallURLLoadFailed:
    case webapps::InstallResultCode::kNotValidManifestForWebApp:
      return blink::mojom::SubAppsServiceResultCode::kFailure;
    default:
      return blink::mojom::SubAppsServiceResultCode::kFailure;
  }
}

WebAppInstallFinalizer::FinalizeOptions GetFinalizerOptionsForSubApps(
    const AppId& parent_app_id) {
  WebAppInstallFinalizer::FinalizeOptions finalize_options(
      webapps::WebappInstallSource::SUB_APP);

  finalize_options.locally_installed = true;
  finalize_options.overwrite_existing_manifest_fields = false;
  if (IsChromeOsDataMandatory()) {
    // Default values for ChromeOS installation.
    finalize_options.chromeos_data.emplace();
    finalize_options.chromeos_data->show_in_launcher = true;
    finalize_options.chromeos_data->show_in_search = true;
    finalize_options.chromeos_data->show_in_management = true;
    finalize_options.chromeos_data->is_disabled = false;
    finalize_options.chromeos_data->oem_installed = false;
    finalize_options.chromeos_data->handles_file_open_intents = true;
  }
  finalize_options.bypass_os_hooks = false;
  finalize_options.add_to_applications_menu = true;
  finalize_options.add_to_desktop = true;
  finalize_options.add_to_quick_launch_bar = false;

  return finalize_options;
}

std::vector<AppId> CreateAppIdsForLock(
    const AppId& parent_app_id,
    const std::vector<std::pair<UnhashedAppId, GURL>>& sub_apps) {
  std::vector<AppId> app_ids_vector = {parent_app_id};
  for (const auto& data : sub_apps) {
    app_ids_vector.push_back(GenerateAppIdFromUnhashed(data.first));
  }
  return app_ids_vector;
}

}  // namespace

SubAppInstallCommand::SubAppInstallCommand(
    const AppId& parent_app_id,
    std::vector<std::pair<UnhashedAppId, GURL>> sub_apps,
    SubAppInstallResultCallback install_callback,
    Profile* profile,
    std::unique_ptr<WebAppUrlLoader> url_loader,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : WebAppCommandTemplate<SharedWebContentsWithAppLock>(
          "SubAppInstallCommand"),
      lock_description_(
          std::make_unique<SharedWebContentsWithAppLockDescription>(
              CreateAppIdsForLock(parent_app_id, sub_apps))),
      parent_app_id_{parent_app_id},
      requested_installs_{std::move(sub_apps)},
      install_callback_{std::move(install_callback)},
      profile_(profile),
      url_loader_(std::move(url_loader)),
      data_retriever_{std::move(data_retriever)},
      log_entry_(/*background_installation=*/false,
                 webapps::WebappInstallSource::SUB_APP) {}

SubAppInstallCommand::~SubAppInstallCommand() = default;

const LockDescription& SubAppInstallCommand::lock_description() const {
  return *lock_description_;
}

base::Value SubAppInstallCommand::ToDebugValue() const {
  base::Value::Dict install_info;
  install_info.Set("parent_app_id", parent_app_id_);
  base::Value::List pending_installs;
  for (const auto& installs_remaining : requested_installs_) {
    base::Value::Dict install_data;
    install_data.Set("unhashed_app_id", installs_remaining.first);
    install_data.Set("install_url", installs_remaining.second.spec());
    pending_installs.Append(base::Value(std::move(install_data)));
  }
  install_info.Set("pending_installs",
                   base::Value(std::move(pending_installs)));
  install_info.Set("completed_install_results", debug_install_results_.Clone());
  return base::Value(std::move(install_info));
}

void SubAppInstallCommand::SetDialogNotAcceptedForTesting() {
  dialog_not_accepted_for_testing_ = true;
}

void SubAppInstallCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsWithAppLock> lock) {
  lock_ = std::move(lock);

  DCHECK(state_ == State::kNotStarted);

  // Abort if parent app is not installed or the calling app is itself a sub
  // app.
  if (!lock_->registrar().IsInstalled(parent_app_id_) ||
      lock_->registrar().GetAppById(parent_app_id_)->IsSubAppInstalledApp()) {
    base::ranges::transform(
        requested_installs_, std::inserter(results_, results_.begin()),
        [](auto const& pair) {
          return std::pair{pair.first,
                           blink::mojom::SubAppsServiceResultCode::kFailure};
        });
    SignalCompletionAndSelfDestruct(
        CommandResult::kFailure,
        base::BindOnce(std::move(install_callback_), results_));
    return;
  }

  if (requested_installs_.empty()) {
    SignalCompletionAndSelfDestruct(
        CommandResult::kSuccess,
        base::BindOnce(std::move(install_callback_), results_));
    return;
  }

  for (const auto& install_data : requested_installs_) {
    // Ensuring that duplicate app_ids are not passed in as part of
    // requested_installs_.
    DCHECK(!base::Contains(pending_installs_map_, install_data.first));
    pending_installs_map_[install_data.first] = install_data.second;
  }
  num_pending_dialog_callbacks_ = pending_installs_map_.size();

  state_ = State::kPendingDialogCallbacks;
  StartNextInstall();
}

void SubAppInstallCommand::OnShutdown() {
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), results_));
  return;
}

void SubAppInstallCommand::StartNextInstall() {
  DCHECK(!requested_installs_.empty());
  std::pair<UnhashedAppId, GURL> install_info =
      std::move(requested_installs_.back());
  const UnhashedAppId& unhashed_app_id = install_info.first;
  GURL install_url = install_info.second;
  requested_installs_.pop_back();

  DCHECK(AreWebAppsUserInstallable(profile_));
  if (IsWebContentsDestroyed()) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  url_loader_->LoadUrl(
      install_url, &lock_->shared_web_contents(),
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(
          &SubAppInstallCommand::OnWebAppUrlLoadedGetWebAppInstallInfo,
          weak_ptr_factory_.GetWeakPtr(), unhashed_app_id, install_url));
}

void SubAppInstallCommand::OnWebAppUrlLoadedGetWebAppInstallInfo(
    const UnhashedAppId& unhashed_app_id,
    const GURL& url_to_load,
    WebAppUrlLoader::Result result) {
  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    log_entry_.LogUrlLoaderError("OnWebAppUrlLoaded", url_to_load.spec(),
                                 result);
  }

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      &lock_->shared_web_contents(),
      base::BindOnce(&SubAppInstallCommand::OnGetWebAppInstallInfo,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id));
}

void SubAppInstallCommand::OnGetWebAppInstallInfo(
    const UnhashedAppId& unhashed_app_id,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }
  install_info->parent_app_id = parent_app_id_;

  DCHECK(base::Contains(pending_installs_map_, unhashed_app_id));
  const GURL& install_url = pending_installs_map_[unhashed_app_id];
  // Set start_url to fallback_start_url as web_contents may have been
  // redirected. Will be overridden by manifest values if present.
  if (install_url.is_valid()) {
    install_info->start_url = install_url;
    install_info->install_url = install_url;
  }
  install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &lock_->shared_web_contents(), /*bypass_service_worker_check=*/false,
      base::BindOnce(&SubAppInstallCommand::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id,
                     std::move(install_info)));
}

void SubAppInstallCommand::OnDidPerformInstallableCheck(
    const UnhashedAppId& unhashed_app_id,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  DCHECK(web_app_info);
  if (!valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << web_app_info->start_url.spec()
                 << " because it didn't have a manifest for web app";
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  if (opt_manifest) {
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 web_app_info.get());
  }

  AppId app_id =
      GenerateAppId(web_app_info->manifest_id, web_app_info->start_url);

  const AppId expected_app_id = GenerateAppIdFromUnhashed(unhashed_app_id);
  if (app_id != expected_app_id) {
    log_entry_.LogExpectedAppIdError("OnDidPerformInstallableCheck",
                                     web_app_info->start_url.spec(), app_id,
                                     expected_app_id);
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  if (lock_->registrar().WasInstalledBySubApp(app_id)) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();
  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info);

  data_retriever_->GetIcons(
      &lock_->shared_web_contents(), std::move(icon_urls), skip_page_favicons,
      base::BindOnce(&SubAppInstallCommand::OnIconsRetrievedShowDialog,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id,
                     std::move(web_app_info)));
}

void SubAppInstallCommand::OnIconsRetrievedShowDialog(
    const UnhashedAppId& unhashed_app_id,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK(web_app_info);
  PopulateProductIcons(web_app_info.get(), &icons_map);
  PopulateOtherIcons(web_app_info.get(), icons_map);
  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  log_entry_.LogDownloadedIconsErrors(*web_app_info, result, icons_map,
                                      icons_http_results);

  acceptance_callbacks_.emplace_back(
      unhashed_app_id, std::move(web_app_info),
      base::BindOnce(&SubAppInstallCommand::OnDialogCompleted,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id));
  num_pending_dialog_callbacks_--;
  DCHECK_GE(num_pending_dialog_callbacks_, 0u);
  MaybeShowDialog();
}

void SubAppInstallCommand::OnDialogCompleted(
    const UnhashedAppId& unhashed_app_id,
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (!user_accepted) {
    MaybeFinishInstall(unhashed_app_id,
                       webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

  lock_->install_finalizer().FinalizeInstall(
      *web_app_info, GetFinalizerOptionsForSubApps(parent_app_id_),
      base::BindOnce(&SubAppInstallCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id,
                     web_app_info->start_url));
}

void SubAppInstallCommand::OnInstallFinalized(
    const UnhashedAppId& unhashed_app_id,
    const GURL& start_url,
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    MaybeFinishInstall(unhashed_app_id, code);
    return;
  }

  RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                    webapps::WebappInstallSource::SUB_APP);
  RecordAppBanner(&lock_->shared_web_contents(), start_url);
  MaybeFinishInstall(unhashed_app_id,
                     webapps::InstallResultCode::kSuccessNewInstall);
}

void SubAppInstallCommand::MaybeFinishInstall(
    const UnhashedAppId& unhashed_app_id,
    webapps::InstallResultCode code) {
  // Verifying that other asynchronous calls have not already installed this
  // app and thus removed it from the pending installs map.
  DCHECK(base::Contains(pending_installs_map_, unhashed_app_id));
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));
  AddResultAndRemoveFromPendingInstalls(unhashed_app_id, code);
  // In case an installation returns with a failure before running the dialog
  // callback.
  if (state_ == State::kPendingDialogCallbacks &&
      num_pending_dialog_callbacks_ > 0) {
    num_pending_dialog_callbacks_--;
    MaybeShowDialog();
    return;
  }

  DCHECK_GE(pending_installs_map_.size(), 0u);
  MaybeFinishCommand();
}

void SubAppInstallCommand::MaybeShowDialog() {
  if (num_pending_dialog_callbacks_ > 0) {
    DCHECK(!requested_installs_.empty());
    StartNextInstall();
    return;
  }

  if (acceptance_callbacks_.empty()) {
    SignalCompletionAndSelfDestruct(
        CommandResult::kFailure,
        base::BindOnce(std::move(install_callback_), results_));
    return;
  }

  // TODO(https://crbug.com/1313109): Replace the placeholder blanket user
  // acceptance below with a permissions dialog shown to the user.
  for (auto& [unhashed_app_id, web_app_info, acceptance_callback] :
       acceptance_callbacks_) {
    if (dialog_not_accepted_for_testing_) {
      std::move(acceptance_callback).Run(false, std::move(web_app_info));
    } else {
      std::move(acceptance_callback).Run(true, std::move(web_app_info));
    }
  }
  acceptance_callbacks_.clear();
  // This needs to happen to measure the state where all acceptance
  // callbacks have been run, to prevent reentrant issues into the loop
  // above after the command has been destroyed.
  state_ = State::kCallbacksComplete;
  MaybeFinishCommand();
}

void SubAppInstallCommand::MaybeFinishCommand() {
  if (pending_installs_map_.size() == 0 &&
      state_ == State::kCallbacksComplete) {
    SignalCompletionAndSelfDestruct(
        CommandResult::kSuccess,
        base::BindOnce(std::move(install_callback_), results_));
  }
  return;
}

void SubAppInstallCommand::AddResultAndRemoveFromPendingInstalls(
    const UnhashedAppId& unhashed_app_id,
    webapps::InstallResultCode result) {
  auto mojo_result = InstallResultCodeToMojo(result);
  std::pair result_pair(unhashed_app_id, mojo_result);
  AddResultToDebugData(unhashed_app_id, pending_installs_map_[unhashed_app_id],
                       GenerateAppIdFromUnhashed(unhashed_app_id), result,
                       mojo_result);
  results_.emplace_back(result_pair);
  pending_installs_map_.erase(unhashed_app_id);
}

bool SubAppInstallCommand::IsWebContentsDestroyed() {
  return lock_->shared_web_contents().IsBeingDestroyed();
}

void SubAppInstallCommand::AddResultToDebugData(
    const UnhashedAppId& unhashed_app_id,
    const GURL& install_url,
    const AppId& installed_app_id,
    webapps::InstallResultCode detailed_code,
    const blink::mojom::SubAppsServiceResultCode& result_code) {
  base::Value::Dict install_info;
  install_info.Set("unhashed_app_id", unhashed_app_id);
  install_info.Set("install_url", install_url.spec());
  install_info.Set("detailed_result_code", base::ToString(detailed_code));
  install_info.Set("result_code", base::ToString(result_code));
  debug_install_results_.Set(installed_app_id,
                             base::Value(std::move(install_info)));
}

}  // namespace web_app
