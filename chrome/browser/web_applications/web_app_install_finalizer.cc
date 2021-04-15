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
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/components/web_app_system_web_app_data.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_installation_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

// TODO(loyso): Call sites should specify Source explicitly as a part of
// AppTraits parameter object.
Source::Type InferSourceFromMetricsInstallSource(
    webapps::WebappInstallSource install_source) {
  switch (install_source) {
    case webapps::WebappInstallSource::MENU_BROWSER_TAB:
    case webapps::WebappInstallSource::MENU_CUSTOM_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case webapps::WebappInstallSource::API_BROWSER_TAB:
    case webapps::WebappInstallSource::API_CUSTOM_TAB:
    case webapps::WebappInstallSource::DEVTOOLS:
    case webapps::WebappInstallSource::MANAGEMENT_API:
    case webapps::WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case webapps::WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case webapps::WebappInstallSource::SYNC:
    case webapps::WebappInstallSource::MENU_CREATE_SHORTCUT:
      return Source::kSync;

    case webapps::WebappInstallSource::INTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_DEFAULT:
      return Source::kDefault;

    case webapps::WebappInstallSource::EXTERNAL_POLICY:
      return Source::kPolicy;

    case webapps::WebappInstallSource::SYSTEM_DEFAULT:
      return Source::kSystem;

    case webapps::WebappInstallSource::ARC:
      return Source::kWebAppStore;

    case webapps::WebappInstallSource::COUNT:
      NOTREACHED();
      return Source::kSync;
  }
}

Source::Type InferSourceFromExternalInstallSource(
    ExternalInstallSource external_install_source) {
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
    case ExternalInstallSource::kExternalDefault:
      return Source::kDefault;

    case ExternalInstallSource::kExternalPolicy:
      return Source::kPolicy;

    case ExternalInstallSource::kSystemInstalled:
      return Source::kSystem;

    case ExternalInstallSource::kArc:
      return Source::kWebAppStore;
  }
}

}  // namespace

WebAppInstallFinalizer::WebAppInstallFinalizer(
    Profile* profile,
    WebAppIconManager* icon_manager,
    std::unique_ptr<InstallFinalizer> legacy_finalizer)
    : legacy_finalizer_(std::move(legacy_finalizer)),
      profile_(profile),
      icon_manager_(icon_manager) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/1084939): Implement a before-start queue in
  // WebAppInstallManager and replace this runtime error in
  // WebAppInstallFinalizer with DCHECK(started_).
  if (!started_) {
    std::move(callback).Run(AppId(),
                            InstallResultCode::kWebAppProviderNotReady);
    return;
  }

  // TODO(loyso): Expose Source argument as a field of AppTraits struct.
  const auto source =
      InferSourceFromMetricsInstallSource(options.install_source);

  AppId app_id =
      GenerateAppId(web_app_info.manifest_id, web_app_info.start_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);
  // A web app might be sync installed with id received from WebAppSpecifics
  // that's different from start_url hash, in this case we look up the app by
  // start_url and respect the app_id from the existing WebApp.
  if (!existing_web_app)
    existing_web_app =
        GetWebAppRegistrar().GetAppByStartUrl(web_app_info.start_url);
  std::unique_ptr<WebApp> web_app;
  if (existing_web_app) {
    app_id = existing_web_app->app_id();
    // Prepare copy-on-write:
    DCHECK_EQ(web_app_info.start_url, existing_web_app->start_url());
    web_app = std::make_unique<WebApp>(*existing_web_app);

    // The UI may initiate a full install to overwrite the existing
    // non-locally-installed app. Therefore, |is_locally_installed| can be
    // promoted to |true|, but not vice versa.
    if (!web_app->is_locally_installed())
      web_app->SetIsLocallyInstalled(options.locally_installed);
  } else {
    // New app.
    web_app = std::make_unique<WebApp>(app_id);
    web_app->SetStartUrl(web_app_info.start_url);
    web_app->SetManifestId(web_app_info.manifest_id);
    web_app->SetIsLocallyInstalled(options.locally_installed);
    if (options.locally_installed)
      web_app->SetInstallTime(base::Time::Now());
  }

  // Set |user_display_mode| and any user-controllable fields here if this
  // install is user initiated or it's a new app.
  if (webapps::InstallableMetrics::IsUserInitiatedInstallSource(
          options.install_source) ||
      !existing_web_app) {
    web_app->SetUserDisplayMode(web_app_info.open_as_window
                                    ? DisplayMode::kStandalone
                                    : DisplayMode::kBrowser);
  }

  // `WebApp::chromeos_data` has a default value already. Only override if the
  // caller provided a new value.
  if (options.chromeos_data.has_value())
    web_app->SetWebAppChromeOsData(options.chromeos_data.value());

  // `WebApp::system_web_app_data` has a default value already. Only override if
  // the caller provided a new value.
  if (options.system_web_app_data.has_value()) {
    web_app->client_data()->system_web_app_data =
        options.system_web_app_data.value();
  }

  web_app->SetAdditionalSearchTerms(web_app_info.additional_search_terms);
  web_app->AddSource(source);
  web_app->SetIsInSyncInstall(false);
  const bool is_synced = web_app->IsSynced();

  UpdateIntWebAppPref(profile_->GetPrefs(), app_id, kLatestWebAppInstallSource,
                      static_cast<int>(options.install_source));

  // TODO(crbug.com/897314): Store this as a display mode on WebApp to
  // participate in the DB transactional model.
  registry_controller().SetExperimentalTabbedWindowMode(
      app_id, web_app_info.enable_experimental_tabbed_window,
      /*is_user_action=*/false);

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id);

  SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                      std::move(commit_callback));

  // Backward compatibility: If a legacy finalizer was provided then install a
  // duplicate bookmark app in the extensions registry. No callback, this is
  // fire-and-forget install. If a user gets switched back to legacy mode they
  // still able to use the duplicate.
  //
  // We should install shadow bookmark app only for kSync source (we sync only
  // user-installed apps). System, Policy, WebAppStore, Default apps should not
  // get a shadow bookmark app.
  if (legacy_finalizer_ && is_synced &&
      base::FeatureList::IsEnabled(features::kSyncBookmarkApps)) {
    legacy_finalizer_->FinalizeInstall(web_app_info, options,
                                       base::DoNothing());
  }
}

