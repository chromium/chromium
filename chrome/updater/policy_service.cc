// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/updater/win/group_policy_manager.h"
#elif defined(OS_MAC)
#include "chrome/updater/mac/managed_preference_policy_manager.h"
#endif

namespace updater {

// Only policy managers that are enterprise managed are used by the policy
// service.
PolicyService::PolicyService() {
#if defined(OS_WIN)
  InsertPolicyManager(std::make_unique<GroupPolicyManager>());
#endif
  // TODO(crbug/1122118): Inject the DMPolicyManager here.
#if defined(OS_MAC)
  InsertPolicyManager(CreateManagedPreferencePolicyManager());
#endif
  InsertPolicyManager(GetPolicyManager());
}

PolicyService::~PolicyService() = default;

void PolicyService::InsertPolicyManager(
    std::unique_ptr<PolicyManagerInterface> manager) {
  if (manager->IsManaged()) {
    for (auto it = policy_managers_.begin(); it != policy_managers_.end();
         ++it) {
      if (!(*it)->IsManaged()) {
        policy_managers_.insert(it, std::move(manager));
        return;
      }
    }
  }

  policy_managers_.push_back(std::move(manager));
}

void PolicyService::SetPolicyManagersForTesting(
    std::vector<std::unique_ptr<PolicyManagerInterface>> managers) {
  // Testing managers are not inserted via InsertPolicyManager(). Do a
  // quick sanity check that all managed providers are ahead of non-managed
  // providers: there should be no adjacent pair with the reversed order.
  DCHECK(std::adjacent_find(managers.begin(), managers.end(),
                            [](const auto& fst, const auto& snd) {
                              return !fst->IsManaged() && snd->IsManaged();
                            }) == managers.end());

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
    status.AddPolicyIfNeeded(policy_manager->IsManaged(),
                             policy_manager->source(), value);
  }
  if (!status.effective_policy())
    return false;

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
    status.AddPolicyIfNeeded(policy_manager->IsManaged(),
                             policy_manager->source(), value);
  }
  if (!status.effective_policy())
    return false;

  *result = status.effective_policy().value().policy;
  if (policy_status)
    *policy_status = status;
  return true;
}

std::unique_ptr<PolicyService> GetUpdaterPolicyService() {
  return std::make_unique<PolicyService>();
}

}  // namespace updater
