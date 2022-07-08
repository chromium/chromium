// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    webapps::WebappInstallSource install_surface)
    : source(ConvertInstallSurfaceToWebAppSource(install_surface)),
      install_surface(install_surface) {}

WebAppInstallFinalizer::FinalizeOptions::~FinalizeOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    const FinalizeOptions&) = default;

WebAppInstallFinalizer::WebAppInstallFinalizer(Profile* profile)
    : profile_(profile) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebAppInstallInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/1084939): Implement a before-start queue in
  // WebAppInstallManager and replace this runtime error in
  // WebAppInstallFinalizer with DCHECK(started_).
  if (!started_) {
    std::move(callback).Run(AppId(),
                            webapps::InstallResultCode::kWebAppProviderNotReady,
                            OsHooksErrors());
    return;
  }

  // TODO(loyso): Expose Source argument as a field of AppTraits struct.
  const WebAppManagement::Type source = options.source;

  AppId app_id =
      GenerateAppId(web_app_info.manifest_id, web_app_info.start_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);
  // A web app might be sync installed with id received from WebAppSpecifics
  // that's different from start_url hash, in this case we look up the app by
  // start_url and respect the app_id from the existing WebApp.
  if (!base::FeatureList::IsEnabled(blink::features::kWebAppEnableManifestId) &&
      !existing_web_app) {
    existing_web_app =
        GetWebAppRegistrar().GetAppByStartUrl(web_app_info.start_url);
  }
  std::unique_ptr<WebApp> web_app;
  if (existing_web_app) {
    app_id = existing_web_app->app_id();
    // Prepare copy-on-write:
    // Allows changing manifest_id and start_url when manifest_id is enabled.
    if (!base::FeatureList::IsEnabled(
            blink::features::kWebAppEnableManifestId)) {
      DCHECK_EQ(web_app_info.start_url, existing_web_app->start_url());
    }
    web_app = std::make_unique<WebApp>(*existing_web_app);

    // The UI may initiate a full install to overwrite the existing
    // non-locally-installed app. Therefore, |is_locally_installed| can be
    // promoted to |true|, but not vice versa.
    if (!web_app->is_locally_installed())
      web_app->SetIsLocallyInstalled(options.locally_installed);

    // There is a chance that existing sources type(s) are user uninstallable
    // but the newly added source type is NOT user uninstallable. In this
    // case, the following call will unregister os uninstallation.
    // TODO(https://crbug.com/1273270): This does NOT block installation, and
    // there is a possible edge case here where installation completes before
    // this os hook is written. The best place to fix this is to put this code
    // is where OS Hooks are called - however that is currently separate from
    // this class. See https://crbug.com/1273269.
    MaybeUnregisterOsUninstall(web_app.get(), source, *os_integration_manager_);
  } else {
    // New app.
    web_app = std::make_unique<WebApp>(app_id);
    web_app->SetStartUrl(web_app_info.start_url);
    web_app->SetManifestId(web_app_info.manifest_id);
    web_app->SetIsLocallyInstalled(options.locally_installed);
    if (options.locally_installed)
      web_app->SetInstallTime(base::Time::Now());
    web_app->SetRunOnOsLoginOsIntegrationState(RunOnOsLoginMode::kNotRun);
  }

  // Set |user_display_mode| and any user-controllable fields here if this
  // install is user initiated or it's a new app.
  if (webapps::InstallableMetrics::IsUserInitiatedInstallSource(
          options.install_surface) ||
      !existing_web_app) {
    DCHECK(web_app_info.user_display_mode.has_value());
    web_app->SetUserDisplayMode(*web_app_info.user_display_mode);
  }

  // `WebApp::chromeos_data` has a default value already. Only override if the
  // caller provided a new value.
  if (options.chromeos_data.has_value())
    web_app->SetWebAppChromeOsData(options.chromeos_data.value());

  if (policy_manager_->IsWebAppInDisabledList(app_id) &&
      web_app->chromeos_data().has_value() &&
      !web_app->chromeos_data()->is_disabled) {
    absl::optional<WebAppChromeOsData> cros_data = web_app->chromeos_data();
    cros_data->is_disabled = true;
    web_app->SetWebAppChromeOsData(std::move(cros_data));
  }

  // `WebApp::system_web_app_data` has a default value already. Only override if
  // the caller provided a new value.
  if (options.system_web_app_data.has_value()) {
    web_app->client_data()->system_web_app_data =
        options.system_web_app_data.value();
  }

  web_app->SetAdditionalSearchTerms(web_app_info.additional_search_terms);
  web_app->AddSource(source);
  web_app->SetIsFromSyncAndPendingInstallation(false);
  web_app->SetParentAppId(options.parent_app_id);
  web_app->SetInstallSourceForMetrics(options.install_surface);

  WriteExternalConfigMapInfo(*web_app, source, web_app_info.is_placeholder,
                             web_app_info.install_url);

  if (!options.locally_installed) {
    DCHECK(!(options.add_to_applications_menu || options.add_to_desktop ||
             options.add_to_quick_launch_bar))
        << "Cannot create os hooks for a non-locally installed ";
  }

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id, options);

  if (options.overwrite_existing_manifest_fields || !existing_web_app) {
    SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                        std::move(commit_callback));
  } else {
    // Updates the web app with an additional source.
    CommitToSyncBridge(std::move(web_app), std::move(commit_callback),
                       /*success=*/true);
  }
}

