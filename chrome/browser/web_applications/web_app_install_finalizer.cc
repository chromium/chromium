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
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/components/web_app_system_web_app_data.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/file_handlers_permission_helper.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_installation_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

Source::Type InferSourceFromWebAppUninstallSource(
    webapps::WebappUninstallSource external_install_source) {
  switch (external_install_source) {
    case webapps::WebappUninstallSource::kAppList:
    case webapps::WebappUninstallSource::kAppMenu:
    case webapps::WebappUninstallSource::kAppManagement:
    case webapps::WebappUninstallSource::kAppsPage:
    case webapps::WebappUninstallSource::kMigration:
    case webapps::WebappUninstallSource::kOsSettings:
    case webapps::WebappUninstallSource::kSync:
    case webapps::WebappUninstallSource::kShelf:
    case webapps::WebappUninstallSource::kUnknown:
      return Source::kSync;

    case webapps::WebappUninstallSource::kExternalPreinstalled:
    case webapps::WebappUninstallSource::kInternalPreinstalled:
    case webapps::WebappUninstallSource::kPlaceholderReplacement:
      return Source::kDefault;

    case webapps::WebappUninstallSource::kExternalPolicy:
      return Source::kPolicy;

    case webapps::WebappUninstallSource::kSystemPreinstalled:
      return Source::kSystem;

    case webapps::WebappUninstallSource::kArc:
      return Source::kWebAppStore;
  }
}

webapps::WebappUninstallSource ConvertSourceTypeToWebAppUninstallSource(
    Source::Type source) {
  switch (source) {
    case Source::kDefault:
      return webapps::WebappUninstallSource::kExternalPreinstalled;

    case Source::kPolicy:
      return webapps::WebappUninstallSource::kExternalPolicy;

    case Source::kSync:
      return webapps::WebappUninstallSource::kInternalPreinstalled;

    case Source::kSystem:
      return webapps::WebappUninstallSource::kSystemPreinstalled;

    case Source::kWebAppStore:
      return webapps::WebappUninstallSource::kArc;
  }
}

}  // namespace

WebAppInstallFinalizer::SyncUninstallState::SyncUninstallState() = default;
WebAppInstallFinalizer::SyncUninstallState::~SyncUninstallState() = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::~FinalizeOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    const FinalizeOptions&) = default;

WebAppInstallFinalizer::WebAppInstallFinalizer(
    Profile* profile,
    WebAppIconManager* icon_manager,
    WebAppPolicyManager* policy_manager)
    : profile_(profile),
      icon_manager_(icon_manager),
      policy_manager_(policy_manager) {}

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
  web_app->SetStorageIsolated(web_app_info.is_storage_isolated);

  UpdateIntWebAppPref(profile_->GetPrefs(), app_id, kLatestWebAppInstallSource,
                      static_cast<int>(options.install_source));

  // TODO(crbug.com/897314): Store this as a display mode on WebApp to
  // participate in the DB transactional model.
  registry_controller().SetExperimentalTabbedWindowMode(
      app_id, web_app_info.enable_experimental_tabbed_window,
      /*is_user_action=*/false);

  file_handlers_helper_->WillInstallApp(web_app_info);

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id);

  SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                      std::move(commit_callback));
}

void WebAppInstallFinalizer::UninstallExternalWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource webapp_uninstall_source,
    UninstallWebAppCallback callback) {
  DCHECK(started_);

  DCHECK(webapp_uninstall_source ==
             webapps::WebappUninstallSource::kInternalPreinstalled ||
         webapp_uninstall_source ==
             webapps::WebappUninstallSource::kExternalPreinstalled ||
         webapp_uninstall_source ==
             webapps::WebappUninstallSource::kExternalPolicy ||
         webapp_uninstall_source ==
             webapps::WebappUninstallSource::kSystemPreinstalled ||
         webapp_uninstall_source ==
             webapps::WebappUninstallSource::kPlaceholderReplacement ||
         webapp_uninstall_source == webapps::WebappUninstallSource::kArc);

  Source::Type source =
      InferSourceFromWebAppUninstallSource(webapp_uninstall_source);
  DCHECK_NE(source, Source::Type::kSync);

  UninstallExternalWebAppOrRemoveSource(app_id, source, std::move(callback));
}

