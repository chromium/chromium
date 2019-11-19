// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registry_controller.h"

#include <utility>

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using web_app::DisplayMode;

namespace extensions {

BookmarkAppRegistryController::BookmarkAppRegistryController(Profile* profile)
    : AppRegistryController(profile) {}

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
    DisplayMode display_mode) {
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

void BookmarkAppRegistryController::SetAppIsLocallyInstalledForTesting(
    const web_app::AppId& app_id,
    bool is_locally_installed) {
  SetBookmarkAppIsLocallyInstalled(profile(), GetExtension(app_id),
                                   is_locally_installed);
}

web_app::WebAppSyncBridge* BookmarkAppRegistryController::AsWebAppSyncBridge() {
  return nullptr;
}

}  // namespace extensions
