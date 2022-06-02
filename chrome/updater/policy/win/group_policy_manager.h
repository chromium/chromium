// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/policy/manager.h"

namespace updater {

// The GroupPolicyManager returns policies for domain-joined machines.
class GroupPolicyManager : public PolicyManagerInterface {
 public:
  explicit GroupPolicyManager(
      scoped_refptr<ExternalConstants> external_constants);
  GroupPolicyManager(const GroupPolicyManager&) = delete;
  GroupPolicyManager& operator=(const GroupPolicyManager&) = delete;
  ~GroupPolicyManager() override;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool IsManaged() const override;

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

 private:
  bool IsManagedInternal() const;
  void LoadAllPolicies();
  bool GetIntPolicy(const std::string& key, int* value) const;
  bool GetStringPolicy(const std::string& key, std::string* value) const;

  base::Value::Dict policies_;
  const base::Value::Dict external_constants_group_policies_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_
