// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
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
      return Source::kMaxValue;
  }
}

void SetWebAppIcons(const std::vector<WebApplicationIconInfo>& icon_infos,
                    WebApp* web_app) {
  WebApp::Icons web_app_icons;

  for (const WebApplicationIconInfo& icon_info : icon_infos) {
    // Skip unfetched bitmaps.
    if (icon_info.data.colorType() == kUnknown_SkColorType)
      continue;

    DCHECK_EQ(icon_info.width, icon_info.height);

    WebApp::IconInfo web_app_icon_info;
    web_app_icon_info.url = icon_info.url;
    web_app_icon_info.size_in_px = icon_info.width;

    web_app_icons.push_back(web_app_icon_info);
  }

  web_app->SetIcons(std::move(web_app_icons));
}

}  // namespace

WebAppInstallFinalizer::WebAppInstallFinalizer(WebAppSyncBridge* sync_bridge,
                                               WebAppIconManager* icon_manager)
    : sync_bridge_(sync_bridge), icon_manager_(icon_manager) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(loyso): Expose Source argument as a field of AppTraits struct.
  const auto source =
      InferSourceFromMetricsInstallSource(options.install_source);

  const AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  const WebApp* existing_web_app = sync_bridge_->registrar().GetAppById(app_id);

  if (existing_web_app && !existing_web_app->is_in_sync_install()) {
    // There is an existing app from other source(s). Preserve
    // is_locally_installed flag value, do not modify it.
    ScopedRegistryUpdate update(sync_bridge_);
    WebApp* local_web_app = update->UpdateApp(app_id);
    local_web_app->AddSource(source);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), app_id,
                                  InstallResultCode::kSuccessAlreadyInstalled));
    return;
  }

  std::unique_ptr<WebApp> web_app;

  if (existing_web_app && existing_web_app->is_in_sync_install()) {
    // There is an existing app awaiting for any online install completion, not
    // only from kSync source. Prepare copy-on-write:
    DCHECK_EQ(web_app_info.app_url, existing_web_app->launch_url());
    web_app = std::make_unique<WebApp>(*existing_web_app);
    // options.locally_installed is ignored here.
    // |user_display_mode| is preserved here: we get it from sync.
  } else {
    // New app.
    web_app = std::make_unique<WebApp>(app_id);
    web_app->SetLaunchUrl(web_app_info.app_url);
    web_app->SetIsLocallyInstalled(options.locally_installed);
    web_app->SetUserDisplayMode(web_app_info.open_as_window
                                    ? DisplayMode::kStandalone
                                    : DisplayMode::kBrowser);
  }

  web_app->AddSource(source);
  web_app->SetIsInSyncInstall(false);

  web_app->SetName(base::UTF16ToUTF8(web_app_info.title));
  web_app->SetDisplayMode(web_app_info.display_mode);
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app->SetScope(web_app_info.scope);
  if (web_app_info.theme_color) {
    web_app->SetThemeColor(
        SkColorSetA(*web_app_info.theme_color, SK_AlphaOPAQUE));
  }

  WebApp::SyncData sync_data;
  sync_data.name = base::UTF16ToUTF8(web_app_info.title);
  sync_data.theme_color = web_app_info.theme_color;
  web_app->SetSyncData(std::move(sync_data));

  SetWebAppIcons(web_app_info.icons, web_app.get());

  icon_manager_->WriteData(
      std::move(app_id), web_app_info.icons,
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(web_app)));
}