void WebAppInstallFinalizer::UninstallExternalWebApp(
    const AppId& app_id,
    WebAppManagement::Type external_install_source,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback) {
  DCHECK(started_);

  DCHECK(external_install_source == WebAppManagement::Type::kSystem ||
         external_install_source == WebAppManagement::Type::kPolicy ||
         external_install_source == WebAppManagement::Type::kSubApp ||
         external_install_source == WebAppManagement::Type::kWebAppStore ||
         external_install_source == WebAppManagement::Type::kDefault);

  UninstallExternalWebAppOrRemoveSource(app_id, external_install_source,
                                        uninstall_source, std::move(callback));
}

void WebAppInstallFinalizer::UninstallExternalWebAppByUrl(
    const GURL& app_url,
    WebAppManagement::Type external_install_source,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback) {
  absl::optional<AppId> app_id =
      GetWebAppRegistrar().LookupExternalAppId(app_url);
  if (!app_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall web app with url " << app_url
                 << "; No corresponding web app for url.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       webapps::UninstallResultCode::kNoAppToUninstall));
    return;
  }

  UninstallExternalWebApp(app_id.value(), external_install_source,
                          uninstall_source, std::move(callback));
}

bool WebAppInstallFinalizer::CanUserUninstallWebApp(const AppId& app_id) const {
  DCHECK(started_);

  // TODO(loyso): Policy Apps: Implement ManagementPolicy taking
  // extensions::ManagementPolicy::UserMayModifySettings as inspiration.
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  return app ? app->CanUserUninstallWebApp() : false;
}

void WebAppInstallFinalizer::UninstallWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource webapp_uninstall_source,
    UninstallWebAppCallback callback) {
  DCHECK(started_);

  // Check that the source was from a known 'user' or allowed ones such
  // as kMigration.
  // WebappUninstallSource::kSync should not be included in this list.
  DCHECK(
      webapp_uninstall_source == webapps::WebappUninstallSource::kUnknown ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kAppMenu ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kAppsPage ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kOsSettings ||
      webapp_uninstall_source ==
          webapps::WebappUninstallSource::kAppManagement ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kMigration ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kAppList ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kShelf ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kSubApp);

  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  DCHECK(app);
  DCHECK(app->CanUserUninstallWebApp());

  if (app->IsPreinstalledApp()) {
    // Update the default uninstalled web_app prefs if it is a preinstalled app
    // but being removed by user.
    const WebApp::ExternalConfigMap& config_map =
        app->management_to_external_config_map();
    auto it = config_map.find(WebAppManagement::kDefault);
    DCHECK(it != config_map.end());
    UserUninstalledPreinstalledWebAppPrefs(profile_->GetPrefs())
        .Add(app_id, it->second.install_urls);
  }

  // UninstallWebApp can wipe out an app with multiple sources. This
  // is the behavior from the old bookmark-app based system, which does not
  // support incremental AddSource/RemoveSource. Here we are preserving that
  // behavior for now.
  // TODO(loyso): Implement different uninstall flows in UI. For example, we
  // should separate UninstallWebAppFromSyncByUser from
  // UninstallWebApp.
  UninstallWebAppInternal(app_id, webapp_uninstall_source, std::move(callback));
}

void WebAppInstallFinalizer::RetryIncompleteUninstalls(
    const base::flat_set<AppId>& apps_to_uninstall) {
  for (const AppId& app_id : apps_to_uninstall) {
    command_manager_->ScheduleCommand(std::make_unique<WebAppUninstallCommand>(
        app_id,
        url::Origin::Create(registrar_->GetAppById(app_id)->start_url()),
        profile_, os_integration_manager_, sync_bridge_, icon_manager_,
        registrar_, install_manager_, this, translation_manager_,
        webapps::WebappUninstallSource::kStartupCleanup, base::DoNothing()));
  }
}

