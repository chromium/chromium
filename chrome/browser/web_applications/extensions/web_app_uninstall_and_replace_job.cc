// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

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
    WithAppResources& to_app_lock,
    const std::vector<webapps::AppId>& from_apps_or_extensions,
    const webapps::AppId& to_app,
    base::OnceCallback<void(bool uninstall_triggered)> on_complete)
    : profile_(*profile),
      to_app_lock_(to_app_lock),
      from_apps_or_extensions_(from_apps_or_extensions),
      to_app_(to_app),
      on_complete_(std::move(on_complete)) {}
WebAppUninstallAndReplaceJob::~WebAppUninstallAndReplaceJob() = default;

void WebAppUninstallAndReplaceJob::Start() {
  DCHECK(to_app_lock_->registrar().IsInstalled(to_app_));

  std::vector<webapps::AppId> apps_to_replace;
  for (const webapps::AppId& from_app : from_apps_or_extensions_) {
    if (IsAppInstalled(&profile_.get(), from_app)) {
      apps_to_replace.emplace_back(from_app);
    }
  }

  if (apps_to_replace.empty()) {
    debug_value_.Set("did_uninstall_and_replace", false);
    std::move(on_complete_).Run(/*uninstall_triggered=*/false);
    return;
  }

  debug_value_.Set("did_uninstall_and_replace", true);
  MigrateUiAndUninstallApp(
      apps_to_replace.front(),
      base::BindOnce(std::move(on_complete_), /*uninstall_triggered=*/true));

  apps_to_replace.erase(apps_to_replace.begin());
  for (const auto& app : apps_to_replace) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(&profile_.get());
    proxy->UninstallSilently(app, apps::UninstallSource::kMigration);
  }
}

base::Value WebAppUninstallAndReplaceJob::ToDebugValue() const {
  return base::Value(debug_value_.Clone());
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

    to_app_lock_->sync_bridge().SetAppUserDisplayMode(
        to_app_, GetExtensionUserDisplayMode(&profile_.get(), from_extension),
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
    to_app_lock_->os_integration_manager().GetShortcutInfoForApp(
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
    ShortcutLocations locations) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(&profile_.get());

  const bool is_extension = proxy->AppRegistryCache().GetAppType(from_app) ==
                            apps::AppType::kChromeApp;
  if (is_extension) {
    // Need to be called before `proxy->UninstallSilently` because
    // UninstallSilently might synchronously finish, so the wait won't get
    // finished if called after.
    WaitForExtensionShortcutsDeleted(
        from_app,
        base::BindOnce(
            &WebAppUninstallAndReplaceJob::InstallOsHooksForReplacementApp,
            weak_ptr_factory_.GetWeakPtr(), std::move(on_complete), locations));
  }

  // When the `from_app` is a web app, we can't wait for it to finish because it
  // underlying schedules WebAppUninstallCommand which uses a `AllAppsLock`,
  // so the uninstall command won't get started until current command that holds
  // the `to_app_lock` finishes.
  proxy->UninstallSilently(from_app, apps::UninstallSource::kMigration);

  if (!is_extension) {
    InstallOsHooksForReplacementApp(std::move(on_complete), locations);
  }
}

void WebAppUninstallAndReplaceJob::InstallOsHooksForReplacementApp(
    base::OnceClosure on_complete,
    ShortcutLocations locations) {
  // This ensures that the os integration matches the app that we are replacing.
  InstallOsHooksOptions options;
  options.os_hooks[OsHookType::kShortcuts] =
      locations.on_desktop || locations.applications_menu_location ||
      locations.in_quick_launch_bar || locations.in_startup;
  options.add_to_desktop = locations.on_desktop;
  options.add_to_quick_launch_bar = locations.in_quick_launch_bar;

  ValueWithPolicy<RunOnOsLoginMode> run_on_os_login =
      to_app_lock_->registrar().GetAppRunOnOsLoginMode(to_app_);
  // Only update run on os login when it's not controlled by policy.
  if (run_on_os_login.user_controllable) {
    options.os_hooks[OsHookType::kRunOnOsLogin] = locations.in_startup;
    // TODO(crbug.com/1091964): Support Run on OS Login mode selection when
    // `from_app` is a web app.
    RunOnOsLoginMode new_mode = locations.in_startup
                                    ? RunOnOsLoginMode::kWindowed
                                    : RunOnOsLoginMode::kNotRun;
    if (new_mode != run_on_os_login.value) {
      {
        ScopedRegistryUpdate update = to_app_lock_->sync_bridge().BeginUpdate();
        update->UpdateApp(to_app_)->SetRunOnOsLoginMode(new_mode);
      }
    }
  }
  options.reason = SHORTCUT_CREATION_AUTOMATED;
  // TODO(crbug.com/1401125): Remove InstallOsHooks() once OS integration
  // sub managers have been implemented.
  auto os_hooks_barrier = OsIntegrationManager::GetBarrierForSynchronize(
      base::BindOnce(&WebAppUninstallAndReplaceJob::OnInstallOsHooksCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_complete)));
  to_app_lock_->os_integration_manager().InstallOsHooks(
      to_app_, os_hooks_barrier,
      /*web_app_info=*/nullptr, options);
  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = options.add_to_desktop;
  synchronize_options.add_to_quick_launch_bar = options.add_to_quick_launch_bar;
  synchronize_options.reason = options.reason;
  to_app_lock_->os_integration_manager().Synchronize(
      to_app_, base::BindOnce(os_hooks_barrier, OsHooksErrors()));
}

void WebAppUninstallAndReplaceJob::OnInstallOsHooksCompleted(
    base::OnceClosure on_complete,
    OsHooksErrors) {
  std::move(on_complete).Run();
}

}  // namespace web_app
