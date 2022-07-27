// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_POLICY_MANAGER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/updater/policy/manager.h"

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
  ~PolicyManager() override;

  // Overrides for PolicyManagerInterface.
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
  bool GetTargetChannel(const std::string& app_id,
                        std::string* channel) const override;
  bool GetTargetVersionPrefix(
      const std::string& app_id,
      std::string* target_version_prefix) const override;
  bool IsRollbackToTargetVersionAllowed(const std::string& app_id,
                                        bool* rollback_allowed) const override;
  bool GetProxyMode(std::string* proxy_mode) const override;
  bool GetProxyPacUrl(std::string* proxy_pac_url) const override;
  bool GetProxyServer(std::string* proxy_server) const override;

  bool GetForceInstallApps(
      std::vector<std::string>* force_install_apps) const override;

 private:
  bool GetIntPolicy(const std::string& key, int* value) const;
  bool GetStringPolicy(const std::string& key, std::string* value) const;

  const base::Value::Dict policies_;
  std::vector<std::string> force_install_apps_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_POLICY_MANAGER_H_