bool WebAppInstallFinalizer::CanReparentTab(const AppId& app_id,
                                            bool shortcut_created) const {
  // Reparent the web contents into its own window only if that is the
  // app's launch type.
  DCHECK(registrar_);
  if (registrar_->GetAppUserDisplayMode(app_id) == UserDisplayMode::kBrowser)
    return false;

  return ui_manager_->CanReparentAppTabToWindow(app_id, shortcut_created);
}

void WebAppInstallFinalizer::ReparentTab(const AppId& app_id,
                                         bool shortcut_created,
                                         content::WebContents* web_contents) {
  DCHECK(web_contents);
  return ui_manager_->ReparentAppTabToWindow(web_contents, app_id,
                                             shortcut_created);
}

void WebAppInstallFinalizer::FinalizeUpdate(
    const WebAppInstallInfo& web_app_info,
    InstallFinalizedCallback callback) {
  CHECK(started_);

  const AppId app_id =
      GenerateAppId(web_app_info.manifest_id, web_app_info.start_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);

  if (!existing_web_app ||
      existing_web_app->is_from_sync_and_pending_installation() ||
      app_id != existing_web_app->app_id()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  webapps::InstallResultCode::kWebAppDisabled,
                                  OsHooksErrors()));
    return;
  }

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id,
      GetWebAppRegistrar().GetAppShortName(app_id),
      GetFileHandlerUpdateAction(app_id, web_app_info), web_app_info);

  // Prepare copy-on-write to update existing app.
  SetWebAppManifestFieldsAndWriteData(
      web_app_info, std::make_unique<WebApp>(*existing_web_app),
      std::move(commit_callback));
}

void WebAppInstallFinalizer::Start() {
  DCHECK(!started_);
  started_ = true;
}

void WebAppInstallFinalizer::Shutdown() {
  started_ = false;
}

void WebAppInstallFinalizer::SetRemoveSourceCallbackForTesting(
    base::RepeatingCallback<void(const AppId&)> callback) {
  install_source_removed_callback_for_testing_ = std::move(callback);
}

void WebAppInstallFinalizer::SetSubsystems(
    WebAppInstallManager* install_manager,
    WebAppRegistrar* registrar,
    WebAppUiManager* ui_manager,
    WebAppSyncBridge* sync_bridge,
    OsIntegrationManager* os_integration_manager,
    WebAppIconManager* icon_manager,
    WebAppPolicyManager* policy_manager,
    WebAppTranslationManager* translation_manager,
    WebAppCommandManager* command_manager) {
  install_manager_ = install_manager;
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  sync_bridge_ = sync_bridge;
  os_integration_manager_ = os_integration_manager;
  icon_manager_ = icon_manager;
  policy_manager_ = policy_manager;
  translation_manager_ = translation_manager;
  command_manager_ = command_manager;
}

void WebAppInstallFinalizer::UninstallWebAppInternal(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback) {
  command_manager_->ScheduleCommand(std::make_unique<WebAppUninstallCommand>(
      app_id, url::Origin::Create(registrar_->GetAppById(app_id)->start_url()),
      profile_, os_integration_manager_, sync_bridge_, icon_manager_,
      registrar_, install_manager_, this, translation_manager_,
      uninstall_source, std::move(callback)));
}

void WebAppInstallFinalizer::UninstallExternalWebAppOrRemoveSource(
    const AppId& app_id,
    WebAppManagement::Type install_source,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback) {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  if (!app) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       webapps::UninstallResultCode::kNoAppToUninstall));
    return;
  }

  if (app->HasOnlySource(install_source)) {
    UninstallWebAppInternal(app_id, uninstall_source, std::move(callback));
  } else {
    // There is a chance that removed source type is NOT user uninstallable
    // but the remaining source (after removal) types are user uninstallable.
    // In this case, the following call will register os uninstallation.
    MaybeRegisterOsUninstall(
        app, install_source, *os_integration_manager_,
        base::BindOnce(&WebAppInstallFinalizer::OnMaybeRegisterOsUninstall,
                       weak_ptr_factory_.GetWeakPtr(), app_id, install_source,
                       std::move(callback)));
  }
}

void WebAppInstallFinalizer::OnMaybeRegisterOsUninstall(
    const AppId& app_id,
    WebAppManagement::Type source,
    UninstallWebAppCallback callback,
    OsHooksErrors os_hooks_errors) {
  ScopedRegistryUpdate update(sync_bridge_);
  WebApp* app_to_update = update->UpdateApp(app_id);
  app_to_update->RemoveSource(source);
  if (source == WebAppManagement::kSubApp) {
    app_to_update->SetParentAppId(absl::nullopt);
  }
  if (install_source_removed_callback_for_testing_)
    install_source_removed_callback_for_testing_.Run(app_id);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                webapps::UninstallResultCode::kSuccess));
}