void WebAppInstallFinalizer::FinalizeUninstallAfterSync(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  DCHECK(started_);
  // WebAppSyncBridge::ApplySyncChangesToRegistrar does the actual
  // NotifyWebAppWillBeUninstalled and unregistration of the app from the
  // registry.
  DCHECK(!GetWebAppRegistrar().GetAppById(app_id));

  icon_manager_->DeleteData(
      app_id,
      base::BindOnce(
          &WebAppInstallFinalizer::OnIconsDataDeletedAndWebAppUninstalled,
          weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback)));
}

void WebAppInstallFinalizer::UninstallExternalWebApp(
    const AppId& app_id,
    ExternalInstallSource external_install_source,
    UninstallWebAppCallback callback) {
  DCHECK(started_);
  Source::Type source =
      InferSourceFromExternalInstallSource(external_install_source);
  UninstallWebAppOrRemoveSource(app_id, source, std::move(callback));
}

bool WebAppInstallFinalizer::CanUserUninstallExternalApp(
    const AppId& app_id) const {
  DCHECK(started_);
  // TODO(loyso): Policy Apps: Implement ManagementPolicy taking
  // extensions::ManagementPolicy::UserMayModifySettings as inspiration.
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  return app ? app->CanUserUninstallExternalApp() : false;
}

void WebAppInstallFinalizer::UninstallExternalAppByUser(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  DCHECK(started_);

  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  DCHECK(app);
  DCHECK(app->CanUserUninstallExternalApp());
  const bool is_synced = app->IsSynced();

  if (app->IsDefaultApp()) {
    UpdateBoolWebAppPref(profile_->GetPrefs(), app_id,
                         kWasExternalAppUninstalledByUser, true);
  }

  // UninstallExternalAppByUser can wipe out an app with multiple sources. This
  // is the behavior from the old bookmark-app based system, which does not
  // support incremental AddSource/RemoveSource. Here we are preserving that
  // behavior for now.
  // TODO(loyso): Implement different uninstall flows in UI. For example, we
  // should separate UninstallWebAppFromSyncByUser from
  // UninstallExternalAppByUser.
  UninstallWebApp(app_id, std::move(callback));

  // Uninstall shadow bookmark app from this device and from the sync server.
  if (legacy_finalizer_ && is_synced)
    legacy_finalizer_->UninstallExternalAppByUser(app_id, base::DoNothing());
}

bool WebAppInstallFinalizer::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  return GetBoolWebAppPref(profile_->GetPrefs(), app_id,
                           kWasExternalAppUninstalledByUser);
}

