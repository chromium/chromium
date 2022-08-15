// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/manager.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/updater/constants.h"

namespace updater {

UpdatesSuppressedTimes::UpdatesSuppressedTimes() = default;

UpdatesSuppressedTimes::~UpdatesSuppressedTimes() = default;

bool UpdatesSuppressedTimes::operator==(
    const UpdatesSuppressedTimes& other) const {
  return start_hour_ == other.start_hour_ &&
         start_minute_ == other.start_minute_ &&
         duration_minute_ == other.duration_minute_;
}

bool UpdatesSuppressedTimes::operator!=(
    const UpdatesSuppressedTimes& other) const {
  return !(*this == other);
}

bool UpdatesSuppressedTimes::valid() const {
  return start_hour_ != kPolicyNotSet && start_minute_ != kPolicyNotSet &&
         duration_minute_ != kPolicyNotSet;
}

bool UpdatesSuppressedTimes::contains(int hour, int minute) const {
  int elapsed_minutes = (hour - start_hour_) * 60 + (minute - start_minute_);
  if (elapsed_minutes >= 0 && elapsed_minutes < duration_minute_) {
    // The given time is in the suppression period that started today.
    // This can be off by up to an hour on the day of a DST transition.
    return true;
  }
  if (elapsed_minutes < 0 && elapsed_minutes + 60 * 24 < duration_minute_) {
    // The given time is in the suppression period that started yesterday.
    // This can be off by up to an hour on the day after a DST transition.
    return true;
  }
  return false;
}

// DefaultValuesPolicyManager returns the default values for policies when no
// other policy manager is present in the system.
class DefaultValuesPolicyManager : public PolicyManagerInterface {
 public:
  DefaultValuesPolicyManager();
  DefaultValuesPolicyManager(const DefaultValuesPolicyManager&) = delete;
  DefaultValuesPolicyManager& operator=(const DefaultValuesPolicyManager&) =
      delete;
  ~DefaultValuesPolicyManager() override;

  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  bool GetLastCheckPeriodMinutes(int* minutes) const override;
  bool GetUpdatesSuppressedTimes(
      UpdatesSuppressedTimes* suppressed_times) const override;
  bool GetDownloadPreferenceGroupPolicy(
      std::string* download_preference) const override;
  bool GetPackageCacheSizeLimitMBytes(int* cache_size_limit) const override;
  bool GetPackageCacheExpirationTimeDays(int* cache_life_limit) const override;

  bool GetEffectivePolicyForAppInstalls(const std::string& app_id,
                                        int* install_policy) const override;
  bool GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                       int* update_policy) const override;
  bool GetTargetVersionPrefix(
      const std::string& app_id,
      std::string* target_version_prefix) const override;
  bool IsRollbackToTargetVersionAllowed(const std::string& app_id,
                                        bool* rollback_allowed) const override;
  bool GetProxyMode(std::string* proxy_mode) const override;
  bool GetProxyPacUrl(std::string* proxy_pac_url) const override;
  bool GetProxyServer(std::string* proxy_server) const override;
  bool GetTargetChannel(const std::string& app_id,
                        std::string* channel) const override;
  bool GetForceInstallApps(
      std::vector<std::string>* force_install_apps) const override;
};

DefaultValuesPolicyManager::DefaultValuesPolicyManager() = default;

DefaultValuesPolicyManager::~DefaultValuesPolicyManager() = default;

bool DefaultValuesPolicyManager::HasActiveDevicePolicies() const {
  return true;
}

std::string DefaultValuesPolicyManager::source() const {
  return std::string("default");
}

bool DefaultValuesPolicyManager::GetLastCheckPeriodMinutes(int* minutes) const {
  *minutes = kDefaultLastCheckPeriod.InMinutes();
  return true;
}

bool DefaultValuesPolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* suppressed_times) const {
  return false;
}

bool DefaultValuesPolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  return false;
}

bool DefaultValuesPolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return false;
}

bool DefaultValuesPolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return false;
}

bool DefaultValuesPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  *install_policy = kInstallPolicyDefault;
  return true;
}

bool DefaultValuesPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    int* update_policy) const {
  *update_policy = kUpdatePolicyDefault;
  return true;
}

bool DefaultValuesPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  return false;
}

bool DefaultValuesPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  *rollback_allowed = false;
  return true;
}

bool DefaultValuesPolicyManager::GetProxyMode(std::string* proxy_mode) const {
  return false;
}

bool DefaultValuesPolicyManager::GetProxyPacUrl(
    std::string* proxy_pac_url) const {
  return false;
}

bool DefaultValuesPolicyManager::GetProxyServer(
    std::string* proxy_server) const {
  return false;
}

bool DefaultValuesPolicyManager::GetTargetChannel(const std::string& app_id,
                                                  std::string* channel) const {
  return false;
}

bool DefaultValuesPolicyManager::GetForceInstallApps(
    std::vector<std::string>* /* force_install_apps */) const {
  return false;
}

std::unique_ptr<PolicyManagerInterface> GetDefaultValuesPolicyManager() {
  return std::make_unique<DefaultValuesPolicyManager>();
}

}  // namespace updater
