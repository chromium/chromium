// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_POLICY_MANAGER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/updater/policy/manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// A policy manager that holds all policies in-memory. Main purposes for this
// class:
//   1) Provides a way for policy override, esp. for testing.
//   2) Cache policies for those providers when loading policies is expensive.
//
class PolicyManager : public PolicyManagerInterface {
 public:
  explicit PolicyManager(base::Value::Dict policies);
  PolicyManager(const PolicyManager&) = delete;
  PolicyManager& operator=(const PolicyManager&) = delete;

  absl::optional<int> GetIntegerPolicy(const std::string& key) const;
  absl::optional<std::string> GetStringPolicy(const std::string& key) const;

  // Overrides for PolicyManagerInterface.
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
  absl::optional<std::vector<std::string>> GetAppsWithPolicy() const override;

 protected:
  ~PolicyManager() override;

 private:
  const base::Value::Dict policies_;
  std::vector<std::string> force_install_apps_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_POLICY_MANAGER_H_
