// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy_manager.h"

namespace updater {

// TODO: crbug 1070833.
// The DefaultPolicyManager is just a stub manager at the moment that returns
// |false| or failure for all methods. This policy manager would be in effect
// when policies are not effective for any other policy manager in the system.
// It is expected that this policy manager will return default values instead of
// failure in the future.
class DefaultPolicyManager : public PolicyManagerInterface {
 public:
  DefaultPolicyManager();
  DefaultPolicyManager(const DefaultPolicyManager&) = delete;
  DefaultPolicyManager& operator=(const DefaultPolicyManager&) = delete;
  ~DefaultPolicyManager() override;

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
};

DefaultPolicyManager::DefaultPolicyManager() = default;

DefaultPolicyManager::~DefaultPolicyManager() = default;

bool DefaultPolicyManager::IsManaged() const {
  return true;
}

std::string DefaultPolicyManager::source() const {
  return std::string("default");
}

bool DefaultPolicyManager::GetLastCheckPeriodMinutes(int* minutes) const {
  return false;
}

bool DefaultPolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* suppressed_times) const {
  return false;
}

bool DefaultPolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  return false;
}

bool DefaultPolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return false;
}

bool DefaultPolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return false;
}

bool DefaultPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  return false;
}

bool DefaultPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    int* update_policy) const {
  return false;
}

bool DefaultPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  return false;
}

bool DefaultPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  return false;
}

bool DefaultPolicyManager::GetProxyMode(std::string* proxy_mode) const {
  return false;
}

bool DefaultPolicyManager::GetProxyPacUrl(std::string* proxy_pac_url) const {
  return false;
}

bool DefaultPolicyManager::GetProxyServer(std::string* proxy_server) const {
  return false;
}

bool DefaultPolicyManager::GetTargetChannel(const std::string& app_id,
                                            std::string* channel) const {
  return false;
}

std::unique_ptr<PolicyManagerInterface> GetPolicyManager() {
  return std::make_unique<DefaultPolicyManager>();
}

}  // namespace updater