void WebAppInstallFinalizer::SetWebAppManifestFieldsAndWriteData(
    const WebAppInstallInfo& web_app_info,
    std::unique_ptr<WebApp> web_app,
    CommitCallback commit_callback) {
  SetWebAppManifestFields(web_app_info, *web_app);

  AppId app_id = web_app->app_id();
  IconBitmaps icon_bitmaps = web_app_info.icon_bitmaps;
  ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps =
      web_app_info.shortcuts_menu_icon_bitmaps;
  IconsMap other_icon_bitmaps = web_app_info.other_icon_bitmaps;

  auto write_translations_callback = base::BindOnce(
      &WebAppInstallFinalizer::WriteTranslations,
      weak_ptr_factory_.GetWeakPtr(), app_id, std::move(web_app_info));
  auto commit_to_sync_bridge_callback =
      base::BindOnce(&WebAppInstallFinalizer::CommitToSyncBridge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app));

  icon_manager_->WriteData(
      app_id, std::move(icon_bitmaps), std::move(shortcuts_menu_icon_bitmaps),
      std::move(other_icon_bitmaps),
      base::BindOnce(std::move(write_translations_callback),
                     base::BindOnce(std::move(commit_to_sync_bridge_callback),
                                    std::move(commit_callback))));
}

void WebAppInstallFinalizer::WriteTranslations(
    const AppId& app_id,
    const WebAppInstallInfo& web_app_info,
    CommitCallback commit_callback,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  translation_manager_->WriteTranslations(app_id, web_app_info.translations,
                                          std::move(commit_callback));
}

void WebAppInstallFinalizer::CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                                                CommitCallback commit_callback,
                                                bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  // Save the isolation state to prefs. On browser startup we may need access
  // to the isolation state before WebAppDatabase has finished loading, so we
  // duplicate this state in a pref to prevent blocking startup.
  RecordOrRemoveAppIsolationState(profile_->GetPrefs(), *web_app);

  AppId app_id = web_app->app_id();

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge_->BeginUpdate();

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override)
    *app_to_override = std::move(*web_app);
  else
    update->CreateApp(std::move(web_app));

  sync_bridge_->CommitUpdate(std::move(update), std::move(commit_callback));
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall(
    InstallFinalizedCallback callback,
    AppId app_id,
    FinalizeOptions finalize_options,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(
        AppId(), webapps::InstallResultCode::kWriteDataFailed, OsHooksErrors());
    return;
  }

  install_manager_->NotifyWebAppInstalled(app_id);

  const WebApp* web_app = GetWebAppRegistrar().GetAppById(app_id);
  // TODO(dmurph): Verify this check is not needed and remove after
  // isolation work is done. https://crbug.com/1298130
  if (!web_app) {
    std::move(callback).Run(
        AppId(), webapps::InstallResultCode::kAppNotInRegistrarAfterCommit,
        OsHooksErrors());
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)  // Deeper OS integration is expected on ChromeOS.
  const bool should_install_os_hooks = !finalize_options.bypass_os_hooks;
#else
  const bool should_install_os_hooks =
      !finalize_options.bypass_os_hooks &&
      !web_app->HasOnlySource(WebAppManagement::Type::kDefault) &&
      finalize_options.locally_installed;
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (!should_install_os_hooks) {
    std::move(callback).Run(app_id,
                            webapps::InstallResultCode::kSuccessNewInstall,
                            OsHooksErrors());
    return;
  }

  InstallOsHooksOptions hooks_options;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  hooks_options.os_hooks[OsHookType::kUrlHandlers] = true;
#else
  hooks_options.os_hooks[OsHookType::kUrlHandlers] = false;
#endif

  hooks_options.os_hooks[OsHookType::kShortcuts] =
      finalize_options.add_to_applications_menu;
  hooks_options.os_hooks[OsHookType::kShortcutsMenu] =
      finalize_options.add_to_applications_menu;

  {
    RunOnOsLoginMode current_mode =
        registrar_->GetAppRunOnOsLoginMode(app_id).value;
    hooks_options.os_hooks[OsHookType::kRunOnOsLogin] =
        current_mode == RunOnOsLoginMode::kWindowed;
  }

  hooks_options.add_to_quick_launch_bar =
      finalize_options.add_to_quick_launch_bar;
  hooks_options.add_to_desktop = finalize_options.add_to_desktop;

  // Apps that can't be uninstalled from users shouldn't register to
  // OS Settings.
  hooks_options.os_hooks[OsHookType::kUninstallationViaOsSettings] =
      web_app->CanUserUninstallWebApp();

  hooks_options.os_hooks[OsHookType::kFileHandlers] = true;
  hooks_options.os_hooks[OsHookType::kProtocolHandlers] = true;

  os_integration_manager_->InstallOsHooks(
      app_id,
      base::BindOnce(&WebAppInstallFinalizer::OnInstallHooksFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_id),
      /*web_app_info=*/nullptr, hooks_options);
}

