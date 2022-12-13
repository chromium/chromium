// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/manager.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  absl::optional<base::TimeDelta> GetLastCheckPeriod() const override;
  absl::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override;
  absl::optional<std::string> GetDownloadPreferenceGroupPolicy() const override;
  absl::optional<int> GetPackageCacheSizeLimitMBytes() const override;
  absl::optional<int> GetPackageCacheExpirationTimeDays() const override;
  absl::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override;
  absl::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override;
  absl::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override;
  absl::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override;
  absl::optional<std::string> GetProxyMode() const override;
  absl::optional<std::string> GetProxyPacUrl() const override;
  absl::optional<std::string> GetProxyServer() const override;
  absl::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override;
  absl::optional<std::vector<std::string>> GetForceInstallApps() const override;
};

DefaultValuesPolicyManager::DefaultValuesPolicyManager() = default;

DefaultValuesPolicyManager::~DefaultValuesPolicyManager() = default;

bool DefaultValuesPolicyManager::HasActiveDevicePolicies() const {
  return true;
}

std::string DefaultValuesPolicyManager::source() const {
  return std::string("default");
}

absl::optional<base::TimeDelta> DefaultValuesPolicyManager::GetLastCheckPeriod()
    const {
  return kDefaultLastCheckPeriod;
}

absl::optional<UpdatesSuppressedTimes>
DefaultValuesPolicyManager::GetUpdatesSuppressedTimes() const {
  return absl::nullopt;
}

absl::optional<std::string>
DefaultValuesPolicyManager::GetDownloadPreferenceGroupPolicy() const {
  return absl::nullopt;
}

absl::optional<int> DefaultValuesPolicyManager::GetPackageCacheSizeLimitMBytes()
    const {
  return absl::nullopt;
}

absl::optional<int>
DefaultValuesPolicyManager::GetPackageCacheExpirationTimeDays() const {
  return absl::nullopt;
}

absl::optional<int>
DefaultValuesPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  return kInstallPolicyDefault;
}

absl::optional<int> DefaultValuesPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  return kUpdatePolicyDefault;
}

absl::optional<std::string> DefaultValuesPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  return absl::nullopt;
}

absl::optional<bool>
DefaultValuesPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  return false;
}

absl::optional<std::string> DefaultValuesPolicyManager::GetProxyMode() const {
  return absl::nullopt;
}

absl::optional<std::string> DefaultValuesPolicyManager::GetProxyPacUrl() const {
  return absl::nullopt;
}

absl::optional<std::string> DefaultValuesPolicyManager::GetProxyServer() const {
  return absl::nullopt;
}

absl::optional<std::string> DefaultValuesPolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  return absl::nullopt;
}

absl::optional<std::vector<std::string>>
DefaultValuesPolicyManager::GetForceInstallApps() const {
  return absl::nullopt;
}

std::unique_ptr<PolicyManagerInterface> GetDefaultValuesPolicyManager() {
  return std::make_unique<DefaultValuesPolicyManager>();
}

}  // namespace updater
