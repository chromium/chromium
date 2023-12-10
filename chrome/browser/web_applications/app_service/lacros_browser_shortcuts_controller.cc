// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/lacros_browser_shortcuts_controller.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
// TODO(crbug.com/1402145): Remove circular dependencies on //c/b/ui.
#include "chrome/browser/ui/startup/first_run_service.h"  // nogncheck
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "ui/base/window_open_disposition.h"

class Browser;

namespace {

base::OnceClosure* GetInitializedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> g_initialized_callback;
  return g_initialized_callback.get();
}

void OnInitialBrowserShortcutsPublished() {
  if (*GetInitializedCallbackForTesting()) {
    std::move(*GetInitializedCallbackForTesting()).Run();
  }
}

}  // namespace

namespace web_app {

LacrosBrowserShortcutsController::LacrosBrowserShortcutsController(
    Profile* profile)
    : profile_(profile), provider_(WebAppProvider::GetForWebApps(profile_)) {}

LacrosBrowserShortcutsController::~LacrosBrowserShortcutsController() = default;

void LacrosBrowserShortcutsController::SetInitializedCallbackForTesting(
    base::OnceClosure callback) {
  *GetInitializedCallbackForTesting() = std::move(callback);
}

void LacrosBrowserShortcutsController::LaunchShortcut(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    int64_t display_id,
    LaunchShortcutCallback callback) {
  auto* fre_service = FirstRunServiceFactory::GetForBrowserContext(profile_);
  if (!fre_service || !fre_service->ShouldOpenFirstRun()) {
    LaunchShortcutInternal(host_app_id, local_shortcut_id, display_id,
                           std::move(callback));
    return;
  }

  fre_service->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      base::BindOnce(
          &LacrosBrowserShortcutsController::OnOpenPrimaryProfileFirstRunExited,
          weak_ptr_factory_.GetWeakPtr(), host_app_id, local_shortcut_id,
          display_id, std::move(callback)));
}

void LacrosBrowserShortcutsController::GetCompressedIcon(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  apps::GetWebAppCompressedIconData(profile_, local_shortcut_id, size_in_dip,
                                    scale_factor, std::move(callback));
}

void LacrosBrowserShortcutsController::RemoveShortcut(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    apps::UninstallSource uninstall_source,
    RemoveShortcutCallback callback) {
  if (!IsAppServiceShortcut(local_shortcut_id, *provider_)) {
    return;
  }

  const WebApp* web_app =
      provider_->registrar_unsafe().GetAppById(local_shortcut_id);
  if (!web_app) {
    return;
  }

  auto origin = url::Origin::Create(web_app->start_url());

  CHECK(
      provider_->registrar_unsafe().CanUserUninstallWebApp(web_app->app_id()));
  webapps::WebappUninstallSource webapp_uninstall_source =
      ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
  provider_->scheduler().UninstallWebApp(
      web_app->app_id(), webapp_uninstall_source,
      base::IgnoreArgs<webapps::UninstallResultCode>(std::move(callback)));
}

void LacrosBrowserShortcutsController::Initialize() {
  CHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  auto* service = chromeos::LacrosService::Get();
  if (!service ||
      !service->IsAvailable<crosapi::mojom::AppShortcutPublisher>()) {
    return;
  }
  if (!chromeos::features::IsCrosWebAppShortcutUiUpdateEnabled()) {
    return;
  }

  CHECK(provider_);
  provider_->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(
          &LacrosBrowserShortcutsController::RegisterControllerOnRegistryReady,
          weak_ptr_factory_.GetWeakPtr()));
}

void LacrosBrowserShortcutsController::RegisterControllerOnRegistryReady() {
  auto* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::AppShortcutPublisher>() <
          int{crosapi::mojom::AppShortcutPublisher::MethodMinVersions::
                  kRegisterAppShortcutControllerMinVersion} &&
      !chromeos::BrowserParamsProxy::Get()->IsCrosapiDisabledForTesting()) {
    LOG(WARNING)
        << "Ash AppShortcutPublisher version "
        << service->GetInterfaceVersion<crosapi::mojom::AppShortcutPublisher>()
        << " does not support RegisterAppShortcutController().";
    return;
  }
  service->GetRemote<crosapi::mojom::AppShortcutPublisher>()
      ->RegisterAppShortcutController(
          receiver_.BindNewPipeAndPassRemoteWithVersion(),
          base::BindOnce(
              &LacrosBrowserShortcutsController::InitializeOnControllerReady,
              weak_ptr_factory_.GetWeakPtr()));
}

void LacrosBrowserShortcutsController::InitializeOnControllerReady(
    crosapi::mojom::ControllerRegistrationResult result) {
  if (result != crosapi::mojom::ControllerRegistrationResult::kSuccess) {
    return;
  }
  MaybePublishBrowserShortcuts(
      provider_->registrar_unsafe().GetAppIds(), false,
      base::BindOnce(&OnInitialBrowserShortcutsPublished));

  install_manager_observation_.Observe(&provider_->install_manager());
  registrar_observation_.Observe(&provider_->registrar_unsafe());
}

