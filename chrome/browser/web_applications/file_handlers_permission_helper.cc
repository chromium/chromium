// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/file_handlers_permission_helper.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "url/gurl.h"

namespace web_app {

FileHandlersPermissionHelper::FileHandlersPermissionHelper(
    WebAppInstallFinalizer* finalizer)
    : finalizer_(finalizer) {
  // This catches permission changes occurring when browser is active, from
  // permission prompts, site settings, and site settings padlock. When
  // permission setting is changed to be blocked/allowed, update app's
  // `file_handler_permission_blocked` state and update file handlers on OS.
  content_settings_observer_.Observe(
      HostContentSettingsMapFactory::GetForProfile(finalizer_->profile()));

  app_registrar_observer_.Observe(&finalizer_->registrar());

  // This catches permission changes occurring when browser is not active, like
  // enterprise policy changes. It detects any file handling permission mismatch
  // between the app db state and permission settings, and correct the state in
  // db as well as in the OS for all apps.
  UpdateAppsMatchingPattern(ContentSettingsPattern::Wildcard());
}

FileHandlersPermissionHelper::~FileHandlersPermissionHelper() = default;

void FileHandlersPermissionHelper::WillInstallApp(
    const WebApplicationInfo& web_app_info) {
  // This step is necessary in case this app shares an origin with another PWA
  // which already asked for file handling permissions, and the new app asks to
  // handle more file types.
  MaybeResetPermission(web_app_info);
}

FileHandlerUpdateAction FileHandlersPermissionHelper::WillUpdateApp(
    const AppId app_id,
    const WebApplicationInfo& web_app_info) {
  if (!finalizer_->os_integration_manager().IsFileHandlingAPIAvailable(app_id))
    return FileHandlerUpdateAction::kNoUpdate;

  const GURL& url = web_app_info.scope;

  // Keep in sync with chromeos::kChromeUIMediaAppURL.
  const char kChromeUIMediaAppURL[] = "chrome://media-app/";
  // Keep in sync with chromeos::kChromeUICameraAppURL.
  const char kChromeUICameraAppURL[] = "chrome://camera-app/";

  // Omit file handler removal and permission downgrade for the ChromeOS Media
  // and Camera System Web Apps (SWAs), which have permissions granted by
  // default. This exception and check is only relevant in ChromeOS, the only
  // platform where SWAs are in use.
  if (url == kChromeUIMediaAppURL || url == kChromeUICameraAppURL) {
    return FileHandlerUpdateAction::kUpdate;
  }

  // It's possible we'll downgrade the permission and then fail to update OS
  // integrations (ex. if the disk or icon downloads fail), but this is ok
  // because these failures should rarely occur.
  ContentSetting content_setting = MaybeResetPermission(web_app_info);

  // If the permission is "BLOCK", leave it as is. When permission is
  // "BLOCK", the `OnContentSettingChanged()` and
  // `DetectAndCorrectFileHandlingPermissionBlocks()` should capture the
  // permission change and make sure the OS and db state are in sync with the
  // PermissionManager permission setting. Therefore, manifest update task
  // should not update file handlers due to blocked permission state.
  if (content_setting == CONTENT_SETTING_BLOCK)
    return FileHandlerUpdateAction::kNoUpdate;

  // TODO(https://crbug.com/1197013): Consider trying to re-use the comparison
  // results from the ManifestUpdateTask.
  const apps::FileHandlers* old_handlers =
      finalizer_->registrar().GetAppFileHandlers(app_id);
  DCHECK(old_handlers);
  if (*old_handlers == web_app_info.file_handlers)
    return FileHandlerUpdateAction::kNoUpdate;

  return FileHandlerUpdateAction::kUpdate;
}

void FileHandlersPermissionHelper::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  if (content_type != ContentSettingsType::FILE_HANDLING)
    return;

  UpdateAppsMatchingPattern(primary_pattern);
}

