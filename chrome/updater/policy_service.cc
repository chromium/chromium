// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy_service.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/updater/win/group_policy_manager.h"
#elif defined(OS_MAC)
#include "chrome/updater/mac/managed_preference_policy_manager.h"
#endif

namespace updater {

// Only policy manager that are enterprise managed are used by the policy
// service.
PolicyService::PolicyService() {
#if defined(OS_WIN)
  auto group_policy_manager = std::make_unique<GroupPolicyManager>();
  if (group_policy_manager->IsManaged())
    policy_managers_.push_back(std::move(group_policy_manager));
#endif
    // TODO (crbug/1122118): Inject the DMPolicyManager here.
#if defined(OS_MAC)
  auto mac_policy_manager = CreateManagedPreferencePolicyManager();
  if (mac_policy_manager->IsManaged())
    policy_managers_.push_back(std::move(mac_policy_manager));
#endif
  policy_managers_.push_back(GetPolicyManager());
}

PolicyService::~PolicyService() = default;

void PolicyService::SetPolicyManagersForTesting(
    std::vector<std::unique_ptr<PolicyManagerInterface>> managers) {
  policy_managers_ = std::move(managers);
}

std::string PolicyService::source() const {
  // Returns the source combination of all active policy providers, separated
  // by ';'. For example: "group_policy;device_management". Note that the
  // default provider is not "managed" and its source will be ignored.
  std::vector<std::string> sources;
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->IsManaged())
      sources.push_back(policy_manager->source());
  }
  return base::JoinString(sources, ";");
}

bool PolicyService::IsManaged() const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->IsManaged())
      return true;
  }

  return false;
}

bool PolicyService::GetLastCheckPeriodMinutes(int* minutes) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetLastCheckPeriodMinutes(minutes))
      return true;
  }

  return false;
}

bool PolicyService::GetUpdatesSuppressedTimes(int* start_hour,
                                              int* start_min,
                                              int* duration_min) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetUpdatesSuppressedTimes(start_hour, start_min,
                                                  duration_min)) {
      return true;
    }
  }
  return false;
}

bool PolicyService::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetDownloadPreferenceGroupPolicy(download_preference))
      return true;
  }
  return false;
}

bool PolicyService::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetPackageCacheSizeLimitMBytes(cache_size_limit))
      return true;
  }
  return false;
}

bool PolicyService::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetPackageCacheExpirationTimeDays(cache_life_limit))
      return true;
  }
  return false;
}

bool PolicyService::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetEffectivePolicyForAppInstalls(app_id,
                                                         install_policy))
      return true;
  }
  return false;
}

bool PolicyService::GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                                    int* update_policy) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetEffectivePolicyForAppUpdates(app_id, update_policy))
      return true;
  }
  return false;
}

bool PolicyService::GetTargetChannel(const std::string& app_id,
                                     std::string* channel) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetTargetChannel(app_id, channel))
      return true;
  }
  return false;
}

bool PolicyService::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetTargetVersionPrefix(app_id, target_version_prefix))
      return true;
  }
  return false;
}

bool PolicyService::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->IsRollbackToTargetVersionAllowed(app_id,
                                                         rollback_allowed))
      return true;
  }
  return false;
}

bool PolicyService::GetProxyMode(std::string* proxy_mode) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetProxyMode(proxy_mode))
      return true;
  }
  return false;
}

bool PolicyService::GetProxyPacUrl(std::string* proxy_pac_url) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetProxyPacUrl(proxy_pac_url))
      return true;
  }
  return false;
}

bool PolicyService::GetProxyServer(std::string* proxy_server) const {
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->GetProxyServer(proxy_server))
      return true;
  }
  return false;
}

std::unique_ptr<PolicyService> GetUpdaterPolicyService() {
  return std::make_unique<PolicyService>();
}

}  // namespace updater
