// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <algorithm>
#include <memory>
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
  UsageStatsProviderImpl(const std::wstring& system_key,
                         const std::wstring& user_key)
      : system_key_(system_key), user_key_(user_key) {}

  bool AnyAppEnablesUsageStats(UpdaterScope scope) override {
    bool allowed = std::ranges::any_of(
        GetAppIdsForScope(scope), [this, &scope](const std::string& app_id) {
          if (!IsUpdaterOrCompanionApp(app_id) &&
              AppAllowsUsageStats(scope, app_id)) {
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
  std::vector<std::string> GetAppIdsForScope(UpdaterScope scope) {
    const HKEY root = UpdaterScopeToHKeyRoot(scope);
    std::vector<std::wstring> subkeys({user_key_});
    if (IsSystemInstall(scope)) {
      subkeys.push_back(system_key_);
    }

    std::vector<std::string> app_ids;
    for (const auto& subkey : subkeys) {
      for (base::win::RegistryKeyIterator it(root, subkey.c_str(),
                                             KEY_WOW64_32KEY);
           it.Valid(); ++it) {
        app_ids.push_back(base::WideToUTF8(it.Name()));
      }
    }

    return app_ids;
  }

  bool AppAllowsUsageStats(UpdaterScope scope, const std::string& id) {
    const std::wstring& app_id = base::UTF8ToWide(id);
    DWORD usagestats = 0;
    if (IsSystemInstall(scope) &&
        base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                          base::StrCat({system_key_, app_id}).c_str(),
                          Wow6432(KEY_READ))
                .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS) {
      return usagestats == 1;
    }

    if (base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                          base::StrCat({user_key_, app_id}).c_str(),
                          Wow6432(KEY_READ))
            .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS) {
      return usagestats == 1;
    }

    return false;
  }

  std::wstring system_key_;
  std::wstring user_key_;
};

// Returns a usage stats provider that checks for apps under the
// CLIENT_STATE_MEDIUM_KEY and CLIENT_STATE_KEY registry keys. The updater
// stores installation and usage stat information in these keys.
std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create() {
  return UsageStatsProvider::Create(
      /*system_key=*/CLIENT_STATE_MEDIUM_KEY, /*user_key=*/CLIENT_STATE_KEY);
}

// Returns a usage stats provider that checks apps installed under the
// `system_key` and `user_key` in the registry. The updater
// stores installation and usage stat information in these keys.
std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    const std::wstring& system_key,
    const std::wstring& user_key) {
  return std::make_unique<UsageStatsProviderImpl>(system_key, user_key);
}

}  // namespace updater
