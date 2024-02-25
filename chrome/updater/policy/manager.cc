// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/manager.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"

namespace updater {

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

  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  std::optional<bool> CloudPolicyOverridesPlatformPolicy() const override;
  std::optional<base::TimeDelta> GetLastCheckPeriod() const override;
  std::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override;
  std::optional<std::string> GetDownloadPreference() const override;
  std::optional<int> GetPackageCacheSizeLimitMBytes() const override;
  std::optional<int> GetPackageCacheExpirationTimeDays() const override;
  std::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override;
  std::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override;
  std::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override;
  std::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override;
  std::optional<std::string> GetProxyMode() const override;
  std::optional<std::string> GetProxyPacUrl() const override;
  std::optional<std::string> GetProxyServer() const override;
  std::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override;
  std::optional<std::vector<std::string>> GetForceInstallApps() const override;
  std::optional<std::vector<std::string>> GetAppsWithPolicy() const override;

 private:
  ~DefaultValuesPolicyManager() override;
};

DefaultValuesPolicyManager::DefaultValuesPolicyManager() = default;

DefaultValuesPolicyManager::~DefaultValuesPolicyManager() = default;

bool DefaultValuesPolicyManager::HasActiveDevicePolicies() const {
  return true;
}

std::string DefaultValuesPolicyManager::source() const {
  return kSourceDefaultValuesPolicyManager;
}

std::optional<bool>
DefaultValuesPolicyManager::CloudPolicyOverridesPlatformPolicy() const {
  return std::nullopt;
}

std::optional<base::TimeDelta> DefaultValuesPolicyManager::GetLastCheckPeriod()
    const {
  return kDefaultLastCheckPeriod;
}

std::optional<UpdatesSuppressedTimes>
DefaultValuesPolicyManager::GetUpdatesSuppressedTimes() const {
  return std::nullopt;
}

std::optional<std::string> DefaultValuesPolicyManager::GetDownloadPreference()
    const {
  return std::nullopt;
}

std::optional<int> DefaultValuesPolicyManager::GetPackageCacheSizeLimitMBytes()
    const {
  return std::nullopt;
}

std::optional<int>
DefaultValuesPolicyManager::GetPackageCacheExpirationTimeDays() const {
  return std::nullopt;
}

std::optional<int> DefaultValuesPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  return kInstallPolicyDefault;
}

std::optional<int> DefaultValuesPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  return kUpdatePolicyDefault;
}

std::optional<std::string> DefaultValuesPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  return std::nullopt;
}

std::optional<bool>
DefaultValuesPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  return false;
}

std::optional<std::string> DefaultValuesPolicyManager::GetProxyMode() const {
  return std::nullopt;
}

std::optional<std::string> DefaultValuesPolicyManager::GetProxyPacUrl() const {
  return std::nullopt;
}

std::optional<std::string> DefaultValuesPolicyManager::GetProxyServer() const {
  return std::nullopt;
}

std::optional<std::string> DefaultValuesPolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  return std::nullopt;
}

std::optional<std::vector<std::string>>
DefaultValuesPolicyManager::GetForceInstallApps() const {
  return std::nullopt;
}

std::optional<std::vector<std::string>>
DefaultValuesPolicyManager::GetAppsWithPolicy() const {
  return std::nullopt;
}

scoped_refptr<PolicyManagerInterface> GetDefaultValuesPolicyManager() {
  return base::MakeRefCounted<DefaultValuesPolicyManager>();
}

}  // namespace updater
