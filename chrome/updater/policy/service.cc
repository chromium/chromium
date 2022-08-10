// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/updater/external_constants.h"

#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/policy/policy_manager.h"
#if BUILDFLAG(IS_WIN)
#include "chrome/updater/policy/win/group_policy_manager.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/updater/policy/mac/managed_preference_policy_manager.h"
#endif

namespace updater {

PolicyService::PolicyService(PolicyManagerVector managers)
    : policy_managers_([](auto managers) {
        // Make sure managed policy managers are ahead of non-managed ones.
        std::stable_sort(
            managers.begin(), managers.end(),
            [](const std::unique_ptr<PolicyManagerInterface>& lhs,
               const std::unique_ptr<PolicyManagerInterface>& rhs) {
              return lhs->HasActiveDevicePolicies() &&
                     !rhs->HasActiveDevicePolicies();
            });
        return managers;
      }(std::move(managers))) {}

PolicyService::~PolicyService() = default;

std::string PolicyService::source() const {
  // Returns the non-empty source combination of all active policy providers,
  // separated by ';'. For example: "group_policy;device_management".
  std::vector<std::string> sources;
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (policy_manager->HasActiveDevicePolicies() &&
        !policy_manager->source().empty()) {
      sources.push_back(policy_manager->source());
    }
  }
  return base::JoinString(sources, ";");
}

bool PolicyService::GetLastCheckPeriodMinutes(PolicyStatus<int>* policy_status,
                                              int* minutes) const {
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetLastCheckPeriodMinutes),
      policy_status, minutes);
}

bool PolicyService::GetUpdatesSuppressedTimes(
    PolicyStatus<UpdatesSuppressedTimes>* policy_status,
    UpdatesSuppressedTimes* suppressed_times) const {
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetUpdatesSuppressedTimes),
      policy_status, suppressed_times);
}

bool PolicyService::GetDownloadPreferenceGroupPolicy(
    PolicyStatus<std::string>* policy_status,
    std::string* download_preference) const {
  return QueryPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetDownloadPreferenceGroupPolicy),
      policy_status, download_preference);
}

bool PolicyService::GetPackageCacheSizeLimitMBytes(
    PolicyStatus<int>* policy_status,
    int* cache_size_limit) const {
  return QueryPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetPackageCacheSizeLimitMBytes),
      policy_status, cache_size_limit);
}

bool PolicyService::GetPackageCacheExpirationTimeDays(
    PolicyStatus<int>* policy_status,
    int* cache_life_limit) const {
  return QueryPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetPackageCacheExpirationTimeDays),
      policy_status, cache_life_limit);
}

bool PolicyService::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    PolicyStatus<int>* policy_status,
    int* install_policy) const {
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetEffectivePolicyForAppInstalls),
      app_id, policy_status, install_policy);
}

bool PolicyService::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    PolicyStatus<int>* policy_status,
    int* update_policy) const {
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetEffectivePolicyForAppUpdates),
      app_id, policy_status, update_policy);
}

bool PolicyService::GetTargetChannel(const std::string& app_id,
                                     PolicyStatus<std::string>* policy_status,
                                     std::string* channel) const {
  return QueryAppPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetTargetChannel), app_id,
      policy_status, channel);
}

bool PolicyService::GetTargetVersionPrefix(
    const std::string& app_id,
    PolicyStatus<std::string>* policy_status,
    std::string* target_version_prefix) const {
  return QueryAppPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetTargetVersionPrefix),
      app_id, policy_status, target_version_prefix);
}

bool PolicyService::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    PolicyStatus<bool>* policy_status,
    bool* rollback_allowed) const {
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::IsRollbackToTargetVersionAllowed),
      app_id, policy_status, rollback_allowed);
}

bool PolicyService::GetProxyMode(PolicyStatus<std::string>* policy_status,
                                 std::string* proxy_mode) const {
  return QueryPolicy(base::BindRepeating(&PolicyManagerInterface::GetProxyMode),
                     policy_status, proxy_mode);
}

bool PolicyService::GetProxyPacUrl(PolicyStatus<std::string>* policy_status,
                                   std::string* proxy_pac_url) const {
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyPacUrl),
      policy_status, proxy_pac_url);
}

bool PolicyService::GetProxyServer(PolicyStatus<std::string>* policy_status,
                                   std::string* proxy_server) const {
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyServer),
      policy_status, proxy_server);
}

bool PolicyService::GetForceInstallApps(
    PolicyStatus<std::vector<std::string>>* policy_status,
    std::vector<std::string>* force_install_apps) const {
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetForceInstallApps),
      policy_status, force_install_apps);
}

template <typename T>
bool PolicyService::QueryPolicy(
    const base::RepeatingCallback<bool(const PolicyManagerInterface*, T*)>&
        policy_query_callback,
    PolicyStatus<T>* policy_status,
    T* result) const {
  T value{};
  PolicyStatus<T> status;
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (!policy_query_callback.Run(policy_manager.get(), &value))
      continue;
    status.AddPolicyIfNeeded(policy_manager->HasActiveDevicePolicies(),
                             policy_manager->source(), value);
  }
  if (!status.effective_policy())
    return false;

  if (result)
    *result = status.effective_policy().value().policy;
  if (policy_status)
    *policy_status = status;
  return true;
}

template <typename T>
bool PolicyService::QueryAppPolicy(
    const base::RepeatingCallback<bool(const PolicyManagerInterface*,
                                       const std::string&,
                                       T*)>& policy_query_callback,
    const std::string& app_id,
    PolicyStatus<T>* policy_status,
    T* result) const {
  T value{};
  PolicyStatus<T> status;
  for (const std::unique_ptr<PolicyManagerInterface>& policy_manager :
       policy_managers_) {
    if (!policy_query_callback.Run(policy_manager.get(), app_id, &value))
      continue;
    status.AddPolicyIfNeeded(policy_manager->HasActiveDevicePolicies(),
                             policy_manager->source(), value);
  }
  if (!status.effective_policy())
    return false;

  if (result)
    *result = status.effective_policy().value().policy;
  if (policy_status)
    *policy_status = status;
  return true;
}

scoped_refptr<PolicyService> PolicyService::Create(
    scoped_refptr<ExternalConstants> external_constants) {
  PolicyManagerVector managers;
  managers.push_back(
      std::make_unique<PolicyManager>(external_constants->GroupPolicies()));
#if BUILDFLAG(IS_WIN)
  managers.push_back(std::make_unique<GroupPolicyManager>());
#endif
  std::unique_ptr<PolicyManagerInterface> dm_policy_manager =
      CreateDMPolicyManager();
  if (dm_policy_manager)
    managers.push_back(std::move(dm_policy_manager));
#if BUILDFLAG(IS_MAC)
  // Managed preference policy manager is being deprecated and thus has a lower
  // priority than DM policy manager.
  managers.push_back(CreateManagedPreferencePolicyManager());
#endif
  managers.push_back(GetDefaultValuesPolicyManager());

  return base::MakeRefCounted<PolicyService>(std::move(managers));
}

}  // namespace updater