void WebAppInstallFinalizer::FinalizeFallbackInstallAfterSync(
    const AppId& app_id,
    InstallFinalizedCallback callback) {
  const WebApp* app_in_sync_install =
      sync_bridge_->registrar().GetAppById(app_id);
  DCHECK(app_in_sync_install);
  DCHECK(app_in_sync_install->is_in_sync_install());

  // Promote the app in sync install to a full user-visible app using the poor
  // data that we've got from sync. Prepare copy-on-write:
  auto web_app = std::make_unique<WebApp>(*app_in_sync_install);
  web_app->SetIsInSyncInstall(false);

  web_app->SetName(web_app->sync_data().name);
  web_app->SetThemeColor(web_app->sync_data().theme_color);

  // If no color has been specified, use dark gray.
  const SkColor background_icon_color =
      web_app->sync_data().theme_color.has_value()
          ? web_app->sync_data().theme_color.value()
          : SK_ColorDKGRAY;

  std::vector<WebApplicationIconInfo> icon_infos =
      GenerateIcons(web_app->sync_data().name, background_icon_color);

  SetWebAppIcons(icon_infos, web_app.get());

  InstallFinalizedCallback fallback_install_callback =
      base::BindOnce(&WebAppInstallFinalizer::OnFallbackInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(),
                     app_in_sync_install->app_id(), std::move(callback));

  icon_manager_->WriteData(
      std::move(app_id), std::move(icon_infos),
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(fallback_install_callback), std::move(web_app)));
}

void WebAppInstallFinalizer::FinalizeUninstallAfterSync(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  // WebAppSyncBridge::ApplySyncChangesToRegistrar does the actual
  // unregistration of the app from the registry.
  DCHECK(!sync_bridge_->registrar().GetAppById(app_id));

  icon_manager_->DeleteData(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnIconsDataDeleted,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             std::move(callback)));
}

void WebAppInstallFinalizer::OnIconsDataDeleted(
    const AppId& app_id,
    UninstallWebAppCallback callback,
    bool success) {
  registrar().NotifyWebAppUninstalled(app_id);
  std::move(callback).Run(success);
}

void WebAppInstallFinalizer::UninstallExternalWebApp(
    const GURL& app_url,
    ExternalInstallSource external_install_source,
    UninstallWebAppCallback callback) {
  NOTIMPLEMENTED();
}

bool WebAppInstallFinalizer::CanUserUninstallFromSync(
    const AppId& app_id) const {
  // TODO(crbug.com/901226): Implement it.
  return false;
}

void WebAppInstallFinalizer::UninstallWebAppFromSyncByUser(
    const AppId& app_id,
    UninstallWebAppCallback) {
  // TODO(loyso): Implement The Unified Uninstall API. Expose Source as an
  // argument for UninstallWebApp method. Do app->RemoveSource from the app and
  // uninstall the app if no more sources interested.
  NOTIMPLEMENTED();
}

void WebAppInstallFinalizer::FinalizeUpdate(
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback) {
  // TODO(crbug.com/926083): Implement update logic, this requires updating
  // WebAppIconManager to clean out the existing icons and write new ones.
  NOTIMPLEMENTED();
}

void WebAppInstallFinalizer::OnIconsDataWritten(
    InstallFinalizedCallback callback,
    std::unique_ptr<WebApp> web_app,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  AppId app_id = web_app->app_id();

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge_->BeginUpdate();

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override)
    *app_to_override = std::move(*web_app);
  else
    update->CreateApp(std::move(web_app));

  sync_bridge_->CommitUpdate(
      std::move(update),
      base::BindOnce(&WebAppInstallFinalizer::OnDatabaseCommitCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(app_id)));
}

void WebAppInstallFinalizer::OnDatabaseCommitCompleted(
    InstallFinalizedCallback callback,
    const AppId& app_id,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  registrar().NotifyWebAppInstalled(app_id);
  std::move(callback).Run(app_id, InstallResultCode::kSuccessNewInstall);
}

void WebAppInstallFinalizer::OnFallbackInstallFinalized(
    const AppId& app_in_sync_install_id,
    InstallFinalizedCallback callback,
    const AppId& installed_app_id,
    InstallResultCode code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("Webapp.SyncInitiatedFallbackInstallResult", code);
  if (!IsSuccess(code)) {
    DLOG(ERROR) << "Installation failed for app in sync install. app_id="
                << app_in_sync_install_id << " code=" << static_cast<int>(code);
  }

  std::move(callback).Run(installed_app_id, code);
}

bool WebAppInstallFinalizer::CanRevealAppShim() const {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
  return false;
}

void WebAppInstallFinalizer::RevealAppShim(const AppId& app_id) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
}

}  // namespace web_app
