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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

PendingAppInstallTask::Result::Result(InstallResultCode code,
                                      base::Optional<AppId> app_id)
    : code(code), app_id(std::move(app_id)) {
  DCHECK_EQ(code == InstallResultCode::kSuccessNewInstall, app_id.has_value());
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
    AppShortcutManager* shortcut_manger,
    WebAppUiManager* ui_manager,
    InstallFinalizer* install_finalizer,
    ExternalInstallOptions install_options)
    : profile_(profile),
      registrar_(registrar),
      shortcut_manager_(shortcut_manger),
      install_finalizer_(install_finalizer),
      ui_manager_(ui_manager),
      externally_installed_app_prefs_(profile_->GetPrefs()),
      install_options_(std::move(install_options)) {}

PendingAppInstallTask::~PendingAppInstallTask() = default;

void PendingAppInstallTask::Install(content::WebContents* web_contents,
                                    WebAppUrlLoader::Result load_url_result,
                                    ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(web_contents->GetBrowserContext(), profile_);

  if (load_url_result == WebAppUrlLoader::Result::kUrlLoaded) {
    // If we are not re-installing a placeholder, then no need to uninstall
    // anything.
    if (!install_options_.reinstall_placeholder) {
      ContinueWebAppInstall(web_contents, std::move(result_callback));
      return;
    }
    // Calling InstallWebAppWithOptions with the same URL used to install a
    // placeholder won't necessarily replace the placeholder app, because the
    // new app might be installed with a new AppId. To avoid this, always
    // uninstall the placeholder app.
    UninstallPlaceholderApp(web_contents, std::move(result_callback));
    return;
  }

  if (install_options_.install_placeholder) {
    InstallPlaceholder(std::move(result_callback));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (load_url_result == WebAppUrlLoader::Result::kFailedWebContentsDestroyed)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(result_callback),
          Result(
              load_url_result == WebAppUrlLoader::Result::kRedirectedUrlLoaded
                  ? InstallResultCode::kInstallURLRedirected
                  : InstallResultCode::kInstallURLLoadFailed,
              base::nullopt)));
}

void PendingAppInstallTask::UninstallPlaceholderApp(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  base::Optional<AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.url);

  // If there is no placeholder app or the app is not installed,
  // then no need to uninstall anything.
  if (!app_id.has_value() || !registrar_->IsInstalled(app_id.value())) {
    ContinueWebAppInstall(web_contents, std::move(result_callback));
    return;
  }

  // Otherwise, uninstall the placeholder app.
  install_finalizer_->UninstallExternalWebApp(
      install_options_.url, install_options_.install_source,
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
               << install_options_.url;
    std::move(result_callback)
        .Run(Result(InstallResultCode::kFailedUnknownReason, base::nullopt));
    return;
  }
  ContinueWebAppInstall(web_contents, std::move(result_callback));
}

void PendingAppInstallTask::ContinueWebAppInstall(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  auto* provider = WebAppProviderBase::GetProviderBase(profile_);
  DCHECK(provider);

  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options_.install_source);

  provider->install_manager().InstallWebAppWithParams(
      web_contents, install_params, install_source,
      base::BindOnce(&PendingAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), false /* is_placeholder */,
                     std::move(result_callback)));
}

void PendingAppInstallTask::InstallPlaceholder(ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.url);
  if (app_id.has_value() && registrar_->IsInstalled(app_id.value())) {
    // No need to install a placeholder app again.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       Result(InstallResultCode::kSuccessNewInstall, app_id)));
    return;
  }

  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(install_options_.url.spec());
  web_app_info.app_url = install_options_.url;

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
                     weak_ptr_factory_.GetWeakPtr(), true /* is_placeholder */,
                     std::move(callback)));
}

void PendingAppInstallTask::OnWebAppInstalled(bool is_placeholder,
                                              ResultCallback result_callback,
                                              const AppId& app_id,
                                              InstallResultCode code) {
  if (code != InstallResultCode::kSuccessNewInstall) {
    std::move(result_callback).Run(Result(code, base::nullopt));
    return;
  }

  // If this is the first time the app has been installed, run a migration. This
  // will not happen again, even if the app is uninstalled and reinstalled.
  if (!externally_installed_app_prefs_.LookupAppId(install_options_.url)) {
    ui_manager_->UninstallAndReplace(install_options().uninstall_and_replace,
                                     app_id);
  }

  externally_installed_app_prefs_.Insert(install_options_.url, app_id,
                                         install_options_.install_source);
  externally_installed_app_prefs_.SetIsPlaceholder(install_options_.url,
                                                   is_placeholder);

  base::ScopedClosureRunner scoped_closure(
      base::BindOnce(std::move(result_callback),
                     Result(InstallResultCode::kSuccessNewInstall, app_id)));

  if (!is_placeholder) {
    return;
  }

  // Installation through InstallFinalizer doesn't create shortcuts so create
  // them here.
  if (install_options_.add_to_quick_launch_bar &&
      install_finalizer_->CanAddAppToQuickLaunchBar()) {
    install_finalizer_->AddAppToQuickLaunchBar(app_id);
  }

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (install_options_.add_to_applications_menu &&
      shortcut_manager_->CanCreateShortcuts()) {
    shortcut_manager_->CreateShortcuts(
        app_id, install_options_.add_to_desktop,
        base::BindOnce(
            [](base::ScopedClosureRunner scoped_closure,
               bool shortcuts_created) {
              // Even if the shortcuts failed to be created, we consider the
              // installation successful since an app was created.
              scoped_closure.RunAndReset();
            },
            std::move(scoped_closure)));
    return;
  }
}

}  // namespace web_app
