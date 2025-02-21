// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

bool AppAllowsUsageStats(UpdaterScope scope, const std::string& id) {
  const std::wstring& app_id = base::UTF8ToWide(id);
  DWORD usagestats = 0;
  if (IsSystemInstall(scope) &&
      base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                        base::StrCat({CLIENT_STATE_MEDIUM_KEY, app_id}).c_str(),
                        Wow6432(KEY_READ))
              .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS) {
    return usagestats == 1;
  }

  if (base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                        GetAppClientStateKey(app_id).c_str(), Wow6432(KEY_READ))
          .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS) {
    return usagestats == 1;
  }

  return false;
}

bool AppInVectorAllowsUsageStats(UpdaterScope scope,
                                 const std::vector<std::string>& app_ids) {
  return std::ranges::any_of(app_ids, [&scope](const std::string& app_id) {
    return AppAllowsUsageStats(scope, app_id);
  });
}

// Returns all app ids which are not the Updater or CECA.
std::vector<std::string> FilterOtherAppIds(std::vector<std::string> app_ids) {
  app_ids.erase(std::remove_if(app_ids.begin(), app_ids.end(),
                               [](const std::string& app_id) {
                                 return IsUpdaterOrCompanionApp(app_id);
                               }),
                app_ids.end());
  return app_ids;
}

std::vector<std::string> GetAppIdsForScope(UpdaterScope scope) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  std::vector<std::wstring> subkeys;
  if (IsSystemInstall(scope)) {
    subkeys.push_back(CLIENT_STATE_MEDIUM_KEY);
  }
  subkeys.push_back(CLIENT_STATE_KEY);

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

}  // namespace

// Check the registry to see if an app besides the Updater and Companion app
// support usage stat reporting.
bool AnyAppUsageStatsAllowed(UpdaterScope scope) {
  bool allowed = AppInVectorAllowsUsageStats(
      scope, FilterOtherAppIds(GetAppIdsForScope(scope)));
  VLOG(2) << (allowed ? "usagestats enabled by another app"
                      : "no app enables usagestats");
  return allowed;
}

void UpdateUsageStatsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AppInVectorAllowsUsageStats, scope_,
                     FilterOtherAppIds(persisted_data_->GetAppIds())),
      base::BindOnce(&UpdateUsageStatsTask::SetUsageStatsEnabled, this,
                     persisted_data_)
          .Then(std::move(callback)));
}

}  // namespace updater
