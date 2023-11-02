// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/policy/policy_fetcher.h"
#include "chrome/updater/policy/policy_manager.h"
#if BUILDFLAG(IS_WIN)
#include "chrome/updater/policy/win/group_policy_manager.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/updater/policy/mac/managed_preference_policy_manager.h"
#endif
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

// Sorts the managed policy managers ahead of the non-managed ones.
PolicyService::PolicyManagerVector SortManagers(
    PolicyService::PolicyManagerVector managers) {
  base::ranges::stable_sort(
      managers, [](const std::unique_ptr<PolicyManagerInterface>& lhs,
                   const std::unique_ptr<PolicyManagerInterface>& rhs) {
        return lhs->HasActiveDevicePolicies() &&
               !rhs->HasActiveDevicePolicies();
      });

  return managers;
}

PolicyService::PolicyManagerVector CreatePolicyManagerVector(
    scoped_refptr<ExternalConstants> external_constants,
    std::unique_ptr<PolicyManagerInterface> dm_policy_manager) {
  PolicyService::PolicyManagerVector managers;
  if (external_constants) {
    managers.push_back(
        std::make_unique<PolicyManager>(external_constants->GroupPolicies()));
  }

#if BUILDFLAG(IS_WIN)
  managers.push_back(std::make_unique<GroupPolicyManager>());
#endif

  if (!dm_policy_manager)
    dm_policy_manager = CreateDMPolicyManager();
  if (dm_policy_manager)
    managers.push_back(std::move(dm_policy_manager));

#if BUILDFLAG(IS_MAC)
  // Managed preference policy manager is being deprecated and thus has a lower
  // priority than DM policy manager.
  managers.push_back(CreateManagedPreferencePolicyManager());
#endif

  managers.push_back(GetDefaultValuesPolicyManager());

  return managers;
}

}  // namespace

PolicyService::PolicyService(PolicyManagerVector managers)
    : policy_managers_(SortManagers(std::move(managers))) {}

PolicyService::PolicyService(
    scoped_refptr<ExternalConstants> external_constants)
    : policy_managers_(
          SortManagers(CreatePolicyManagerVector(external_constants, nullptr))),
      external_constants_(external_constants),
      policy_fetcher_(base::MakeRefCounted<PolicyFetcher>(this)) {}

PolicyService::~PolicyService() = default;

void PolicyService::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  policy_fetcher_->FetchPolicies(base::BindOnce(
      &PolicyService::FetchPoliciesDone, this, std::move(callback)));
}

void PolicyService::FetchPoliciesDone(
    base::OnceCallback<void(int)> callback,
    int result,
    std::unique_ptr<PolicyManagerInterface> dm_policy_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  if (dm_policy_manager) {
    policy_managers_ = SortManagers(CreatePolicyManagerVector(
        external_constants_, std::move(dm_policy_manager)));
  }

  std::move(callback).Run(result);
}

std::string PolicyService::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetLastCheckPeriodMinutes),
      policy_status, minutes);
}

bool PolicyService::GetUpdatesSuppressedTimes(
    PolicyStatus<UpdatesSuppressedTimes>* policy_status,
    UpdatesSuppressedTimes* suppressed_times) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetUpdatesSuppressedTimes),
      policy_status, suppressed_times);
}

bool PolicyService::GetDownloadPreferenceGroupPolicy(
    PolicyStatus<std::string>* policy_status,
    std::string* download_preference) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetDownloadPreferenceGroupPolicy),
      policy_status, download_preference);
}

bool PolicyService::GetPackageCacheSizeLimitMBytes(
    PolicyStatus<int>* policy_status,
    int* cache_size_limit) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetPackageCacheSizeLimitMBytes),
      policy_status, cache_size_limit);
}

bool PolicyService::GetPackageCacheExpirationTimeDays(
    PolicyStatus<int>* policy_status,
    int* cache_life_limit) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetPackageCacheExpirationTimeDays),
      policy_status, cache_life_limit);
}