void WebAppInstallFinalizer::OnInstallHooksFinished(
    InstallFinalizedCallback callback,
    AppId app_id,
    OsHooksErrors os_hooks_errors) {
  auto joined = std::move(callback).Then(
      base::BindOnce(&WebAppInstallFinalizer::NotifyWebAppInstalledWithOsHooks,
                     weak_ptr_factory_.GetWeakPtr(), app_id));

  std::move(joined).Run(app_id, webapps::InstallResultCode::kSuccessNewInstall,
                        os_hooks_errors);
}

void WebAppInstallFinalizer::NotifyWebAppInstalledWithOsHooks(AppId app_id) {
  install_manager_->NotifyWebAppInstalledWithOsHooks(app_id);
}

bool WebAppInstallFinalizer::ShouldUpdateOsHooks(const AppId& app_id) {
#if BUILDFLAG(IS_CHROMEOS)
  // OS integration should always be enabled on ChromeOS.
  return true;
#else
  // If the app being updated was installed by default and not also manually
  // installed by the user or an enterprise policy, disable os integration.
  return !GetWebAppRegistrar().WasInstalledByDefaultOnly(app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate(
    InstallFinalizedCallback callback,
    AppId app_id,
    std::string old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebAppInstallInfo& web_app_info,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(
        AppId(), webapps::InstallResultCode::kWriteDataFailed, OsHooksErrors());
    return;
  }

  if (ShouldUpdateOsHooks(app_id)) {
    os_integration_manager_->UpdateOsHooks(
        app_id, old_name, file_handlers_need_os_update, web_app_info,
        base::BindOnce(&WebAppInstallFinalizer::OnUpdateHooksFinished,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       app_id, old_name));
  } else {
    std::move(callback).Run(
        app_id, webapps::InstallResultCode::kSuccessAlreadyInstalled,
        OsHooksErrors());
  }
}

void WebAppInstallFinalizer::OnUpdateHooksFinished(
    InstallFinalizedCallback callback,
    AppId app_id,
    std::string old_name,
    OsHooksErrors os_hooks_errors) {
  install_manager_->NotifyWebAppManifestUpdated(app_id, old_name);

  std::move(callback).Run(
      app_id,
      os_hooks_errors.any()
          ? webapps::InstallResultCode::kUpdateTaskFailed
          : webapps::InstallResultCode::kSuccessAlreadyInstalled,
      os_hooks_errors);
}

const WebAppRegistrar& WebAppInstallFinalizer::GetWebAppRegistrar() const {
  return *registrar_;
}

void WebAppInstallFinalizer::WriteExternalConfigMapInfo(
    WebApp& web_app,
    WebAppManagement::Type source,
    bool is_placeholder,
    GURL install_url) {
  DCHECK(!(source == WebAppManagement::Type::kSync && is_placeholder));
  if (source != WebAppManagement::Type::kSync) {
    web_app.AddPlaceholderInfoToManagementExternalConfigMap(source,
                                                            is_placeholder);
    if (install_url.is_valid()) {
      web_app.AddInstallURLToManagementExternalConfigMap(source, install_url);
    }
  }
}

FileHandlerUpdateAction WebAppInstallFinalizer::GetFileHandlerUpdateAction(
    const AppId& app_id,
    const WebAppInstallInfo& new_web_app_info) {
  if (!os_integration_manager_->IsFileHandlingAPIAvailable(app_id))
    return FileHandlerUpdateAction::kNoUpdate;

  if (GetWebAppRegistrar().GetAppFileHandlerApprovalState(app_id) ==
      ApiApprovalState::kDisallowed) {
    return FileHandlerUpdateAction::kNoUpdate;
  }

  // TODO(https://crbug.com/1197013): Consider trying to re-use the comparison
  // results from the ManifestUpdateTask.
  const apps::FileHandlers* old_handlers =
      GetWebAppRegistrar().GetAppFileHandlers(app_id);
  DCHECK(old_handlers);
  if (*old_handlers == new_web_app_info.file_handlers)
    return FileHandlerUpdateAction::kNoUpdate;

  return FileHandlerUpdateAction::kUpdate;
}

}  // namespace web_app
