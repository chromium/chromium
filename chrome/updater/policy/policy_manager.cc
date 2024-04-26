// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_manager.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

namespace {

constexpr char kCloudPolicyOverridesPlatformPolicy[] =
    "CloudPolicyOverridesPlatformPolicy";

// Preferences Category.
constexpr char kAutoUpdateCheckPeriodOverrideMinutes[] =
    "AutoUpdateCheckPeriodMinutes";
constexpr char kUpdatesSuppressedStartHour[] = "UpdatesSuppressedStartHour";
constexpr char kUpdatesSuppressedStartMin[] = "UpdatesSuppressedStartMin";
constexpr char kUpdatesSuppressedDurationMin[] = "UpdatesSuppressedDurationMin";

// This policy specifies what kind of download URLs could be returned to the
// client in the update response and in which order of priority. The client
// provides this information in the update request as a hint for the server.
// The server may decide to ignore the hint. As a general idea, some urls are
// cacheable, some urls have higher bandwidth, and some urls are slightly more
// secure since they are https.
constexpr char kDownloadPreference[] = "DownloadPreference";

// Proxy Server Category.  (The keys used, and the values of ProxyMode,
// directly mirror that of Chrome.  However, we omit ProxyBypassList, as the
// domains that Omaha uses are largely fixed.)
constexpr char kProxyMode[] = "ProxyMode";
constexpr char kProxyServer[] = "ProxyServer";
constexpr char kProxyPacUrl[] = "ProxyPacUrl";

// Package cache constants.
constexpr char kCacheSizeLimitMBytes[] = "PackageCacheSizeLimit";
constexpr char kCacheLifeLimitDays[] = "PackageCacheLifeLimit";

// Applications Category.
// The prefix strings have the app's GUID appended to them.
constexpr char kInstallAppsDefault[] = "installdefault";
constexpr char kInstallAppPrefix[] = "install";
constexpr char kUpdateAppsDefault[] = "updatedefault";
constexpr char kUpdateAppPrefix[] = "update";
constexpr char kTargetVersionPrefix[] = "targetversionprefix";
constexpr char kTargetChannel[] = "targetchannel";
constexpr char kRollbackToTargetVersion[] = "RollbackToTargetVersion";

}  // namespace

PolicyManager::PolicyManager(base::Value::Dict policies)
    : policies_(std::move(policies)) {
  constexpr size_t kInstallAppPrefixLength =
      std::string_view(kInstallAppPrefix).length();
  base::ranges::for_each(policies_, [&](const auto& policy) {
    const std::string policy_name = policy.first;
    VLOG_IF(1, policy_name != base::ToLowerASCII(policy_name))
        << "Policy [" << policy_name
        << "] is ignored because it's not all lower-case.";
    if (policy_name.length() <= kInstallAppPrefixLength ||
        !base::StartsWith(policy_name, kInstallAppPrefix,
                          base::CompareCase::INSENSITIVE_ASCII) ||
        base::StartsWith(policy_name, kInstallAppsDefault,
                         base::CompareCase::INSENSITIVE_ASCII) ||
        !policy.second.is_int()) {
      return;
    }

    if (policy.second.GetInt() != (IsSystemInstall()
                                       ? kPolicyForceInstallMachine
                                       : kPolicyForceInstallUser)) {
      return;
    }

    force_install_apps_.push_back(policy_name.substr(kInstallAppPrefixLength));
  });
}

PolicyManager::~PolicyManager() = default;

std::optional<bool> PolicyManager::CloudPolicyOverridesPlatformPolicy() const {
  std::optional<int> policy =
      GetIntegerPolicy(kCloudPolicyOverridesPlatformPolicy);
  return policy ? std::optional<bool>(policy.value()) : std::nullopt;
}

bool PolicyManager::HasActiveDevicePolicies() const {
  return !policies_.empty();
}

std::string PolicyManager::source() const {
  return kSourceDictValuesPolicyManager;
}

std::optional<base::TimeDelta> PolicyManager::GetLastCheckPeriod() const {
  std::optional<int> minutes =
      GetIntegerPolicy(kAutoUpdateCheckPeriodOverrideMinutes);
  if (!minutes) {
    return std::nullopt;
  }
  return base::Minutes(*minutes);
}