void WebAppInstallFinalizer::UninstallExternalWebAppByUrl(
    const GURL& app_url,
    webapps::WebappUninstallSource webapp_uninstall_source,
    UninstallWebAppCallback callback) {
  absl::optional<AppId> app_id = registrar().LookupExternalAppId(app_url);
  if (!app_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall web app with url " << app_url
                 << "; No corresponding web app for url.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*uninstalled=*/false));
    return;
  }

  UninstallExternalWebApp(app_id.value(), webapp_uninstall_source,
                          std::move(callback));
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
  DCHECK(
      webapp_uninstall_source == webapps::WebappUninstallSource::kUnknown ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kAppMenu ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kAppsPage ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kOsSettings ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kSync ||
      webapp_uninstall_source ==
          webapps::WebappUninstallSource::kAppManagement ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kMigration ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kAppList ||
      webapp_uninstall_source == webapps::WebappUninstallSource::kShelf);

  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  DCHECK(app);
  DCHECK(app->CanUserUninstallWebApp());

  if (app->IsPreinstalledApp()) {
    UpdateBoolWebAppPref(profile_->GetPrefs(), app_id,
                         kWasExternalAppUninstalledByUser, true);
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

void WebAppInstallFinalizer::UninstallFromSyncBeforeRegistryUpdate(
    std::vector<AppId> web_apps) {
  DCHECK(started_);

  for (auto& app_id : web_apps) {
    registrar().NotifyWebAppWillBeUninstalled(app_id);
    pending_sync_uninstalls_[app_id] = std::make_unique<SyncUninstallState>();
    os_integration_manager().UninstallAllOsHooks(
        app_id,
        base::BindOnce(&WebAppInstallFinalizer::OnSyncUninstallOsHooksUninstall,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
  }
}

void WebAppInstallFinalizer::UninstallFromSyncAfterRegistryUpdate(
    std::vector<std::unique_ptr<WebApp>> web_apps,
    RepeatingUninstallCallback repeating_callback) {
  DCHECK(started_);

  for (std::unique_ptr<WebApp>& web_app : web_apps) {
    const AppId& app_id = web_app->app_id();
    DCHECK(base::Contains(pending_sync_uninstalls_, app_id));
    auto& uninstall_state = pending_sync_uninstalls_[app_id];
    uninstall_state->callback = base::BindOnce(repeating_callback, app_id);
    uninstall_state->web_app = std::move(web_app);

    DCHECK(started_);
    // WebAppSyncBridge::ApplySyncChangesToRegistrar does the actual
    // NotifyWebAppWillBeUninstalled and unregistration of the app from the
    // registry, and MaybeFinishSyncUninstall does the NotifyWebAppUninstalled.
    DCHECK(!GetWebAppRegistrar().GetAppById(app_id));

    icon_manager_->DeleteData(
        app_id,
        base::BindOnce(&WebAppInstallFinalizer::OnSyncUninstallAppDataDeleted,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
  }
}

bool WebAppInstallFinalizer::WasPreinstalledWebAppUninstalled(
    const AppId& app_id) const {
  return GetBoolWebAppPref(profile_->GetPrefs(), app_id,
                           kWasExternalAppUninstalledByUser);
}

bool WebAppInstallFinalizer::CanReparentTab(const AppId& app_id,
                                            bool shortcut_created) const {
  // Reparent the web contents into its own window only if that is the
  // app's launch type.
  DCHECK(registrar_);
  if (registrar_->GetAppUserDisplayMode(app_id) != DisplayMode::kStandalone)
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
    const WebApplicationInfo& web_app_info,
    content::WebContents* web_contents,
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
                                  InstallResultCode::kWebAppDisabled));
    return;
  }

  bool should_update_os_hooks = ShouldUpdateOsHooks(app_id);

  FileHandlerUpdateAction file_handlers_need_os_update =
      file_handlers_helper_->WillUpdateApp(app_id, web_app_info);

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id,
      existing_web_app->name(), should_update_os_hooks,
      file_handlers_need_os_update, web_app_info);

  // Prepare copy-on-write to update existing app.
  SetWebAppManifestFieldsAndWriteData(
      web_app_info, std::make_unique<WebApp>(*existing_web_app),
      std::move(commit_callback));
}

void WebAppInstallFinalizer::Start() {
  DCHECK(!started_);

  file_handlers_helper_ = std::make_unique<FileHandlersPermissionHelper>(this);
  started_ = true;
}

void WebAppInstallFinalizer::Shutdown() {
  file_handlers_helper_.reset();
  started_ = false;
}

void WebAppInstallFinalizer::SetRemoveSourceCallbackForTesting(
    base::RepeatingCallback<void(const AppId&)> callback) {
  install_source_removed_callback_for_testing_ = std::move(callback);
}

void WebAppInstallFinalizer::SetSubsystems(
    WebAppRegistrar* registrar,
    WebAppUiManager* ui_manager,
    AppRegistryController* registry_controller,
    OsIntegrationManager* os_integration_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  registry_controller_ = registry_controller;
  os_integration_manager_ = os_integration_manager;
}

void WebAppInstallFinalizer::UninstallWebAppInternal(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback) {
  // If the app is already uninstalling then avoid triggering another uninstall.
  {
    ScopedRegistryUpdate update(registry_controller().AsWebAppSyncBridge());
    WebApp* app = update->UpdateApp(app_id);
    if (!app || app->is_uninstalling()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    /*uninstalled=*/false));
      return;
    }
    // Set uninstalling flag and continue with app uninstall.
    app->SetIsUninstalling(true);
  }
  registrar().NotifyWebAppWillBeUninstalled(app_id);
  os_integration_manager().UninstallAllOsHooks(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnUninstallOsHooks,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             uninstall_source, std::move(callback)));
}