void LacrosBrowserShortcutsController::MaybePublishBrowserShortcuts(
    const std::vector<webapps::AppId>& app_ids,
    bool raw_icon_updated,
    crosapi::mojom::AppShortcutPublisher::PublishShortcutsCallback callback) {
  auto* service = chromeos::LacrosService::Get();
  auto* remote_publisher =
      service->GetRemote<crosapi::mojom::AppShortcutPublisher>().get();

  if (service->GetInterfaceVersion<crosapi::mojom::AppShortcutPublisher>() <
          int{crosapi::mojom::AppShortcutPublisher::MethodMinVersions::
                  kPublishShortcutsMinVersion} &&
      !chromeos::BrowserParamsProxy::Get()->IsCrosapiDisabledForTesting()) {
    LOG(WARNING)
        << "Ash AppShortcutPublisher version "
        << service->GetInterfaceVersion<crosapi::mojom::AppShortcutPublisher>()
        << " does not support PublishShortcuts().";
    return;
  }

  std::vector<apps::ShortcutPtr> shortcuts;

  for (const auto& app_id : app_ids) {
    if (!IsAppServiceShortcut(app_id, *provider_)) {
      continue;
    }
    const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
    if (!web_app) {
      continue;
    }

    apps::ShortcutPtr shortcut = std::make_unique<apps::Shortcut>(
        app_constants::kLacrosAppId, web_app->app_id());
    shortcut->name =
        provider_->registrar_unsafe().GetAppShortName(web_app->app_id());
    shortcut->shortcut_source = apps::ShortcutSource::kUser;

    apps::IconEffects icon_effects = apps::IconEffects::kRoundCorners;
    icon_effects |= web_app->is_generated_icon()
                        ? apps::IconEffects::kCrOsStandardMask
                        : apps::IconEffects::kCrOsStandardIcon;
    shortcut->icon_key = apps::IconKey(raw_icon_updated, icon_effects);

    shortcut->allow_removal =
        provider_->registrar_unsafe().CanUserUninstallWebApp(web_app->app_id());

    shortcuts.push_back(std::move(shortcut));
  }
  remote_publisher->PublishShortcuts(std::move(shortcuts), std::move(callback));
}

void LacrosBrowserShortcutsController::OnOpenPrimaryProfileFirstRunExited(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    int64_t display_id,
    LaunchShortcutCallback callback,
    bool proceed) {
  if (!proceed) {
    std::move(callback).Run();
    return;
  }
  LaunchShortcutInternal(host_app_id, local_shortcut_id, display_id,
                         std::move(callback));
}

void LacrosBrowserShortcutsController::LaunchShortcutInternal(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    int64_t display_id,
    LaunchShortcutCallback callback) {
  apps::AppLaunchParams params(
      local_shortcut_id, apps::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::LaunchSource::kFromAppListGrid, display_id);
  provider_->scheduler().LaunchAppWithCustomParams(
      std::move(params),
      base::IgnoreArgs<base::WeakPtr<Browser>,
                       base::WeakPtr<content::WebContents>,
                       apps::LaunchContainer>(std::move(callback)));
}

void LacrosBrowserShortcutsController::OnWebAppInstalled(
    const webapps::AppId& app_id) {
  MaybePublishBrowserShortcuts({app_id});
}

void LacrosBrowserShortcutsController::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  MaybePublishBrowserShortcuts({app_id});
}

void LacrosBrowserShortcutsController::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void LacrosBrowserShortcutsController::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  // Once a web app has been uninstalled, the WebAppRegistrar can no longer
  // be used to determine if it is a shortcut. Here we check if we have got an
  // app registered in AppRegistryCache that can be be uninstalled. If this is
  // registered as an app, we do not update for shortcut.
  bool found = apps::AppServiceProxyFactory::GetForProfile(profile_)
                   ->AppRegistryCache()
                   .ForOneApp(app_id, [](const apps::AppUpdate& update) {});
  if (found) {
    return;
  }

  auto* service = chromeos::LacrosService::Get();
  auto* remote_publisher =
      service->GetRemote<crosapi::mojom::AppShortcutPublisher>().get();

  if (service->GetInterfaceVersion<crosapi::mojom::AppShortcutPublisher>() <
          int{crosapi::mojom::AppShortcutPublisher::MethodMinVersions::
                  kShortcutRemovedMinVersion} &&
      !chromeos::BrowserParamsProxy::Get()->IsCrosapiDisabledForTesting()) {
    LOG(WARNING)
        << "Ash AppShortcutPublisher version "
        << service->GetInterfaceVersion<crosapi::mojom::AppShortcutPublisher>()
        << " does not support ShortcutRemoved().";
    return;
  }

  remote_publisher->ShortcutRemoved(
      apps::GenerateShortcutId(app_constants::kLacrosAppId, app_id).value(),
      base::DoNothing());
}

void LacrosBrowserShortcutsController::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void LacrosBrowserShortcutsController::OnWebAppUserDisplayModeChanged(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode) {
  MaybePublishBrowserShortcuts({app_id});
}

}  // namespace web_app
