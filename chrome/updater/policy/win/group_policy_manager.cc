// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/win/group_policy_manager.h"

#include <ostream>
#include <string>
#include <utility>

#include <userenv.h>

#include "base/enterprise_util.h"
#include "base/scoped_generic.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

// Registry values.
// Preferences Category.
const char kRegValueAutoUpdateCheckPeriodOverrideMinutes[] =
    "AutoUpdateCheckPeriodMinutes";
const char kRegValueUpdatesSuppressedStartHour[] = "UpdatesSuppressedStartHour";
const char kRegValueUpdatesSuppressedStartMin[] = "UpdatesSuppressedStartMin";
const char kRegValueUpdatesSuppressedDurationMin[] =
    "UpdatesSuppressedDurationMin";

// This policy specifies what kind of download URLs could be returned to the
// client in the update response and in which order of priority. The client
// provides this information in the update request as a hint for the server.
// The server may decide to ignore the hint. As a general idea, some urls are
// cacheable, some urls have higher bandwidth, and some urls are slightly more
// secure since they are https.
const char kRegValueDownloadPreference[] = "DownloadPreference";

// Proxy Server Category.  (The registry keys used, and the values of ProxyMode,
// directly mirror that of Chrome.  However, we omit ProxyBypassList, as the
// domains that Omaha uses are largely fixed.)
const char kRegValueProxyMode[] = "ProxyMode";
const char kRegValueProxyServer[] = "ProxyServer";
const char kRegValueProxyPacUrl[] = "ProxyPacUrl";

// Package cache constants.
const char kRegValueCacheSizeLimitMBytes[] = "PackageCacheSizeLimit";
const char kRegValueCacheLifeLimitDays[] = "PackageCacheLifeLimit";

// Applications Category.
// The prefix strings have the app's GUID appended to them.
const char kRegValueInstallAppsDefault[] = "InstallDefault";
const char kRegValueInstallAppPrefix[] = "Install";
const char kRegValueUpdateAppsDefault[] = "UpdateDefault";
const char kRegValueUpdateAppPrefix[] = "Update";
const char kRegValueTargetVersionPrefix[] = "TargetVersionPrefix";
const char kRegValueTargetChannel[] = "TargetChannel";
const char kRegValueRollbackToTargetVersion[] = "RollbackToTargetVersion";

struct ScopedHCriticalPolicySectionTraits {
  static HANDLE InvalidValue() { return nullptr; }
  static void Free(HANDLE handle) {
    if (handle != InvalidValue())
      ::LeaveCriticalPolicySection(handle);
  }
};

// Manages the lifetime of critical policy section handle allocated by
// ::EnterCriticalPolicySection.
using scoped_hpolicy =
    base::ScopedGeneric<HANDLE, updater::ScopedHCriticalPolicySectionTraits>;

}  // namespace

GroupPolicyManager::GroupPolicyManager() {
  LoadAllPolicies();
}

GroupPolicyManager::~GroupPolicyManager() = default;

bool GroupPolicyManager::IsManaged() const {
  return policies_.DictSize() > 0 && base::IsMachineExternallyManaged();
}

std::string GroupPolicyManager::source() const {
  return std::string("GroupPolicy");
}

bool GroupPolicyManager::GetLastCheckPeriodMinutes(int* minutes) const {
  return GetIntPolicy(kRegValueAutoUpdateCheckPeriodOverrideMinutes, minutes);
}

bool GroupPolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* suppressed_times) const {
  return GetIntPolicy(kRegValueUpdatesSuppressedStartHour,
                      &suppressed_times->start_hour_) &&
         GetIntPolicy(kRegValueUpdatesSuppressedStartMin,
                      &suppressed_times->start_minute_) &&
         GetIntPolicy(kRegValueUpdatesSuppressedDurationMin,
                      &suppressed_times->duration_minute_);
}

bool GroupPolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  return GetStringPolicy(kRegValueDownloadPreference, download_preference);
}