bool PolicyService::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    PolicyStatus<int>* policy_status,
    int* install_policy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetEffectivePolicyForAppInstalls),
      app_id, policy_status, install_policy);
}

bool PolicyService::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    PolicyStatus<int>* policy_status,
    int* update_policy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetEffectivePolicyForAppUpdates),
      app_id, policy_status, update_policy);
}

bool PolicyService::GetTargetChannel(const std::string& app_id,
                                     PolicyStatus<std::string>* policy_status,
                                     std::string* channel) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetTargetChannel), app_id,
      policy_status, channel);
}

bool PolicyService::GetTargetVersionPrefix(
    const std::string& app_id,
    PolicyStatus<std::string>* policy_status,
    std::string* target_version_prefix) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetTargetVersionPrefix),
      app_id, policy_status, target_version_prefix);
}

bool PolicyService::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    PolicyStatus<bool>* policy_status,
    bool* rollback_allowed) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::IsRollbackToTargetVersionAllowed),
      app_id, policy_status, rollback_allowed);
}

bool PolicyService::GetProxyMode(PolicyStatus<std::string>* policy_status,
                                 std::string* proxy_mode) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(base::BindRepeating(&PolicyManagerInterface::GetProxyMode),
                     policy_status, proxy_mode);
}

bool PolicyService::GetProxyPacUrl(PolicyStatus<std::string>* policy_status,
                                   std::string* proxy_pac_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyPacUrl),
      policy_status, proxy_pac_url);
}

bool PolicyService::GetProxyServer(PolicyStatus<std::string>* policy_status,
                                   std::string* proxy_server) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyServer),
      policy_status, proxy_server);
}

bool PolicyService::GetForceInstallApps(
    PolicyStatus<std::vector<std::string>>* policy_status,
    std::vector<std::string>* force_install_apps) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

PolicyServiceProxyConfiguration::PolicyServiceProxyConfiguration() = default;
PolicyServiceProxyConfiguration::~PolicyServiceProxyConfiguration() = default;
PolicyServiceProxyConfiguration::PolicyServiceProxyConfiguration(
    const PolicyServiceProxyConfiguration&) = default;
PolicyServiceProxyConfiguration& PolicyServiceProxyConfiguration::operator=(
    const PolicyServiceProxyConfiguration&) = default;

absl::optional<PolicyServiceProxyConfiguration>
PolicyServiceProxyConfiguration::Get(
    scoped_refptr<PolicyService> policy_service) {
  std::string policy_proxy_mode;
  if (!policy_service->GetProxyMode(nullptr, &policy_proxy_mode) ||
      policy_proxy_mode.compare(kProxyModeSystem) == 0) {
    return absl::nullopt;
  }
  VLOG(2) << "Using policy proxy " << policy_proxy_mode;

  PolicyServiceProxyConfiguration policy_service_proxy_configuration;

  bool is_policy_config_valid = true;
  if (policy_proxy_mode.compare(kProxyModeFixedServers) == 0) {
    std::string policy_proxy_url;
    if (!policy_service->GetProxyServer(nullptr, &policy_proxy_url)) {
      VLOG(1) << "Fixed server mode proxy has no URL specified.";
      is_policy_config_valid = false;
    } else {
      policy_service_proxy_configuration.proxy_url = policy_proxy_url;
    }
  } else if (policy_proxy_mode.compare(kProxyModePacScript) == 0) {
    std::string policy_proxy_pac_url;
    if (!policy_service->GetProxyPacUrl(nullptr, &policy_proxy_pac_url)) {
      VLOG(1) << "PAC proxy policy has no PAC URL specified.";
      is_policy_config_valid = false;
    } else {
      policy_service_proxy_configuration.proxy_pac_url = policy_proxy_pac_url;
    }
  } else if (policy_proxy_mode.compare(kProxyModeAutoDetect)) {
    policy_service_proxy_configuration.proxy_auto_detect = true;
  }

  if (!is_policy_config_valid) {
    VLOG(1) << "Configuration set by policy was invalid.";
    return absl::nullopt;
  }

  return policy_service_proxy_configuration;
}

}  // namespace updater
