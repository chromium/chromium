// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/usage_stats_permissions.h"

#include <algorithm>
#include <cwchar>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

std::vector<std::wstring> GetClientStatePathsForScope(UpdaterScope scope) {
  std::vector<std::wstring> paths = {CLIENT_STATE_KEY};
  if (IsSystemInstall(scope)) {
    paths.push_back(CLIENT_STATE_MEDIUM_KEY);
  }
  return paths;
}

std::vector<std::string> GetInstalledAppIds(
    HKEY hive,
    const std::vector<std::wstring>& key_paths) {
  std::vector<std::string> app_ids;
  for (const std::wstring& path : key_paths) {
    for (base::win::RegistryKeyIterator it(hive, path.c_str(), KEY_WOW64_32KEY);
         it.Valid(); ++it) {
      std::string app_id;
      if (!base::WideToUTF8(it.Name(), wcslen(it.Name()), &app_id)) {
        VLOG(1) << "AppId " << it.Name() << " from registry is not UTF-8";
        continue;
      }
      app_ids.push_back(app_id);
    }
  }
  return app_ids;
}

bool AppAllowsUsageStats(HKEY hive,
                         const std::vector<std::wstring>& key_paths,
                         const std::string& app_id) {
  return std::ranges::any_of(key_paths, [&](std::wstring key_path) {
    DWORD usagestats = 0;
    base::win::RegKey key;
    return key.Open(hive,
                    base::StrCat({key_path, base::UTF8ToWide(app_id)}).c_str(),
                    Wow6432(KEY_READ)) == ERROR_SUCCESS &&
           key.ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS &&
           usagestats == 1;
  });
}

}  // namespace

bool AnyAppEnablesUsageStats(HKEY hive,
                             const std::vector<std::wstring>& key_paths) {
  bool allowed = std::ranges::any_of(
      GetInstalledAppIds(hive, key_paths), [&](const std::string& app_id) {
        if (!IsUpdaterOrCompanionApp(app_id) &&
            AppAllowsUsageStats(hive, key_paths, app_id)) {
          VLOG(2) << "usage stats enabled by app " << app_id;
          return true;
        }
        return false;
      });

  if (!allowed) {
    VLOG(2) << "no app enables usage stats";
  }
  return allowed;
}

bool RemoteEventLoggingAllowed(
    HKEY hive,
    const std::vector<std::wstring>& key_paths,
    const std::vector<std::string>& installed_app_ids,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider) {
  if (!event_logging_permission_provider) {
    VLOG(2) << "Event logging disabled by absence of permission provider";
    return false;
  }

  bool manages_additional_apps =
      std::ranges::any_of(installed_app_ids, [&](const std::string& app_id) {
        return !IsRemoteEventLoggingPermissionExempt(app_id) &&
               !base::EqualsCaseInsensitiveASCII(
                   app_id, event_logging_permission_provider->app_id);
      });

  if (manages_additional_apps) {
    VLOG(2) << "Event logging disabled by presence of other apps";
    return false;
  }

  bool allowed = AppAllowsUsageStats(hive, key_paths,
                                     event_logging_permission_provider->app_id);

  VLOG_IF(2, !allowed) << "Event logging disabled; app "
                       << event_logging_permission_provider->app_id
                       << " does not enable usage stats";
  return allowed;
}

bool AnyAppEnablesUsageStats(UpdaterScope scope) {
  return AnyAppEnablesUsageStats(UpdaterScopeToHKeyRoot(scope),
                                 GetClientStatePathsForScope(scope));
}

bool RemoteEventLoggingAllowed(
    UpdaterScope scope,
    const std::vector<std::string>& installed_app_ids,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider) {
  return RemoteEventLoggingAllowed(
      UpdaterScopeToHKeyRoot(scope), GetClientStatePathsForScope(scope),
      installed_app_ids, std::move(event_logging_permission_provider));
}

}  // namespace updater
