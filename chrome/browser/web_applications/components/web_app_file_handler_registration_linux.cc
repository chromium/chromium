// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace web_app {

namespace {

void OnShortcutInfoReceived(const std::set<std::string> mime_types,
                            std::unique_ptr<ShortcutInfo> info) {
  for (const auto& mime_type : mime_types)
    info->mime_types.push_back(mime_type);

  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  internals::ScheduleCreatePlatformShortcuts(
      std::move(shortcut_data_dir), locations,
      ShortcutCreationReason::SHORTCUT_CREATION_BY_USER, std::move(info),
      base::DoNothing());
}

}  // namespace

bool OsSupportsWebAppFileHandling() {
  return true;
}

void RegisterFileHandlersForWebApp(const AppId& app_id,
                                   const std::string& app_name,
                                   Profile* profile,
                                   const std::set<std::string>& file_extensions,
                                   const std::set<std::string>& mime_types) {
  AppShortcutManager& shortcut_manager =
      WebAppProviderBase::GetProviderBase(profile)->shortcut_manager();
  shortcut_manager.GetShortcutInfoForApp(
      app_id, base::BindOnce(OnShortcutInfoReceived, mime_types));
}

void UnregisterFileHandlersForWebApp(const AppId& app_id, Profile* profile) {
  // TODO(harrisjay): Add support for unregistering file handlers.
}

}  // namespace web_app
