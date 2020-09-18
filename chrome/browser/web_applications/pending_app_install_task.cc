// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_install_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/installable/installable_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

PendingAppInstallTask::Result::Result(InstallResultCode code,
                                      base::Optional<AppId> app_id)
    : code(code), app_id(std::move(app_id)) {
  DCHECK_EQ(IsNewInstall(code), app_id.has_value());
}

PendingAppInstallTask::Result::Result(Result&&) = default;

PendingAppInstallTask::Result::~Result() = default;

// static
void PendingAppInstallTask::CreateTabHelpers(
    content::WebContents* web_contents) {
  InstallableManager::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
}

PendingAppInstallTask::PendingAppInstallTask(
    Profile* profile,
    AppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppUiManager* ui_manager,
    InstallFinalizer* install_finalizer,
    InstallManager* install_manager,
    ExternalInstallOptions install_options)
    : profile_(profile),
      registrar_(registrar),
      os_integration_manager_(os_integration_manager),
      install_finalizer_(install_finalizer),
      install_manager_(install_manager),
      ui_manager_(ui_manager),
      externally_installed_app_prefs_(profile_->GetPrefs()),
      install_options_(std::move(install_options)) {}

PendingAppInstallTask::~PendingAppInstallTask() = default;

void PendingAppInstallTask::Install(content::WebContents* web_contents,
                                    WebAppUrlLoader::Result load_url_result,
                                    ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(web_contents->GetBrowserContext(), profile_);

  ResultCallback retry_on_failure = base::BindOnce(
      &PendingAppInstallTask::TryAppInfoFactoryOnFailure,
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
    UninstallPlaceholderApp(web_contents, std::move(retry_on_failure));
    return;
  }

  if (install_options_.install_placeholder) {
    InstallPlaceholder(std::move(retry_on_failure));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (load_url_result == WebAppUrlLoader::Result::kFailedWebContentsDestroyed)
    return;

  InstallResultCode code = InstallResultCode::kInstallURLLoadFailed;

  switch (load_url_result) {
    case WebAppUrlLoader::Result::kUrlLoaded:
    case WebAppUrlLoader::Result::kFailedWebContentsDestroyed:
      // Handled above.
      NOTREACHED();
      break;
    case WebAppUrlLoader::Result::kRedirectedUrlLoaded:
      code = InstallResultCode::kInstallURLRedirected;
      break;
    case WebAppUrlLoader::Result::kFailedUnknownReason:
      code = InstallResultCode::kInstallURLLoadFailed;
      break;
    case WebAppUrlLoader::Result::kFailedPageTookTooLong:
      code = InstallResultCode::kInstallURLLoadTimeOut;
      break;
    case WebAppUrlLoader::Result::kFailedErrorPageLoaded:
      code = InstallResultCode::kInstallURLLoadFailed;
      break;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(retry_on_failure), Result(code, base::nullopt)));
}

void PendingAppInstallTask::InstallFromInfo(ResultCallback result_callback) {
  auto internal_install_source = ConvertExternalInstallSourceToInstallSource(
      install_options().install_source);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);
  auto web_app_info = install_options_.app_info_factory.Run();
  for (std::string& search_term : install_params.additional_search_terms) {
    web_app_info->additional_search_terms.push_back(std::move(search_term));
  }
  install_manager_->InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes, install_params,
      internal_install_source,
      base::BindOnce(&PendingAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /* is_placeholder=*/false,
                     /*offline_install=*/true, std::move(result_callback)));
}