void WebAppInstallFinalizer::FinalizeUpdate(
    const WebApplicationInfo& web_app_info,
    content::WebContents* web_contents,
    InstallFinalizedCallback callback) {
  CHECK(started_);

  const AppId app_id =
      GenerateAppId(web_app_info.manifest_id, web_app_info.start_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);

  if (!existing_web_app || existing_web_app->is_in_sync_install() ||
      web_app_info.start_url != existing_web_app->start_url()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  InstallResultCode::kWebAppDisabled));
    return;
  }

  bool file_handlers_need_os_update =
      DoFileHandlersNeedOsUpdate(app_id, web_app_info, web_contents);
  // Grab the shortcut info before the app is removed from the database.
  os_integration_manager().GetShortcutInfoForApp(
      app_id, base::BindOnce(
                  &WebAppInstallFinalizer::FinalizeUpdateWithShortcutInfo,
                  weak_ptr_factory_.GetWeakPtr(), file_handlers_need_os_update,
                  std::move(callback), app_id, web_app_info));
}

void WebAppInstallFinalizer::RemoveLegacyInstallFinalizerForTesting() {
  legacy_finalizer_ = nullptr;
}

void WebAppInstallFinalizer::Start() {
  DCHECK(!started_);
  started_ = true;
}

void WebAppInstallFinalizer::Shutdown() {
  started_ = false;
}

void WebAppInstallFinalizer::UninstallWebApp(const AppId& app_id,
                                             UninstallWebAppCallback callback) {

  registrar().NotifyWebAppWillBeUninstalled(app_id);
  os_integration_manager().UninstallAllOsHooks(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnUninstallOsHooks,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             std::move(callback)));
}

void WebAppInstallFinalizer::OnUninstallOsHooks(
    const AppId& app_id,
    UninstallWebAppCallback callback,
    OsHooksResults os_hooks_info) {
  ScopedRegistryUpdate update(registry_controller().AsWebAppSyncBridge());
  update->DeleteApp(app_id);

  icon_manager_->DeleteData(
      app_id,
      base::BindOnce(
          &WebAppInstallFinalizer::OnIconsDataDeletedAndWebAppUninstalled,
          weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback)));
}

void WebAppInstallFinalizer::UninstallWebAppOrRemoveSource(
    const AppId& app_id,
    Source::Type source,
    UninstallWebAppCallback callback) {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  if (!app) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*uninstalled=*/false));
    return;
  }

  if (app->HasOnlySource(source)) {
    UninstallWebApp(app_id, std::move(callback));
  } else {
    ScopedRegistryUpdate update(registry_controller().AsWebAppSyncBridge());
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->RemoveSource(source);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*uninstalled=*/true));
  }
}

void WebAppInstallFinalizer::SetWebAppManifestFieldsAndWriteData(
    const WebApplicationInfo& web_app_info,
    std::unique_ptr<WebApp> web_app,
    CommitCallback commit_callback) {
  SetWebAppManifestFields(web_app_info, *web_app);

  AppId app_id = web_app->app_id();
  IconBitmaps icon_bitmaps;
  icon_bitmaps.any = web_app_info.icon_bitmaps.any;
  icon_bitmaps.maskable = web_app_info.icon_bitmaps.maskable;
  icon_manager_->WriteData(
      std::move(app_id), std::move(icon_bitmaps),
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(commit_callback),
                     std::move(web_app),
                     web_app_info.shortcuts_menu_icon_bitmaps));
}

void WebAppInstallFinalizer::OnIconsDataWritten(
    CommitCallback commit_callback,
    std::unique_ptr<WebApp> web_app,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  if (shortcuts_menu_icon_bitmaps.empty()) {
    OnShortcutsMenuIconsDataWritten(std::move(commit_callback),
                                    std::move(web_app), success);
  } else {
    AppId app_id = web_app->app_id();
    icon_manager_->WriteShortcutsMenuIconsData(
        app_id, shortcuts_menu_icon_bitmaps,
        base::BindOnce(&WebAppInstallFinalizer::OnShortcutsMenuIconsDataWritten,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(commit_callback), std::move(web_app)));
  }
}

void WebAppInstallFinalizer::OnShortcutsMenuIconsDataWritten(
    CommitCallback commit_callback,
    std::unique_ptr<WebApp> web_app,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  AppId app_id = web_app->app_id();

  std::unique_ptr<WebAppRegistryUpdate> update =
      registry_controller().AsWebAppSyncBridge()->BeginUpdate();

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override)
    *app_to_override = std::move(*web_app);
  else
    update->CreateApp(std::move(web_app));

  registry_controller().AsWebAppSyncBridge()->CommitUpdate(
      std::move(update), std::move(commit_callback));
}