void FileHandlersPermissionHelper::OnWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  ScopedRegistryUpdate update(
      finalizer_->registry_controller().AsWebAppSyncBridge());
  WebApp* app = update->UpdateApp(app_id);
  app->SetFileHandlerPermissionBlocked(IsPermissionBlocked(app->scope()));
}

void FileHandlersPermissionHelper::OnWebAppWillBeUninstalled(
    const AppId& app_id) {
  // If the uninstalled app had no file handler registrations, do nothing.
  const WebAppRegistrar& registrar = finalizer_->GetWebAppRegistrar();
  if (registrar.GetAppFileHandlers(app_id)->empty())
    return;

  // See if there are any remaining apps in the same origin that have file
  // handlers. If so, do nothing.
  const WebApp* app = registrar.GetAppById(app_id);
  const GURL origin = app->scope().GetOrigin();
  std::vector<AppId> app_ids = registrar.FindAppsInScope(origin);
  for (const AppId& app_id_iter : app_ids) {
    const apps::FileHandlers* handlers =
        registrar.GetAppFileHandlers(app_id_iter);
    if (app_id != app_id_iter && !handlers->empty())
      return;
  }

  // This was the last app in the given origin that had file handlers, so reset
  // the permission.
  PermissionManagerFactory::GetForProfile(finalizer_->profile())
      ->ResetPermission(content::PermissionType::FILE_HANDLING, origin, origin);
}

bool FileHandlersPermissionHelper::IsPermissionBlocked(const GURL& scope) {
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(finalizer_->profile());
  permissions::PermissionResult status =
      permission_manager->GetPermissionStatus(
          ContentSettingsType::FILE_HANDLING, scope, scope);
  return status.content_setting == CONTENT_SETTING_BLOCK;
}

ContentSetting FileHandlersPermissionHelper::MaybeResetPermission(
    const WebApplicationInfo& web_app_info) {
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(finalizer_->profile());
  DCHECK(permission_manager);
  const GURL& url = web_app_info.scope;
  // Note: Since a frame is not available, using GetPermissionStatus() instead
  // of GetPermissionStatusForFrame().
  permissions::PermissionResult status =
      permission_manager->GetPermissionStatus(
          ContentSettingsType::FILE_HANDLING, url, url);

  // If file handling permission is "ALLOW", downgrade to "ASK" via reset, as
  // the user may not want to allow newly added file handlers, which may include
  // more dangerous extensions.
  if (status.content_setting == CONTENT_SETTING_ALLOW &&
      !AreFileHandlersAlreadyRegistered(finalizer_->profile(), url,
                                        web_app_info.file_handlers)) {
    permission_manager->ResetPermission(content::PermissionType::FILE_HANDLING,
                                        url, url);
    return CONTENT_SETTING_ASK;
  }

  return status.content_setting;
}

void FileHandlersPermissionHelper::UpdateAppsMatchingPattern(
    const ContentSettingsPattern& pattern) {
  ScopedRegistryUpdate update(
      finalizer_->registry_controller().AsWebAppSyncBridge());
  for (const AppId& app_id : finalizer_->registrar().GetAppIds()) {
    const WebApp* app = finalizer_->GetWebAppRegistrar().GetAppById(app_id);
    if (!app || !app->is_locally_installed())
      continue;

    const GURL url = app->scope();
    if (!pattern.Matches(url))
      continue;

    bool permission_blocked = IsPermissionBlocked(url);
    if (permission_blocked == app->file_handler_permission_blocked())
      continue;

    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetFileHandlerPermissionBlocked(permission_blocked);
    FileHandlerUpdateAction file_handlers_need_os_update =
        permission_blocked ? FileHandlerUpdateAction::kRemove
                           : FileHandlerUpdateAction::kUpdate;
    finalizer_->os_integration_manager().UpdateFileHandlers(
        app_id, file_handlers_need_os_update);
  }
}

}  // namespace web_app
