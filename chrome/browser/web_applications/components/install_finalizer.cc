// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_finalizer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"

namespace web_app {

InstallFinalizer::FinalizeOptions::FinalizeOptions() = default;

InstallFinalizer::FinalizeOptions::~FinalizeOptions() = default;

InstallFinalizer::FinalizeOptions::FinalizeOptions(const FinalizeOptions&) =
    default;

void InstallFinalizer::UninstallExternalWebAppByUrl(
    const GURL& app_url,
    ExternalInstallSource external_install_source,
    UninstallWebAppCallback callback) {
  base::Optional<AppId> app_id = registrar().LookupExternalAppId(app_url);
  if (!app_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall web app with url " << app_url
                 << "; No corresponding web app for url.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*uninstalled=*/false));
    return;
  }

  UninstallExternalWebApp(app_id.value(), external_install_source,
                          std::move(callback));
}

void InstallFinalizer::SetSubsystems(
    AppRegistrar* registrar,
    WebAppUiManager* ui_manager,
    AppRegistryController* registry_controller,
    OsIntegrationManager* os_integration_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  registry_controller_ = registry_controller;
  os_integration_manager_ = os_integration_manager;
}

bool InstallFinalizer::CanReparentTab(const AppId& app_id,
                                      bool shortcut_created) const {
  // Reparent the web contents into its own window only if that is the
  // app's launch type.
  DCHECK(registrar_);
  if (registrar_->GetAppUserDisplayMode(app_id) != DisplayMode::kStandalone)
    return false;

  return ui_manager().CanReparentAppTabToWindow(app_id, shortcut_created);
}

void InstallFinalizer::ReparentTab(const AppId& app_id,
                                   bool shortcut_created,
                                   content::WebContents* web_contents) {
  DCHECK(web_contents);
  return ui_manager().ReparentAppTabToWindow(web_contents, app_id,
                                             shortcut_created);
}

AppRegistrar& InstallFinalizer::registrar() const {
  DCHECK(!is_legacy_finalizer());
  return *registrar_;
}

}  // namespace web_app