void PendingAppInstallTask::UninstallPlaceholderApp(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  base::Optional<AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.install_url);

  // If there is no placeholder app or the app is not installed,
  // then no need to uninstall anything.
  if (!app_id.has_value() || !registrar_->IsInstalled(app_id.value())) {
    ContinueWebAppInstall(web_contents, std::move(result_callback));
    return;
  }

  // Otherwise, uninstall the placeholder app.
  install_finalizer_->UninstallExternalWebAppByUrl(
      install_options_.install_url, install_options_.install_source,
      base::BindOnce(&PendingAppInstallTask::OnPlaceholderUninstalled,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void PendingAppInstallTask::OnPlaceholderUninstalled(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    bool uninstalled) {
  if (!uninstalled) {
    LOG(ERROR) << "Failed to uninstall placeholder for: "
               << install_options_.install_url;
    std::move(result_callback)
        .Run(Result(InstallResultCode::kFailedPlaceholderUninstall,
                    base::nullopt));
    return;
  }
  ContinueWebAppInstall(web_contents, std::move(result_callback));
}

void PendingAppInstallTask::ContinueWebAppInstall(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options_.install_source);

  install_manager_->InstallWebAppWithParams(
      web_contents, install_params, install_source,
      base::BindOnce(&PendingAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /*is_placeholder=*/false,
                     /*offline_install=*/false, std::move(result_callback)));
}

void PendingAppInstallTask::InstallPlaceholder(ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.install_url);
  if (app_id.has_value() && registrar_->IsInstalled(app_id.value())) {
    // No need to install a placeholder app again.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       Result(InstallResultCode::kSuccessNewInstall, app_id)));
    return;
  }

  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(install_options_.install_url.spec());
  web_app_info.start_url = install_options_.install_url;

  switch (install_options_.user_display_mode) {
    case DisplayMode::kUndefined:
    case DisplayMode::kBrowser:
      web_app_info.open_as_window = false;
      break;
    case DisplayMode::kMinimalUi:
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      web_app_info.open_as_window = true;
      break;
  }

  InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::EXTERNAL_POLICY;

  install_finalizer_->FinalizeInstall(
      web_app_info, options,
      base::BindOnce(&PendingAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /*is_placeholder=*/true,
                     /*offline_install=*/false, std::move(callback)));
}

void PendingAppInstallTask::OnWebAppInstalled(bool is_placeholder,
                                              bool offline_install,
                                              ResultCallback result_callback,
                                              const AppId& app_id,
                                              InstallResultCode code) {
  if (!IsNewInstall(code)) {
    std::move(result_callback).Run(Result(code, base::nullopt));
    return;
  }

  ui_manager_->UninstallAndReplaceIfExists(
      install_options().uninstall_and_replace, app_id);

  externally_installed_app_prefs_.Insert(install_options_.install_url, app_id,
                                         install_options_.install_source);
  externally_installed_app_prefs_.SetIsPlaceholder(install_options_.install_url,
                                                   is_placeholder);

  if (offline_install) {
    code = install_options().only_use_app_info_factory
               ? InstallResultCode::kSuccessOfflineOnlyInstall
               : InstallResultCode::kSuccessOfflineFallbackInstall;
  }
  base::ScopedClosureRunner scoped_closure(
      base::BindOnce(std::move(result_callback), Result(code, app_id)));

  if (!is_placeholder) {
    return;
  }
  InstallOsHooksOptions options;
  options.os_hooks[OsHookType::kShortcuts] =
      install_options_.add_to_applications_menu;
  options.add_to_desktop = install_options_.add_to_desktop;
  options.add_to_quick_launch_bar = install_options_.add_to_quick_launch_bar;
  options.os_hooks[OsHookType::kRunOnOsLogin] =
      install_options_.run_on_os_login;

  // TODO(crbug.com/1087219): Determine if |register_file_handlers| should be
  // configured from somewhere else rather than always true.
  options.os_hooks[OsHookType::kFileHandlers] = true;
  options.os_hooks[OsHookType::kShortcutsMenu] = true;

  os_integration_manager_->InstallOsHooks(
      app_id,
      base::BindOnce(
          [](base::ScopedClosureRunner scoped_closure,
             OsHooksResults os_hooks_results) { scoped_closure.RunAndReset(); },
          std::move(scoped_closure)),
      nullptr, options);
}

void PendingAppInstallTask::TryAppInfoFactoryOnFailure(
    ResultCallback result_callback,
    Result result) {
  if (!IsSuccess(result.code) && install_options().app_info_factory) {
    InstallFromInfo(std::move(result_callback));
    return;
  }
  std::move(result_callback).Run(std::move(result));
}

}  // namespace web_app
