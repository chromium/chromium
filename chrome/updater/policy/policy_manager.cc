// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_manager.h"

#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

namespace {

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
constexpr char kInstallAppsDefault[] = "InstallDefault";
constexpr char kInstallAppPrefix[] = "Install";
constexpr char kUpdateAppsDefault[] = "UpdateDefault";
constexpr char kUpdateAppPrefix[] = "Update";
constexpr char kTargetVersionPrefix[] = "TargetVersionPrefix";
constexpr char kTargetChannel[] = "TargetChannel";
constexpr char kRollbackToTargetVersion[] = "RollbackToTargetVersion";

}  // namespace

PolicyManager::PolicyManager(base::Value::Dict policies)
    : policies_(std::move(policies)) {
  constexpr size_t kInstallAppPrefixLength =
      base::StringPiece(kInstallAppPrefix).length();
  base::ranges::for_each(policies_, [&](const auto& policy) {
    const std::string policy_name = policy.first;
    if (policy_name.length() <= kInstallAppPrefixLength ||
        !base::StartsWith(policy_name, kInstallAppPrefix) ||
        base::StartsWith(policy_name, kInstallAppsDefault) ||
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

bool PolicyManager::HasActiveDevicePolicies() const {
  return !policies_.empty();
}

std::string PolicyManager::source() const {
  return kSourceDictValuesPolicyManager;
}

absl::optional<base::TimeDelta> PolicyManager::GetLastCheckPeriod() const {
  absl::optional<int> minutes =
      policies_.FindInt(kAutoUpdateCheckPeriodOverrideMinutes);
  if (!minutes)
    return absl::nullopt;
  return base::Minutes(*minutes);
}

absl::optional<UpdatesSuppressedTimes>
PolicyManager::GetUpdatesSuppressedTimes() const {
  absl::optional<int> start_hour =
      policies_.FindInt(kUpdatesSuppressedStartHour);
  absl::optional<int> start_min = policies_.FindInt(kUpdatesSuppressedStartMin);
  absl::optional<int> duration_min =
      policies_.FindInt(kUpdatesSuppressedDurationMin);

  if (!start_hour || !start_min || !duration_min)
    return absl::nullopt;

  UpdatesSuppressedTimes supressed_times;
  supressed_times.start_hour_ = start_hour.value();
  supressed_times.start_minute_ = start_min.value();
  supressed_times.duration_minute_ = duration_min.value();
  return supressed_times;
}

absl::optional<std::string> PolicyManager::GetDownloadPreferenceGroupPolicy()
    const {
  return GetStringPolicy(kDownloadPreference);
}

absl::optional<int> PolicyManager::GetPackageCacheSizeLimitMBytes() const {
  return policies_.FindInt(kCacheSizeLimitMBytes);
}

absl::optional<int> PolicyManager::GetPackageCacheExpirationTimeDays() const {
  return policies_.FindInt(kCacheLifeLimitDays);
}

absl::optional<int> PolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  std::string app_value_name(kInstallAppPrefix);
  app_value_name.append(app_id);
  absl::optional<int> policy = policies_.FindInt(app_value_name.c_str());
  return policy ? policy : policies_.FindInt(kInstallAppsDefault);
}

absl::optional<int> PolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  std::string app_value_name(kUpdateAppPrefix);
  app_value_name.append(app_id);
  absl::optional<int> policy = policies_.FindInt(app_value_name.c_str());
  return policy ? policy : policies_.FindInt(kUpdateAppsDefault);
}

absl::optional<std::string> PolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  std::string app_value_name(kTargetChannel);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str());
}

absl::optional<std::string> PolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  std::string app_value_name(kTargetVersionPrefix);
  app_value_name.append(app_id);
  return GetStringPolicy(app_value_name.c_str());
}

absl::optional<bool> PolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  std::string app_value_name(kRollbackToTargetVersion);
  app_value_name.append(app_id);
  absl::optional<int> policy = policies_.FindInt(app_value_name);
  return policy ? absl::optional<bool>(policy.value()) : absl::nullopt;
}

absl::optional<std::string> PolicyManager::GetProxyMode() const {
  return GetStringPolicy(kProxyMode);
}

absl::optional<std::string> PolicyManager::GetProxyPacUrl() const {
  return GetStringPolicy(kProxyPacUrl);
}

absl::optional<std::string> PolicyManager::GetProxyServer() const {
  return GetStringPolicy(kProxyServer);
}

absl::optional<std::vector<std::string>> PolicyManager::GetForceInstallApps()
    const {
  return force_install_apps_.empty()
             ? absl::optional<std::vector<std::string>>()
             : force_install_apps_;
}

absl::optional<std::string> PolicyManager::GetStringPolicy(
    const std::string& key) const {
  const std::string* policy = policies_.FindString(key);
  return policy ? absl::optional<std::string>(*policy) : absl::nullopt;
}

}  // namespace updater
