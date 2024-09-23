// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace web_app {

namespace {

bool IsAppInstalled(Profile* profile, const webapps::AppId& app_id) {
  bool installed = false;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

mojom::UserDisplayMode GetExtensionUserDisplayMode(
    Profile* profile,
    const extensions::Extension* extension) {
  // Platform apps always open in an app window and their user preference is
  // meaningless.
  if (extension->is_platform_app()) {
    return mojom::UserDisplayMode::kStandalone;
  }

  switch (extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile), extension)) {
    case apps::LaunchContainer::kLaunchContainerWindow:
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      return mojom::UserDisplayMode::kStandalone;
    case apps::LaunchContainer::kLaunchContainerTab:
    case apps::LaunchContainer::kLaunchContainerNone:
      return mojom::UserDisplayMode::kBrowser;
  }
}

}  // namespace

WebAppUninstallAndReplaceJob::WebAppUninstallAndReplaceJob(
    Profile* profile,
    base::Value::Dict& debug_value,
    WithAppResources& to_app_lock,
    const std::vector<webapps::AppId>& from_apps_or_extensions,
    const webapps::AppId& to_app,
    base::OnceCallback<void(bool uninstall_triggered)> on_complete)
    : profile_(*profile),
      debug_value_(debug_value),
      to_app_lock_(to_app_lock),
      from_apps_or_extensions_(from_apps_or_extensions),
      to_app_(to_app),
      on_complete_(std::move(on_complete)) {}
WebAppUninstallAndReplaceJob::~WebAppUninstallAndReplaceJob() = default;

void WebAppUninstallAndReplaceJob::Start() {
  CHECK(to_app_lock_->registrar().GetAppById(to_app_));

  std::vector<webapps::AppId> apps_to_replace;
  for (const webapps::AppId& from_app : from_apps_or_extensions_) {
    if (IsAppInstalled(&profile_.get(), from_app)) {
      apps_to_replace.emplace_back(from_app);
    }
  }

  if (apps_to_replace.empty()) {
    debug_value_->Set("did_uninstall_and_replace", false);
    std::move(on_complete_).Run(/*uninstall_triggered=*/false);
    return;
  }

  debug_value_->Set("did_uninstall_and_replace", true);
  MigrateUiAndUninstallApp(
      apps_to_replace.front(),
      base::BindOnce(std::move(on_complete_), /*uninstall_triggered=*/true));

  apps_to_replace.erase(apps_to_replace.begin());
  for (const auto& app : apps_to_replace) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(&profile_.get());
    proxy->UninstallSilently(app, apps::UninstallSource::kMigration);
  }
}

void WebAppUninstallAndReplaceJob::MigrateUiAndUninstallApp(
    const webapps::AppId& from_app,
    base::OnceClosure on_complete) {
#if BUILDFLAG(IS_CHROMEOS)
  to_app_lock_->ui_manager().MigrateLauncherState(
      from_app, to_app_,
      base::BindOnce(&WebAppUninstallAndReplaceJob::OnMigrateLauncherState,
                     weak_ptr_factory_.GetWeakPtr(), from_app,
                     std::move(on_complete)));
#else
  OnMigrateLauncherState(from_app, std::move(on_complete));
#endif
}

void WebAppUninstallAndReplaceJob::OnMigrateLauncherState(
    const webapps::AppId& from_app,
    base::OnceClosure on_complete) {
  // If migration of user/UI data is required for other app types consider
  // generalising this operation to be part of app service.
  const extensions::Extension* from_extension =
      extensions::ExtensionRegistry::Get(&profile_.get())
          ->enabled_extensions()
          .GetByID(from_app);
  if (from_extension) {
    // Grid position in chrome://apps.
    extensions::AppSorting* app_sorting =
        extensions::ExtensionSystem::Get(&profile_.get())->app_sorting();
    app_sorting->SetAppLaunchOrdinal(
        to_app_, app_sorting->GetAppLaunchOrdinal(from_app));
    app_sorting->SetPageOrdinal(to_app_, app_sorting->GetPageOrdinal(from_app));

    SetUserDisplayModeCommand::DoSetDisplayMode(
        *to_app_lock_, to_app_,
        GetExtensionUserDisplayMode(&profile_.get(), from_extension),
        /*is_user_action=*/false);

    auto shortcut_info = web_app::ShortcutInfoForExtensionAndProfile(
        from_extension, &profile_.get());
    to_app_lock_->os_integration_manager().GetAppExistingShortCutLocation(
        base::BindOnce(
            &WebAppUninstallAndReplaceJob::OnShortcutLocationGathered,
            weak_ptr_factory_.GetWeakPtr(), from_app, std::move(on_complete)),
        std::move(shortcut_info));
  } else {
    // The from_app could be a web app.
    to_app_lock_->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
        from_app,
        base::BindOnce(&WebAppUninstallAndReplaceJob::
                           OnShortcutInfoReceivedSearchShortcutLocations,
                       weak_ptr_factory_.GetWeakPtr(), from_app,
                       std::move(on_complete)));
  }
}

