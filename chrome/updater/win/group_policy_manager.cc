// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/group_policy_manager.h"

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/updater/policy_manager.h"
#include "chrome/updater/win/constants.h"

namespace updater {

// Registry values.
// Preferences Category.
const wchar_t kRegValueAutoUpdateCheckPeriodOverrideMinutes[] =
    L"AutoUpdateCheckPeriodMinutes";
const wchar_t kRegValueUpdatesSuppressedStartHour[] =
    L"UpdatesSuppressedStartHour";
const wchar_t kRegValueUpdatesSuppressedStartMin[] =
    L"UpdatesSuppressedStartMin";
const wchar_t kRegValueUpdatesSuppressedDurationMin[] =
    L"UpdatesSuppressedDurationMin";

// This policy specifies what kind of download URLs could be returned to the
// client in the update response and in which order of priority. The client
// provides this information in the update request as a hint for the server.
// The server may decide to ignore the hint. As a general idea, some urls are
// cacheable, some urls have higher bandwidth, and some urls are slightly more
// secure since they are https.
const wchar_t kRegValueDownloadPreference[] = L"DownloadPreference";

// Proxy Server Category.  (The registry keys used, and the values of ProxyMode,
// directly mirror that of Chrome.  However, we omit ProxyBypassList, as the
// domains that Omaha uses are largely fixed.)
const wchar_t kRegValueProxyMode[] = L"ProxyMode";
const wchar_t kRegValueProxyServer[] = L"ProxyServer";
const wchar_t kRegValueProxyPacUrl[] = L"ProxyPacUrl";

// Package cache constants.
const wchar_t kRegValueCacheSizeLimitMBytes[] = L"PackageCacheSizeLimit";
const wchar_t kRegValueCacheLifeLimitDays[] = L"PackageCacheLifeLimit";

// Applications Category.
// The prefix strings have the app's GUID appended to them.
const wchar_t kRegValueInstallAppsDefault[] = L"InstallDefault";
const wchar_t kRegValueInstallAppPrefix[] = L"Install";
const wchar_t kRegValueUpdateAppsDefault[] = L"UpdateDefault";
const wchar_t kRegValueUpdateAppPrefix[] = L"Update";
const wchar_t kRegValueTargetVersionPrefix[] = L"TargetVersionPrefix";
const wchar_t kRegValueTargetChannel[] = L"TargetChannel";
const wchar_t kRegValueRollbackToTargetVersion[] = L"RollbackToTargetVersion";

GroupPolicyManager::GroupPolicyManager() {
  key_.Open(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY, KEY_READ);
}

GroupPolicyManager::~GroupPolicyManager() = default;

bool GroupPolicyManager::IsManaged() const {
  return key_.Valid() && base::win::IsEnrolledToDomain();
}

std::string GroupPolicyManager::source() const {
  return std::string("GroupPolicy");
}

bool GroupPolicyManager::GetLastCheckPeriodMinutes(int* minutes) const {
  return ReadValueDW(kRegValueAutoUpdateCheckPeriodOverrideMinutes, minutes);
}

bool GroupPolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* suppressed_times) const {
  return ReadValueDW(kRegValueUpdatesSuppressedStartHour,
                     &suppressed_times->start_hour) &&
         ReadValueDW(kRegValueUpdatesSuppressedStartMin,
                     &suppressed_times->start_minute) &&
         ReadValueDW(kRegValueUpdatesSuppressedDurationMin,
                     &suppressed_times->duration_minute);
}

bool GroupPolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  return ReadValue(kRegValueDownloadPreference, download_preference);
}

bool GroupPolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return ReadValueDW(kRegValueCacheSizeLimitMBytes, cache_size_limit);
}

bool GroupPolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return ReadValueDW(kRegValueCacheLifeLimitDays, cache_life_limit);
}

bool GroupPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  std::wstring app_value_name(kRegValueInstallAppPrefix);
  app_value_name.append(base::SysUTF8ToWide(app_id));
  return ReadValueDW(app_value_name.c_str(), install_policy)
             ? true
             : ReadValueDW(kRegValueInstallAppsDefault, install_policy);
}

bool GroupPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    int* update_policy) const {
  std::wstring app_value_name(kRegValueUpdateAppPrefix);
  app_value_name.append(base::SysUTF8ToWide(app_id));
  return ReadValueDW(app_value_name.c_str(), update_policy)
             ? true
             : ReadValueDW(kRegValueUpdateAppsDefault, update_policy);
}

bool GroupPolicyManager::GetTargetChannel(const std::string& app_id,
                                          std::string* channel) const {
  std::wstring app_value_name(kRegValueTargetChannel);
  app_value_name.append(base::SysUTF8ToWide(app_id));
  return ReadValue(app_value_name.c_str(), channel);
}

bool GroupPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  std::wstring app_value_name(kRegValueTargetVersionPrefix);
  app_value_name.append(base::SysUTF8ToWide(app_id));
  return ReadValue(app_value_name.c_str(), target_version_prefix);
}

bool GroupPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  std::wstring app_value_name(kRegValueRollbackToTargetVersion);
  app_value_name.append(base::SysUTF8ToWide(app_id));
  int is_rollback_allowed = 0;
  if (ReadValueDW(app_value_name.c_str(), &is_rollback_allowed)) {
    *rollback_allowed = is_rollback_allowed;
    return true;
  }

  return false;
}

bool GroupPolicyManager::GetProxyMode(std::string* proxy_mode) const {
  return ReadValue(kRegValueProxyMode, proxy_mode);
}

bool GroupPolicyManager::GetProxyPacUrl(std::string* proxy_pac_url) const {
  return ReadValue(kRegValueProxyPacUrl, proxy_pac_url);
}

bool GroupPolicyManager::GetProxyServer(std::string* proxy_server) const {
  return ReadValue(kRegValueProxyServer, proxy_server);
}

bool GroupPolicyManager::ReadValue(const wchar_t* name,
                                   std::string* value) const {
  std::wstring value_wide;
  if (key_.ReadValue(name, &value_wide) != ERROR_SUCCESS)
    return false;

  *value = base::SysWideToUTF8(value_wide);
  return true;
}

bool GroupPolicyManager::ReadValueDW(const wchar_t* name, int* value) const {
  return key_.ReadValueDW(name, reinterpret_cast<DWORD*>(value)) ==
         ERROR_SUCCESS;
}

}  // namespace updater