bool GroupPolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return GetIntPolicy(kRegValueCacheSizeLimitMBytes, cache_size_limit);
}

bool GroupPolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return GetIntPolicy(kRegValueCacheLifeLimitDays, cache_life_limit);
}

bool GroupPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  std::string app_value_name(kRegValueInstallAppPrefix);
  app_value_name.append(app_id);
  return GetIntPolicy(app_value_name.c_str(), install_policy)
             ? true
             : GetIntPolicy(kRegValueInstallAppsDefault, install_policy);
}

bool GroupPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    int* update_policy) const {
  std::string app_value_name(kRegValueUpdateAppPrefix);
  app_value_name.append(app_id);
  return GetIntPolicy(app_value_name.c_str(), update_policy)
             ? true
             : GetIntPolicy(kRegValueUpdateAppsDefault, update_policy);
}

bool GroupPolicyManager::GetTargetChannel(const std::string& app_id,
                                          std::string* channel) const {
  std::string app_value_name(kRegValueTargetChannel);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str(), channel);
}

bool GroupPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  std::string app_value_name(kRegValueTargetVersionPrefix);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str(), target_version_prefix);
}

bool GroupPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  std::string app_value_name(kRegValueRollbackToTargetVersion);
  app_value_name.append(app_id);
  int is_rollback_allowed = 0;
  if (GetIntPolicy(app_value_name.c_str(), &is_rollback_allowed)) {
    *rollback_allowed = is_rollback_allowed;
    return true;
  }

  return false;
}

bool GroupPolicyManager::GetProxyMode(std::string* proxy_mode) const {
  return GetStringPolicy(kRegValueProxyMode, proxy_mode);
}

bool GroupPolicyManager::GetProxyPacUrl(std::string* proxy_pac_url) const {
  return GetStringPolicy(kRegValueProxyPacUrl, proxy_pac_url);
}

bool GroupPolicyManager::GetProxyServer(std::string* proxy_server) const {
  return GetStringPolicy(kRegValueProxyServer, proxy_server);
}

bool GroupPolicyManager::GetIntPolicy(const std::string& key,
                                      int* value) const {
  const base::Value* policy =
      policies_.FindKeyOfType(key, base::Value::Type::INTEGER);
  if (policy == nullptr)
    return false;

  *value = policy->GetInt();
  return true;
}

bool GroupPolicyManager::GetStringPolicy(const std::string& key,
                                         std::string* value) const {
  const base::Value* policy =
      policies_.FindKeyOfType(key, base::Value::Type::STRING);
  if (policy == nullptr)
    return false;

  *value = policy->GetString();
  return true;
}

void GroupPolicyManager::LoadAllPolicies() {
  scoped_hpolicy policy_lock;

  if (base::IsMachineExternallyManaged()) {
    // GPO rules mandate a call to EnterCriticalPolicySection() before reading
    // policies (and a matching LeaveCriticalPolicySection() call after read).
    // Acquire the lock for domain-joined machines because group policies are
    // applied only in this case, and the lock acquisition can take a long
    // time, in the worst case scenarios.
    policy_lock.reset(::EnterCriticalPolicySection(true));
    CHECK(policy_lock.is_valid()) << "Failed to get policy lock.";
  }

  base::Value::DictStorage policy_storage;

  for (base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE,
                                           UPDATER_POLICIES_KEY);
       it.Valid(); ++it) {
    const std::string key_name = base::SysWideToUTF8(it.Name());
    switch (it.Type()) {
      case REG_SZ:
        policy_storage.emplace(key_name,
                               base::Value(base::SysWideToUTF8(it.Value())));
        break;

      case REG_DWORD:
        policy_storage.emplace(
            key_name, base::Value(*(reinterpret_cast<const int*>(it.Value()))));
        break;

      default:
        // Ignore all types that are not used by updater policies.
        break;
    }
  }

  policies_ = base::Value(std::move(policy_storage));
}

}  // namespace updater