void WebAppInstallFinalizer::OnIconsDataDeletedAndWebAppUninstalled(
    const AppId& app_id,
    UninstallWebAppCallback callback,
    bool success) {
  registrar().NotifyWebAppUninstalled(app_id);
  std::move(callback).Run(success);
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall(
    InstallFinalizedCallback callback,
    AppId app_id,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  registrar().NotifyWebAppInstalled(app_id);
  std::move(callback).Run(app_id, InstallResultCode::kSuccessNewInstall);
}

void WebAppInstallFinalizer::FinalizeUpdateWithShortcutInfo(
    bool file_handlers_need_os_update,
    InstallFinalizedCallback callback,
    const AppId app_id,
    const WebApplicationInfo& web_app_info,
    std::unique_ptr<ShortcutInfo> old_shortcut) {
  // Prepare copy-on-write to update existing app.
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);
  auto web_app = std::make_unique<WebApp>(*existing_web_app);
  const bool is_synced = web_app->IsSynced();
  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id,
      existing_web_app->name(), std::move(old_shortcut),
      file_handlers_need_os_update, web_app_info);

  SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                      std::move(commit_callback));

  if (legacy_finalizer_ && is_synced &&
      base::FeatureList::IsEnabled(features::kSyncBookmarkApps)) {
    // Passing an empty web_contents because BookmarkApps should not need this.
    legacy_finalizer_->FinalizeUpdate(web_app_info, nullptr, base::DoNothing());
  }
}

bool WebAppInstallFinalizer::DoFileHandlersNeedOsUpdate(
    const AppId app_id,
    const WebApplicationInfo& web_app_info,
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI))
    return false;
  // TODO(https://crbug.com/1197013): Consider trying to re-use
  // HaveFileHandlersChanged() results from the ManifestUpdateTask.
  if (!HaveFileHandlersChanged(
          /*old_handlers=*/registrar().GetAppFileHandlers(app_id),
          /*new_handlers=*/web_app_info.file_handlers)) {
    return false;
  }

  const GURL& url = web_app_info.scope;

  // Keep in sync with chromeos::kChromeUIMediaAppURL.
  const char kChromeUIMediaAppURL[] = "chrome://media-app/";
  // Keep in sync with chromeos::kChromeUICameraAppURL.
  const char kChromeUICameraAppURL[] = "chrome://camera-app/";

  // Omit file handler permission downgrade for the ChromeOS Media and Camera
  // System Web Apps (SWAs), which have permissions granted by default.
  // TODO(huangdarwin): Find a better architecture to structure this exception
  // and check relevant only in ChromeOS (outside of LaCrOS).
  if (url == kChromeUIMediaAppURL || url == kChromeUICameraAppURL) {
    return true;
  }

  // Downgrade file handlers permission before
  // OsIntegrationManager::UpdateOsHooks(), as `web_contents` may no
  // longer exist by the time we reach OsIntegrationManager.
  //
  // It's possible we'll downgrade the permission and then fail to update OS
  // integrations (ex. if the disk or icon downloads fail), but this is ok
  // because these failures should rarely occur.
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(permission_manager);
  // Note: Using GetPermissionStatus instead of GetPermissionStatusForFrame()
  // because the relevant frame isn't available here.
  permissions::PermissionResult status =
      permission_manager->GetPermissionStatus(
          ContentSettingsType::FILE_HANDLING, url, url);

  // If file handling permission is "ALLOW" during manifest update, downgrade
  // to "ASK" via reset, as the user may not want to allow newly added file
  // handlers, which may include more dangerous extensions.
  // If the permission is "ASK" or "BLOCK", leave it as is. To avoid misuse, the
  // user must re-install the PWA to enable previously blocked file handlers.
  if (status.content_setting == CONTENT_SETTING_ALLOW) {
    permission_manager->ResetPermission(content::PermissionType::FILE_HANDLING,
                                        url, url);
  } else if (status.content_setting == CONTENT_SETTING_BLOCK) {
    // TODO(https://crbug.com/1194163): CONTENT_SETTING_BLOCK should block
    // update to avoid re-registering file handlers after they're unregistered.
    // Implement unregistration of file handlers when "BLOCK" is set.
    return false;
  }
  return true;
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate(
    InstallFinalizedCallback callback,
    AppId app_id,
    std::string old_name,
    std::unique_ptr<ShortcutInfo> old_shortcut,
    bool file_handlers_need_os_update,
    const WebApplicationInfo& web_app_info,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  os_integration_manager().UpdateOsHooks(
      app_id, old_name, std::move(old_shortcut), file_handlers_need_os_update,
      web_app_info);
  registrar().NotifyWebAppManifestUpdated(app_id, old_name);
  std::move(callback).Run(app_id, InstallResultCode::kSuccessAlreadyInstalled);
}

WebAppRegistrar& WebAppInstallFinalizer::GetWebAppRegistrar() const {
  WebAppRegistrar* web_app_registrar = registrar().AsWebAppRegistrar();
  DCHECK(web_app_registrar);
  return *web_app_registrar;
}

}  // namespace web_app