void WebAppInstallFinalizer::OnSyncUninstallOsHooksUninstall(
    AppId app_id,
    OsHooksResults results) {
  DCHECK(base::Contains(pending_sync_uninstalls_, app_id));
  auto& uninstall_state = pending_sync_uninstalls_[app_id];
  uninstall_state->hooks_uninstalled = true;
  uninstall_state->success = uninstall_state->success && results.all();
  MaybeFinishSyncUninstall(app_id);
}

void WebAppInstallFinalizer::OnSyncUninstallAppDataDeleted(AppId app_id,
                                                           bool success) {
  DCHECK(base::Contains(pending_sync_uninstalls_, app_id));
  auto& uninstall_state = pending_sync_uninstalls_[app_id];
  uninstall_state->app_data_deleted = true;
  uninstall_state->success = uninstall_state->success && success;
  MaybeFinishSyncUninstall(app_id);
}

void WebAppInstallFinalizer::MaybeFinishSyncUninstall(AppId app_id) {
  DCHECK(base::Contains(pending_sync_uninstalls_, app_id));
  auto& uninstall_state = pending_sync_uninstalls_[app_id];
  if (uninstall_state->hooks_uninstalled && uninstall_state->app_data_deleted) {
    UMA_HISTOGRAM_BOOLEAN("Webapp.SyncInitiatedUninstallResult",
                          uninstall_state->success);
    OnIconsDataDeletedAndWebAppUninstalled(
        app_id, webapps::WebappUninstallSource::kSync,
        std::move(uninstall_state->callback), uninstall_state->success);
    // WebApp object is destroyed here.
    pending_sync_uninstalls_.erase(app_id);
  }
}

void WebAppInstallFinalizer::OnUninstallOsHooks(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback,
    OsHooksResults os_hooks_info) {
  const WebApp* web_app = registrar().GetAppById(app_id);
  DCHECK(web_app);
  RemoveAppIsolationState(profile_->GetPrefs(),
                          url::Origin::Create(web_app->scope()));

  ScopedRegistryUpdate update(registry_controller().AsWebAppSyncBridge());
  update->DeleteApp(app_id);

  icon_manager_->DeleteData(
      app_id,
      base::BindOnce(
          &WebAppInstallFinalizer::OnIconsDataDeletedAndWebAppUninstalled,
          weak_ptr_factory_.GetWeakPtr(), app_id, uninstall_source,
          std::move(callback)));
}

void WebAppInstallFinalizer::UninstallExternalWebAppOrRemoveSource(
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
    webapps::WebappUninstallSource uninstall_source =
        ConvertSourceTypeToWebAppUninstallSource(source);
    UninstallWebAppInternal(app_id, uninstall_source, std::move(callback));
  } else {
    ScopedRegistryUpdate update(registry_controller().AsWebAppSyncBridge());
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->RemoveSource(source);
    if (install_source_removed_callback_for_testing_)
      install_source_removed_callback_for_testing_.Run(app_id);

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

  icon_manager_->WriteData(
      std::move(app_id), web_app_info.icon_bitmaps,
      web_app_info.shortcuts_menu_icon_bitmaps, web_app_info.other_icon_bitmaps,
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(commit_callback),
                     std::move(web_app)));
}

void WebAppInstallFinalizer::OnIconsDataWritten(
    CommitCallback commit_callback,
    std::unique_ptr<WebApp> web_app,
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
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback,
    bool success) {
  registrar().NotifyWebAppUninstalled(app_id);

  webapps::InstallableMetrics::TrackUninstallEvent(uninstall_source);

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

bool WebAppInstallFinalizer::ShouldUpdateOsHooks(const AppId& app_id) {
#if defined(OS_CHROMEOS)
  // OS integration should always be enabled on ChromeOS.
  return true;
#else
  // If the app being updated was installed by default and not also manually
  // installed by the user or an enterprise policy, disable os integration.
  return !registrar().WasInstalledByDefaultOnly(app_id);
#endif  // defined(OS_CHROMEOS)
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate(
    InstallFinalizedCallback callback,
    AppId app_id,
    std::string old_name,
    bool should_update_os_hooks,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebApplicationInfo& web_app_info,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  if (should_update_os_hooks) {
    os_integration_manager().UpdateOsHooks(
        app_id, old_name, file_handlers_need_os_update, web_app_info);
  }
  registrar().NotifyWebAppManifestUpdated(app_id, old_name);
  std::move(callback).Run(app_id, InstallResultCode::kSuccessAlreadyInstalled);
}

WebAppRegistrar& WebAppInstallFinalizer::GetWebAppRegistrar() const {
  return registrar();
}

}  // namespace web_app
