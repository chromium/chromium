// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

ExternallyManagedAppInstallTask::ExternallyManagedAppInstallTask(
    Profile* profile,
    WebAppUrlLoader* url_loader,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* install_finalizer,
    WebAppCommandScheduler* command_scheduler,
    DataRetrieverFactory data_retriever_factory,
    ExternalInstallOptions install_options)
    : profile_(profile),
      url_loader_(url_loader),
      ui_manager_(ui_manager),
      install_finalizer_(install_finalizer),
      command_scheduler_(command_scheduler),
      data_retriever_factory_(std::move(data_retriever_factory)),
      install_options_(std::move(install_options)) {}

ExternallyManagedAppInstallTask::~ExternallyManagedAppInstallTask() = default;

void ExternallyManagedAppInstallTask::Install(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  if (install_options_.only_use_app_info_factory) {
    DCHECK(install_options_.app_info_factory);
    InstallFromInfo(std::move(result_callback));
    return;
  }

  url_loader_->PrepareForLoad(
      web_contents,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebContentsReady,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

void ExternallyManagedAppInstallTask::OnWebContentsReady(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    WebAppUrlLoader::Result prepare_for_load_result) {
  // TODO(crbug.com/1098139): Handle the scenario where WebAppUrlLoader fails to
  // load about:blank and flush WebContents states.
  url_loader_->LoadUrl(
      install_options_.install_url, web_contents,
      WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnUrlLoaded,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnUrlLoaded(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    WebAppUrlLoader::Result load_url_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(web_contents->GetBrowserContext(), profile_);

  ResultCallback retry_on_failure = base::BindOnce(
      &ExternallyManagedAppInstallTask::TryAppInfoFactoryOnFailure,
      weak_ptr_factory_.GetWeakPtr(), std::move(result_callback));

  if (load_url_result == WebAppUrlLoader::Result::kUrlLoaded) {
    // If we are not re-installing a placeholder, then no need to uninstall
    // anything.
    if (!install_options_.reinstall_placeholder) {
      ContinueWebAppInstall(web_contents, std::move(retry_on_failure));
      return;
    }
    // Calling InstallWebAppWithOptions with the same URL used to install a
    // placeholder won't necessarily replace the placeholder app, because the
    // new app might be installed with a new AppId. To avoid this, always
    // uninstall the placeholder app.
    GetPlaceholderAppId(
        install_options_.install_url,
        ConvertExternalInstallSourceToSource(install_options_.install_source),
        base::BindOnce(
            &ExternallyManagedAppInstallTask::UninstallPlaceholderApp,
            weak_ptr_factory_.GetWeakPtr(), web_contents,
            std::move(retry_on_failure)));
    return;
  }

  if (install_options_.install_placeholder) {
    GetPlaceholderAppId(
        install_options_.install_url,
        ConvertExternalInstallSourceToSource(install_options_.install_source),
        base::BindOnce(&ExternallyManagedAppInstallTask::InstallPlaceholder,
                       weak_ptr_factory_.GetWeakPtr(), web_contents,
                       std::move(retry_on_failure)));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (load_url_result == WebAppUrlLoader::Result::kFailedWebContentsDestroyed)
    return;

  webapps::InstallResultCode code =
      webapps::InstallResultCode::kInstallURLLoadFailed;

  switch (load_url_result) {
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(retry_on_failure),
                     ExternallyManagedAppManager::InstallResult(code)));
}

void ExternallyManagedAppInstallTask::InstallFromInfo(
    ResultCallback result_callback) {
  auto internal_install_source = ConvertExternalInstallSourceToInstallSource(
      install_options().install_source);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);

  // Do not fetch web_app_origin_association data over network.
  if (install_options_.only_use_app_info_factory) {
    install_params.skip_origin_association_validation = true;
  }

  install_params.bypass_os_hooks = true;
  auto web_app_info = install_options_.app_info_factory.Run();
  for (std::string& search_term : install_params.additional_search_terms) {
    web_app_info->additional_search_terms.push_back(std::move(search_term));
  }
  web_app_info->install_url = install_params.install_url;
  command_scheduler_->InstallFromInfoWithParams(
      std::move(web_app_info),
      /*overwrite_existing_manifest_fields=*/install_params.force_reinstall,
      internal_install_source,
      base::BindOnce(
          &ExternallyManagedAppInstallTask::OnWebAppInstalledAndReplaced,
          weak_ptr_factory_.GetWeakPtr(), /* is_placeholder=*/false,
          /*offline_install=*/true, std::move(result_callback)),
      install_params, install_options_.uninstall_and_replace);
}

void ExternallyManagedAppInstallTask::UninstallPlaceholderApp(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    absl::optional<AppId> app_id) {
  // If there is no app in the DB that is a placeholder app or if the app exists
  // but is not a placeholder, then no need to uninstall anything.
  // These checks are handled by the WebAppRegistrar itself.
  if (!app_id.has_value()) {
    ContinueWebAppInstall(web_contents, std::move(result_callback));
    return;
  }

  // Otherwise, uninstall the placeholder app.
  install_finalizer_->UninstallExternalWebAppByUrl(
      install_options_.install_url,
      ConvertExternalInstallSourceToSource(install_options_.install_source),
      webapps::WebappUninstallSource::kPlaceholderReplacement,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnPlaceholderUninstalled,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnPlaceholderUninstalled(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    webapps::UninstallResultCode code) {
  if (!UninstallSucceeded(code)) {
    LOG(ERROR) << "Failed to uninstall placeholder for: "
               << install_options_.install_url;
    std::move(result_callback)
        .Run(ExternallyManagedAppManager::InstallResult(
            webapps::InstallResultCode::kFailedPlaceholderUninstall));
    return;
  }
  ContinueWebAppInstall(web_contents, std::move(result_callback));
}

void ExternallyManagedAppInstallTask::ContinueWebAppInstall(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  command_scheduler_->InstallExternallyManagedApp(
      install_options_,
      base::BindOnce(
          &ExternallyManagedAppInstallTask::OnWebAppInstalledAndReplaced,
          weak_ptr_factory_.GetWeakPtr(),
          /*is_placeholder=*/false,
          /*offline_install=*/false, std::move(result_callback)),
      web_contents->GetWeakPtr(), data_retriever_factory_.Run(), url_loader_);
}

void ExternallyManagedAppInstallTask::InstallPlaceholder(
    content::WebContents* web_contents,
    ResultCallback callback,
    absl::optional<AppId> app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (app_id.has_value() && !install_options_.force_reinstall) {
    // No need to install a placeholder app again.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id)));
    return;
  }

  command_scheduler_->InstallPlaceholder(
      install_options_,
      base::BindOnce(
          &ExternallyManagedAppInstallTask::OnWebAppInstalledAndReplaced,
          weak_ptr_factory_.GetWeakPtr(), /*is_placeholder=*/true,
          /*offline_install=*/false, std::move(callback)),
      web_contents->GetWeakPtr());
}

void ExternallyManagedAppInstallTask::OnWebAppInstalledAndReplaced(
    bool is_placeholder,
    bool offline_install,
    ResultCallback result_callback,
    const AppId& app_id,
    webapps::InstallResultCode code,
    bool did_uninstall_and_replace) {
  if (!IsNewInstall(code)) {
    std::move(result_callback)
        .Run(ExternallyManagedAppManager::InstallResult(code));
    return;
  }

  if (offline_install) {
    code = install_options().only_use_app_info_factory
               ? webapps::InstallResultCode::kSuccessOfflineOnlyInstall
               : webapps::InstallResultCode::kSuccessOfflineFallbackInstall;
  }

  std::move(result_callback)
      .Run(ExternallyManagedAppManager::InstallResult(
          code, app_id, did_uninstall_and_replace));
}

void ExternallyManagedAppInstallTask::TryAppInfoFactoryOnFailure(
    ResultCallback result_callback,
    ExternallyManagedAppManager::InstallResult result) {
  if (!IsSuccess(result.code) && install_options().app_info_factory) {
    InstallFromInfo(std::move(result_callback));
    return;
  }
  std::move(result_callback).Run(std::move(result));
}

void ExternallyManagedAppInstallTask::GetPlaceholderAppId(
    const GURL& install_url,
    WebAppManagement::Type source_type,
    base::OnceCallback<void(absl::optional<AppId>)> callback) {
  command_scheduler_->ScheduleCallbackWithLock<AllAppsLock>(
      "ExternallyManagedAppInstallTask::GetPlaceholderAppId",
      std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(
          [](const GURL& install_url, WebAppManagement::Type source_type,
             base::OnceCallback<void(absl::optional<AppId>)> callback,
             AllAppsLock& lock) {
            absl::optional<AppId> app_id =
                lock.registrar().LookupPlaceholderAppId(install_url,
                                                        source_type);
            std::move(callback).Run(app_id);
          },
          install_url, source_type, std::move(callback)));
}

}  // namespace web_app
