// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_manager.h"

#include <string>

#include "base/values.h"
#include "chrome/updater/policy/manager.h"

namespace updater {

namespace {

// Preferences Category.
const char kAutoUpdateCheckPeriodOverrideMinutes[] =
    "AutoUpdateCheckPeriodMinutes";
const char kUpdatesSuppressedStartHour[] = "UpdatesSuppressedStartHour";
const char kUpdatesSuppressedStartMin[] = "UpdatesSuppressedStartMin";
const char kUpdatesSuppressedDurationMin[] = "UpdatesSuppressedDurationMin";

// This policy specifies what kind of download URLs could be returned to the
// client in the update response and in which order of priority. The client
// provides this information in the update request as a hint for the server.
// The server may decide to ignore the hint. As a general idea, some urls are
// cacheable, some urls have higher bandwidth, and some urls are slightly more
// secure since they are https.
const char kDownloadPreference[] = "DownloadPreference";

// Proxy Server Category.  (The keys used, and the values of ProxyMode,
// directly mirror that of Chrome.  However, we omit ProxyBypassList, as the
// domains that Omaha uses are largely fixed.)
const char kProxyMode[] = "ProxyMode";
const char kProxyServer[] = "ProxyServer";
const char kProxyPacUrl[] = "ProxyPacUrl";

// Package cache constants.
const char kCacheSizeLimitMBytes[] = "PackageCacheSizeLimit";
const char kCacheLifeLimitDays[] = "PackageCacheLifeLimit";

// Applications Category.
// The prefix strings have the app's GUID appended to them.
const char kInstallAppsDefault[] = "InstallDefault";
const char kInstallAppPrefix[] = "Install";
const char kUpdateAppsDefault[] = "UpdateDefault";
const char kUpdateAppPrefix[] = "Update";
const char kTargetVersionPrefix[] = "TargetVersionPrefix";
const char kTargetChannel[] = "TargetChannel";
const char kRollbackToTargetVersion[] = "RollbackToTargetVersion";

}  // namespace

PolicyManager::PolicyManager(base::Value::Dict policies)
    : policies_(std::move(policies)) {}

PolicyManager::~PolicyManager() = default;

bool PolicyManager::IsManaged() const {
  return !policies_.empty();
}

std::string PolicyManager::source() const {
  return std::string("DictValuePolicy");
}

bool PolicyManager::GetLastCheckPeriodMinutes(int* minutes) const {
  return GetIntPolicy(kAutoUpdateCheckPeriodOverrideMinutes, minutes);
}

bool PolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* suppressed_times) const {
  return GetIntPolicy(kUpdatesSuppressedStartHour,
                      &suppressed_times->start_hour_) &&
         GetIntPolicy(kUpdatesSuppressedStartMin,
                      &suppressed_times->start_minute_) &&
         GetIntPolicy(kUpdatesSuppressedDurationMin,
                      &suppressed_times->duration_minute_);
}

bool PolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  return GetStringPolicy(kDownloadPreference, download_preference);
}

bool PolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return GetIntPolicy(kCacheSizeLimitMBytes, cache_size_limit);
}

bool PolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return GetIntPolicy(kCacheLifeLimitDays, cache_life_limit);
}

bool PolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  std::string app_value_name(kInstallAppPrefix);
  app_value_name.append(app_id);
  return GetIntPolicy(app_value_name.c_str(), install_policy)
             ? true
             : GetIntPolicy(kInstallAppsDefault, install_policy);
}

bool PolicyManager::GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                                    int* update_policy) const {
  std::string app_value_name(kUpdateAppPrefix);
  app_value_name.append(app_id);
  return GetIntPolicy(app_value_name.c_str(), update_policy)
             ? true
             : GetIntPolicy(kUpdateAppsDefault, update_policy);
}

bool PolicyManager::GetTargetChannel(const std::string& app_id,
                                     std::string* channel) const {
  std::string app_value_name(kTargetChannel);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str(), channel);
}

bool PolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  std::string app_value_name(kTargetVersionPrefix);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str(), target_version_prefix);
}

bool PolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  std::string app_value_name(kRollbackToTargetVersion);
  app_value_name.append(app_id);
  int is_rollback_allowed = 0;
  if (GetIntPolicy(app_value_name.c_str(), &is_rollback_allowed)) {
    *rollback_allowed = is_rollback_allowed;
    return true;
  }

  return false;
}

bool PolicyManager::GetProxyMode(std::string* proxy_mode) const {
  return GetStringPolicy(kProxyMode, proxy_mode);
}

bool PolicyManager::GetProxyPacUrl(std::string* proxy_pac_url) const {
  return GetStringPolicy(kProxyPacUrl, proxy_pac_url);
}

bool PolicyManager::GetProxyServer(std::string* proxy_server) const {
  return GetStringPolicy(kProxyServer, proxy_server);
}

bool PolicyManager::GetIntPolicy(const std::string& key, int* value) const {
  absl::optional<int> policy = policies_.FindInt(key);
  if (!policy.has_value())
    return false;

  *value = *policy;
  return true;
}

bool PolicyManager::GetStringPolicy(const std::string& key,
                                    std::string* value) const {
  const std::string* policy = policies_.FindString(key);
  if (policy == nullptr)
    return false;

  *value = *policy;
  return true;
}

}  // namespace updater
