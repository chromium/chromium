// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <utility>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_installation_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

// TODO(loyso): Call sites should specify Source explicitly as a part of
// AppTraits parameter object.
Source::Type InferSourceFromMetricsInstallSource(
    WebappInstallSource install_source) {
  switch (install_source) {
    case WebappInstallSource::MENU_BROWSER_TAB:
    case WebappInstallSource::MENU_CUSTOM_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case WebappInstallSource::API_BROWSER_TAB:
    case WebappInstallSource::API_CUSTOM_TAB:
    case WebappInstallSource::DEVTOOLS:
    case WebappInstallSource::MANAGEMENT_API:
    case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case WebappInstallSource::SYNC:
    case WebappInstallSource::MENU_CREATE_SHORTCUT:
      return Source::kSync;

    case WebappInstallSource::INTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_DEFAULT:
      return Source::kDefault;

    case WebappInstallSource::EXTERNAL_POLICY:
      return Source::kPolicy;

    case WebappInstallSource::SYSTEM_DEFAULT:
      return Source::kSystem;

    case WebappInstallSource::ARC:
      return Source::kWebAppStore;

    case WebappInstallSource::COUNT:
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

  const AppId app_id = GenerateAppIdFromURL(web_app_info.start_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);

  std::unique_ptr<WebApp> web_app;

  if (existing_web_app) {
    // There is an existing app from other source(s). Preserve
    // |user_display_mode| and any user-controllable fields here, do not modify
    // them. Prepare copy-on-write:
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
    web_app->SetIsLocallyInstalled(options.locally_installed);
    web_app->SetUserDisplayMode(web_app_info.open_as_window
                                    ? DisplayMode::kStandalone
                                    : DisplayMode::kBrowser);
    if (options.locally_installed)
      web_app->SetInstallTime(base::Time::Now());
  }

  // `WebApp::chromeos_data` has a default value already. Only override if the
  // caller provided a new value.
  if (options.chromeos_data.has_value())
    web_app->SetWebAppChromeOsData(options.chromeos_data.value());

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
  if (legacy_finalizer_ && is_synced) {
    legacy_finalizer_->FinalizeInstall(web_app_info, options,
                                       base::DoNothing());
  }
}

void WebAppInstallFinalizer::FinalizeUninstallAfterSync(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  DCHECK(started_);
  // WebAppSyncBridge::ApplySyncChangesToRegistrar does the actual
  // NotifyWebAppUninstalled and unregistration of the app from the registry.
  DCHECK(!GetWebAppRegistrar().GetAppById(app_id));

  icon_manager_->DeleteData(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnIconsDataDeleted,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             std::move(callback)));
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

bool WebAppInstallFinalizer::CanUserUninstallFromSync(
    const AppId& app_id) const {
  DCHECK(started_);
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  return app ? app->IsSynced() : false;
}

void WebAppInstallFinalizer::UninstallWebAppFromSyncByUser(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  DCHECK(CanUserUninstallFromSync(app_id));
  UninstallWebAppOrRemoveSource(app_id, Source::kSync, std::move(callback));
}

bool WebAppInstallFinalizer::CanUserUninstallExternalApp(
    const AppId& app_id) const {
  DCHECK(started_);
  // TODO(loyso): Policy Apps: Implement web_app::ManagementPolicy taking
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
    InstallFinalizedCallback callback) {
  CHECK(started_);

  const AppId app_id = GenerateAppIdFromURL(web_app_info.start_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);

  if (!existing_web_app || existing_web_app->is_in_sync_install() ||
      web_app_info.start_url != existing_web_app->start_url()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  InstallResultCode::kWebAppDisabled));
    return;
  }

  // Prepare copy-on-write to update existing app.
  auto web_app = std::make_unique<WebApp>(*existing_web_app);
  const bool is_synced = web_app->IsSynced();

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id,
      existing_web_app->name(), web_app_info);

  SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                      std::move(commit_callback));

  if (legacy_finalizer_ && is_synced)
    legacy_finalizer_->FinalizeUpdate(web_app_info, base::DoNothing());
}

void WebAppInstallFinalizer::RemoveLegacyInstallFinalizerForTesting() {
  legacy_finalizer_ = nullptr;
}

InstallFinalizer* WebAppInstallFinalizer::legacy_finalizer_for_testing() {
  return legacy_finalizer_.get();
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
  registrar().NotifyWebAppUninstalled(app_id);
  WebAppProviderBase::GetProviderBase(profile_)
      ->os_integration_manager()
      .UninstallAllOsHooks(app_id, base::DoNothing());

  ScopedRegistryUpdate update(registry_controller().AsWebAppSyncBridge());
  update->DeleteApp(app_id);

  icon_manager_->DeleteData(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnIconsDataDeleted,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             std::move(callback)));
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
  icon_bitmaps.any = web_app_info.icon_bitmaps_any;
  icon_bitmaps.maskable = web_app_info.icon_bitmaps_maskable;
  icon_manager_->WriteData(
      std::move(app_id), std::move(icon_bitmaps),
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(commit_callback),
                     std::move(web_app),
                     web_app_info.shortcuts_menu_icons_bitmaps));
}

void WebAppInstallFinalizer::OnIconsDataWritten(
    CommitCallback commit_callback,
    std::unique_ptr<WebApp> web_app,
    const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  if (shortcuts_menu_icons_bitmaps.empty()) {
    OnShortcutsMenuIconsDataWritten(std::move(commit_callback),
                                    std::move(web_app), success);
  } else {
    AppId app_id = web_app->app_id();
    icon_manager_->WriteShortcutsMenuIconsData(
        app_id, shortcuts_menu_icons_bitmaps,
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

void WebAppInstallFinalizer::OnIconsDataDeleted(
    const AppId& app_id,
    UninstallWebAppCallback callback,
    bool success) {
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

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate(
    InstallFinalizedCallback callback,
    AppId app_id,
    std::string old_name,
    const WebApplicationInfo& web_app_info,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  WebAppProviderBase::GetProviderBase(profile_)
      ->os_integration_manager()
      .UpdateOsHooks(app_id, old_name, web_app_info);

  registrar().NotifyWebAppManifestUpdated(app_id, old_name);
  std::move(callback).Run(app_id, InstallResultCode::kSuccessAlreadyInstalled);
}

WebAppRegistrar& WebAppInstallFinalizer::GetWebAppRegistrar() const {
  WebAppRegistrar* web_app_registrar = registrar().AsWebAppRegistrar();
  DCHECK(web_app_registrar);
  return *web_app_registrar;
}

}  // namespace web_app
