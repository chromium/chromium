// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_SERVICE_H_
#define CHROME_UPDATER_POLICY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/updater/policy_manager.h"

namespace updater {

// The PolicyService returns policies for enterprise managed machines from the
// source with the highest priority where the policy available.
class PolicyService : public PolicyManagerInterface {
 public:
  PolicyService();
  PolicyService(const PolicyService&) = delete;
  PolicyService& operator=(const PolicyService&) = delete;
  ~PolicyService() override;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool IsManaged() const override;

  bool GetLastCheckPeriodMinutes(int* minutes) const override;
  bool GetUpdatesSuppressedTimes(int* start_hour,
                                 int* start_min,
                                 int* duration_min) const override;
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

  const std::vector<std::unique_ptr<PolicyManagerInterface>>&
  policy_managers() {
    return policy_managers_;
  }

  void SetPolicyManagersForTesting(
      std::vector<std::unique_ptr<PolicyManagerInterface>> managers);
  const PolicyManagerInterface& GetActivePolicyManager();

 private:
  bool ShouldFallbackToDefaultManager() const;

  // Sets the policy manager that is managed and has the highest priority as the
  // active policy manager. If no manager is managed, use the default policy
  // manager as the active one.
  void UpdateActivePolicyManager();
  // List of policy managers in descending order of priority. The first policy
  // manager's policies takes precedence over the following.
  std::vector<std::unique_ptr<PolicyManagerInterface>> policy_managers_;
  std::unique_ptr<PolicyManagerInterface> default_policy_manager_;
  const PolicyManagerInterface* active_policy_manager_;
};

std::unique_ptr<PolicyService> GetUpdaterPolicyService();

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_SERVICE_H_