void WebAppUninstallAndReplaceJob::
    OnShortcutInfoReceivedSearchShortcutLocations(
        const webapps::AppId& from_app,
        base::OnceClosure on_complete,
        std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (!shortcut_info) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(&profile_.get());
    // The shortcut info couldn't be found, simply uninstall.
    proxy->UninstallSilently(from_app, apps::UninstallSource::kMigration);
    std::move(on_complete).Run();
    return;
  }

  auto callback = base::BindOnce(
      &WebAppUninstallAndReplaceJob::OnShortcutLocationGathered,
      weak_ptr_factory_.GetWeakPtr(), from_app, std::move(on_complete));
  to_app_lock_->os_integration_manager().GetAppExistingShortCutLocation(
      std::move(callback), std::move(shortcut_info));
}

void WebAppUninstallAndReplaceJob::OnShortcutLocationGathered(
    const webapps::AppId& from_app,
    base::OnceClosure on_complete,
    ShortcutLocations from_app_locations) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(&profile_.get());

  const bool is_extension = proxy->AppRegistryCache().GetAppType(from_app) ==
                            apps::AppType::kChromeApp;
  bool run_on_os_login = from_app_locations.in_startup;
  if (is_extension) {
    // Need to be called before `proxy->UninstallSilently` because
    // UninstallSilently might synchronously finish, so the wait won't get
    // finished if called after.
    WaitForExtensionShortcutsDeleted(
        from_app,
        base::BindOnce(&WebAppUninstallAndReplaceJob::
                           SynchronizeOSIntegrationForReplacementApp,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_complete),
                       run_on_os_login, from_app_locations));
  } else {
    // Platforms like Mac don't fetch the 'run on os login' property from the
    // GetAppExistingShortCutLocation API.
    run_on_os_login =
        run_on_os_login ||
        to_app_lock_->registrar().GetAppRunOnOsLoginMode(from_app).value ==
            RunOnOsLoginMode::kWindowed;
  }

  // When the `from_app` is a web app, we can't wait for it to finish because it
  // underlying schedules WebAppUninstallCommand which uses a `AllAppsLock`,
  // so the uninstall command won't get started until current command that holds
  // the `to_app_lock` finishes.
  proxy->UninstallSilently(from_app, apps::UninstallSource::kMigration);

  if (!is_extension) {
    SynchronizeOSIntegrationForReplacementApp(
        std::move(on_complete), run_on_os_login, from_app_locations);
  }
}

void WebAppUninstallAndReplaceJob::SynchronizeOSIntegrationForReplacementApp(
    base::OnceClosure on_complete,
    bool from_app_run_on_os_login,
    ShortcutLocations from_app_locations) {
  ValueWithPolicy<RunOnOsLoginMode> run_on_os_login =
      to_app_lock_->registrar().GetAppRunOnOsLoginMode(to_app_);
  if (run_on_os_login.user_controllable) {
    RunOnOsLoginMode new_mode = from_app_run_on_os_login
                                    ? RunOnOsLoginMode::kWindowed
                                    : RunOnOsLoginMode::kNotRun;
    if (new_mode != run_on_os_login.value) {
      {
        ScopedRegistryUpdate update = to_app_lock_->sync_bridge().BeginUpdate();
        update->UpdateApp(to_app_)->SetRunOnOsLoginMode(new_mode);
      }
    }
  }

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = from_app_locations.on_desktop;
  synchronize_options.add_to_quick_launch_bar =
      from_app_locations.in_quick_launch_bar;
  synchronize_options.reason = SHORTCUT_CREATION_AUTOMATED;
  to_app_lock_->os_integration_manager().Synchronize(to_app_,
                                                     std::move(on_complete));
}

}  // namespace web_app
