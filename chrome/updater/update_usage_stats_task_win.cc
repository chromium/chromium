// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <algorithm>
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
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

class UsageStatsProviderImpl : public UsageStatsProvider {
 public:
  UsageStatsProviderImpl(
      HKEY hive,
      std::optional<std::wstring> event_logging_permission_provider,
      std::vector<std::wstring> key_paths)
      : hive_(hive),
        event_logging_permission_provider_(
            std::move(event_logging_permission_provider)),
        key_paths_(std::move(key_paths)) {}

  bool AnyAppEnablesUsageStats() const override {
    bool allowed = std::ranges::any_of(
        GetInstalledAppIds(), [this](const std::wstring& app_id) {
          if (!IsUpdaterOrCompanionApp(base::WideToUTF8(app_id)) &&
              AppAllowsUsageStats(app_id)) {
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

 private:
  std::vector<std::wstring> GetInstalledAppIds() const {
    std::vector<std::wstring> app_ids;
    for (const std::wstring& path : key_paths_) {
      for (base::win::RegistryKeyIterator it(hive_, path.c_str(),
                                             KEY_WOW64_32KEY);
           it.Valid(); ++it) {
        app_ids.push_back(it.Name());
      }
    }
    return app_ids;
  }

  bool AppAllowsUsageStats(const std::wstring& app_id) const {
    return std::ranges::any_of(
        key_paths_, [this, app_id](std::wstring key_path) {
          DWORD usagestats = 0;
          base::win::RegKey key;
          return key.Open(hive_, base::StrCat({key_path, app_id}).c_str(),
                          Wow6432(KEY_READ)) == ERROR_SUCCESS &&
                 key.ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS &&
                 usagestats == 1;
        });
  }

  bool RemoteEventLoggingAllowed() const override {
    if (!event_logging_permission_provider_) {
      return false;
    }

    bool manages_additional_apps = std::ranges::any_of(
        GetInstalledAppIds(), [this](const std::wstring& app_id) {
          return !IsUpdaterOrCompanionApp(base::WideToUTF8(app_id)) &&
                 (app_id != *event_logging_permission_provider_);
        });

    return !manages_additional_apps &&
           AppAllowsUsageStats(*event_logging_permission_provider_);
  }

  HKEY hive_;
  std::optional<std::wstring> event_logging_permission_provider_;
  std::vector<std::wstring> key_paths_;
};

// CLIENT_STATE_MEDIUM_KEY and CLIENT_STATE_KEY registry keys. The updater
// stores installation and usage stat information in these keys.
std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    UpdaterScope scope) {
  return UsageStatsProvider::Create(
      UpdaterScopeToHKeyRoot(scope), std::nullopt,
      IsSystemInstall(scope) ? std::vector<std::wstring>(
                                   {CLIENT_STATE_KEY, CLIENT_STATE_MEDIUM_KEY})
                             : std::vector<std::wstring>({CLIENT_STATE_KEY}));
}

// Returns a usage stats provider that checks for installed app data under the
// given registry keys.
std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    HKEY hive,
    std::optional<std::wstring> event_logging_permission_provider,
    std::vector<std::wstring> key_paths) {
  return std::make_unique<UsageStatsProviderImpl>(
      hive, std::move(event_logging_permission_provider), std::move(key_paths));
}

}  // namespace updater
