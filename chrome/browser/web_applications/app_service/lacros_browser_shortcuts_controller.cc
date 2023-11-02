// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/lacros_browser_shortcuts_controller.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/web_app.h"
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
          &LacrosBrowserShortcutsController::InitializeOnRegistryReady,
          weak_ptr_factory_.GetWeakPtr()));
}

void LacrosBrowserShortcutsController::InitializeOnRegistryReady() {
  MaybePublishBrowserShortcuts(
      provider_->registrar_unsafe().GetAppIds(), false,
      base::BindOnce(&OnInitialBrowserShortcutsPublished));

  install_manager_observation_.Observe(&provider_->install_manager());
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
    // TODO(b/306295113): Add shortcut specific icon masking.
    shortcut->icon_key = std::move(
        *icon_key_factory_.CreateIconKey(apps::IconEffects::kCrOsStandardMask));
    shortcut->icon_key->raw_icon_updated = raw_icon_updated;

    shortcuts.push_back(std::move(shortcut));
  }
  remote_publisher->PublishShortcuts(std::move(shortcuts), std::move(callback));
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

}  // namespace web_app
