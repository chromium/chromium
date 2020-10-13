// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registry_controller.h"

#include <utility>

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using web_app::DisplayMode;

namespace extensions {

BookmarkAppRegistryController::BookmarkAppRegistryController(
    Profile* profile,
    BookmarkAppRegistrar* registrar)
    : AppRegistryController(profile), registrar_(registrar) {}

BookmarkAppRegistryController::~BookmarkAppRegistryController() = default;

void BookmarkAppRegistryController::Init(base::OnceClosure callback) {
  ExtensionSystem::Get(profile())->ready().Post(FROM_HERE, std::move(callback));
}

const Extension* BookmarkAppRegistryController::GetExtension(
    const web_app::AppId& app_id) const {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(app_id);
  DCHECK(!extension || extension->from_bookmark());
  return extension;
}

void BookmarkAppRegistryController::SetAppUserDisplayMode(
    const web_app::AppId& app_id,
    DisplayMode display_mode,
    bool is_user_action) {
  const Extension* extension = GetExtension(app_id);
  if (!extension)
    return;

  switch (display_mode) {
    case DisplayMode::kStandalone:
      extensions::SetLaunchType(profile(), extension->id(),
                                extensions::LAUNCH_TYPE_WINDOW);
      return;
    case DisplayMode::kBrowser:
      extensions::SetLaunchType(profile(), extension->id(),
                                extensions::LAUNCH_TYPE_REGULAR);
      return;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      return;
  }
}

// Disabling here isn't equivalent to extensions disabling. It means the app is
// installed, but won't be launched via app service. Ideally this disabled state
// should be kept in the data model (ExtensionPrefs) and have a getter the same
// as WebApp::chromeos_data.is_disabled, but as BMO will launch soon and this is
// a short-term solution, it's not added to ExtensionPrefs.
void BookmarkAppRegistryController::SetAppIsDisabled(
    const web_app::AppId& app_id,
    bool is_disabled) {
  const Extension* extension = GetExtension(app_id);
  if (!extension)
    return;

  registrar_->NotifyWebAppDisabledStateChanged(app_id, is_disabled);
}

void BookmarkAppRegistryController::SetAppIsLocallyInstalled(
    const web_app::AppId& app_id,
    bool is_locally_installed) {
  SetBookmarkAppIsLocallyInstalled(profile(), GetExtension(app_id),
                                   is_locally_installed);
  registrar_->NotifyWebAppLocallyInstalledStateChanged(app_id,
                                                       is_locally_installed);
}

void BookmarkAppRegistryController::SetAppLastLaunchTime(
    const web_app::AppId& app_id,
    const base::Time& time) {
  const Extension* extension = GetExtension(app_id);
  if (!extension)
    return;
  ExtensionPrefs::Get(profile())->SetLastLaunchTime(extension->id(), time);
  registrar_->NotifyWebAppLastLaunchTimeChanged(app_id, time);
}

// Bookmark apps are deprecated. They don't update install time on local
// installs.
void BookmarkAppRegistryController::SetAppInstallTime(
    const web_app::AppId& app_id,
    const base::Time& time) {}

// Bookmark apps are deprecated. They don't support Run on OS Login.
void BookmarkAppRegistryController::SetAppRunOnOsLoginMode(
    const web_app::AppId& app_id,
    web_app::RunOnOsLoginMode mode) {}

web_app::WebAppSyncBridge* BookmarkAppRegistryController::AsWebAppSyncBridge() {
  return nullptr;
}

}  // namespace extensions