std::optional<UpdatesSuppressedTimes> PolicyManager::GetUpdatesSuppressedTimes()
    const {
  std::optional<int> start_hour = GetIntegerPolicy(kUpdatesSuppressedStartHour);
  std::optional<int> start_min = GetIntegerPolicy(kUpdatesSuppressedStartMin);
  std::optional<int> duration_min =
      GetIntegerPolicy(kUpdatesSuppressedDurationMin);

  if (!start_hour || !start_min || !duration_min) {
    return std::nullopt;
  }

  UpdatesSuppressedTimes supressed_times;
  supressed_times.start_hour_ = start_hour.value();
  supressed_times.start_minute_ = start_min.value();
  supressed_times.duration_minute_ = duration_min.value();
  return supressed_times;
}

std::optional<std::string> PolicyManager::GetDownloadPreference() const {
  return GetStringPolicy(kDownloadPreference);
}

std::optional<int> PolicyManager::GetPackageCacheSizeLimitMBytes() const {
  return GetIntegerPolicy(kCacheSizeLimitMBytes);
}

std::optional<int> PolicyManager::GetPackageCacheExpirationTimeDays() const {
  return GetIntegerPolicy(kCacheLifeLimitDays);
}

std::optional<int> PolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  std::string app_value_name(kInstallAppPrefix);
  app_value_name.append(app_id);
  std::optional<int> policy = GetIntegerPolicy(app_value_name);
  return policy ? policy : GetIntegerPolicy(kInstallAppsDefault);
}

std::optional<int> PolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  std::string app_value_name(kUpdateAppPrefix);
  app_value_name.append(app_id);
  std::optional<int> policy = GetIntegerPolicy(app_value_name);
  return policy ? policy : GetIntegerPolicy(kUpdateAppsDefault);
}

std::optional<std::string> PolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  std::string app_value_name(kTargetChannel);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str());
}

std::optional<std::string> PolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  std::string app_value_name(kTargetVersionPrefix);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str());
}

std::optional<bool> PolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  std::string app_value_name(kRollbackToTargetVersion);
  app_value_name.append(app_id);
  std::optional<int> policy = GetIntegerPolicy(app_value_name);
  return policy ? std::optional<bool>(policy.value()) : std::nullopt;
}

std::optional<std::string> PolicyManager::GetProxyMode() const {
  return GetStringPolicy(kProxyMode);
}

std::optional<std::string> PolicyManager::GetProxyPacUrl() const {
  return GetStringPolicy(kProxyPacUrl);
}

std::optional<std::string> PolicyManager::GetProxyServer() const {
  return GetStringPolicy(kProxyServer);
}

std::optional<std::vector<std::string>> PolicyManager::GetForceInstallApps()
    const {
  return force_install_apps_.empty() ? std::optional<std::vector<std::string>>()
                                     : force_install_apps_;
}

std::optional<std::vector<std::string>> PolicyManager::GetAppsWithPolicy()
    const {
  const base::flat_set<std::string> prefixed_policy_names = {
      // prefixed by kUpdateAppPrefix:
      base::ToLowerASCII(kUpdatesSuppressedStartHour),
      base::ToLowerASCII(kUpdatesSuppressedStartMin),
      base::ToLowerASCII(kUpdatesSuppressedDurationMin),
  };
  static constexpr const char* kAppPolicyPrefixes[] = {
      kInstallAppsDefault,     kInstallAppPrefix,    kUpdateAppsDefault,
      kUpdateAppPrefix,        kTargetVersionPrefix, kTargetChannel,
      kRollbackToTargetVersion};
  std::vector<std::string> apps_with_policy;
  base::ranges::for_each(policies_, [&](const auto& policy) {
    const std::string policy_name = policy.first;
    base::ranges::for_each(kAppPolicyPrefixes, [&](const auto& prefix) {
      if (base::StartsWith(policy_name, base::ToLowerASCII(prefix)) &&
          !prefixed_policy_names.contains(policy_name)) {
        apps_with_policy.push_back(
            policy_name.substr(std::string_view(prefix).length()));
      }
    });
  });

  return apps_with_policy;
}

std::optional<int> PolicyManager::GetIntegerPolicy(
    const std::string& key) const {
  return policies_.FindInt(base::ToLowerASCII(key));
}

std::optional<std::string> PolicyManager::GetStringPolicy(
    const std::string& key) const {
  const std::string* policy = policies_.FindString(base::ToLowerASCII(key));
  return policy ? std::make_optional(*policy) : std::nullopt;
}
}  // namespace updater
