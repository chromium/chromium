// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
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

// Sorts the managed policy managers ahead of the non-managed ones in the
// vector, and creates a named map indexed by `source()`.
PolicyService::PolicyManagers SortManagers(
    PolicyService::PolicyManagerVector managers_vector) {
  base::ranges::stable_sort(
      managers_vector, [](const scoped_refptr<PolicyManagerInterface>& lhs,
                          const scoped_refptr<PolicyManagerInterface>& rhs) {
        return lhs->HasActiveDevicePolicies() &&
               !rhs->HasActiveDevicePolicies();
      });

  PolicyService::PolicyManagerNameMap managers_map;
  base::ranges::for_each(
      managers_vector,
      [&managers_map](const scoped_refptr<PolicyManagerInterface>& manager) {
        managers_map[manager->source()] = manager;
      });

  return {managers_vector, managers_map};
}

PolicyService::PolicyManagerVector CreatePolicyManagerVector(
    bool should_take_policy_critical_section,
    scoped_refptr<ExternalConstants> external_constants,
    scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
  PolicyService::PolicyManagerVector managers;
  if (external_constants) {
    managers.push_back(base::MakeRefCounted<PolicyManager>(
        external_constants->GroupPolicies()));
  }

#if BUILDFLAG(IS_WIN)
  managers.push_back(base::MakeRefCounted<GroupPolicyManager>(
      should_take_policy_critical_section));
#endif

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

PolicyService::PolicyManagers::PolicyManagers(
    PolicyManagerVector manager_vector,
    PolicyManagerNameMap manager_name_map)
    : vector(manager_vector), name_map(manager_name_map) {}
PolicyService::PolicyManagers::~PolicyManagers() = default;

PolicyService::PolicyService(PolicyManagerVector managers)
    : policy_managers_(SortManagers(std::move(managers))) {}

// The policy managers are initialized without taking the Group Policy critical
// section here, by passing `false` for `should_take_policy_critical_section`,
// to avoid blocking the main sequence. Later in `FetchPoliciesDone`, the
// policies are reloaded with the critical section lock.
PolicyService::PolicyService(
    scoped_refptr<ExternalConstants> external_constants)
    : policy_managers_(SortManagers(CreatePolicyManagerVector(
          /*should_take_policy_critical_section*/ false,
          external_constants,
          CreateDMPolicyManager()))),
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
    scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](scoped_refptr<ExternalConstants> external_constants,
             scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
            return CreatePolicyManagerVector(
                /*should_take_policy_critical_section*/ true,
                external_constants, dm_policy_manager);
          },
          external_constants_,
          dm_policy_manager ? dm_policy_manager
          : policy_managers_.name_map.count(kSourceDMPolicyManager)
              ? policy_managers_.name_map[kSourceDMPolicyManager]
              : nullptr),
      base::BindOnce(
          [](scoped_refptr<PolicyService> self,
             base::OnceCallback<void(int)> callback, int result,
             PolicyService::PolicyManagerVector managers) {
            self->policy_managers_ = SortManagers(std::move(managers));
            std::move(callback).Run(result);
          },
          base::WrapRefCounted(this), std::move(callback), result));
}

std::string PolicyService::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Returns the non-empty source combination of all active policy providers,
  // separated by ';'. For example: "group_policy;device_management".
  std::vector<std::string> sources;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_.vector) {
    if (policy_manager->HasActiveDevicePolicies() &&
        !policy_manager->source().empty()) {
      sources.push_back(policy_manager->source());
    }
  }
  return base::JoinString(sources, ";");
}

PolicyStatus<base::TimeDelta> PolicyService::GetLastCheckPeriod() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetLastCheckPeriod));
}

PolicyStatus<UpdatesSuppressedTimes> PolicyService::GetUpdatesSuppressedTimes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetUpdatesSuppressedTimes));
}

PolicyStatus<std::string> PolicyService::GetDownloadPreferenceGroupPolicy()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(base::BindRepeating(
      &PolicyManagerInterface::GetDownloadPreferenceGroupPolicy));
}

PolicyStatus<int> PolicyService::GetPackageCacheSizeLimitMBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(base::BindRepeating(
      &PolicyManagerInterface::GetPackageCacheSizeLimitMBytes));
}

PolicyStatus<int> PolicyService::GetPackageCacheExpirationTimeDays() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(base::BindRepeating(
      &PolicyManagerInterface::GetPackageCacheExpirationTimeDays));
}

PolicyStatus<int> PolicyService::GetPolicyForAppInstalls(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetEffectivePolicyForAppInstalls),
      app_id);
}

PolicyStatus<int> PolicyService::GetPolicyForAppUpdates(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::GetEffectivePolicyForAppUpdates),
      app_id);
}

PolicyStatus<std::string> PolicyService::GetTargetChannel(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetTargetChannel), app_id);
}

PolicyStatus<std::string> PolicyService::GetTargetVersionPrefix(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetTargetVersionPrefix),
      app_id);
}

PolicyStatus<bool> PolicyService::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      base::BindRepeating(
          &PolicyManagerInterface::IsRollbackToTargetVersionAllowed),
      app_id);
}

PolicyStatus<std::string> PolicyService::GetProxyMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyMode));
}

PolicyStatus<std::string> PolicyService::GetProxyPacUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyPacUrl));
}

PolicyStatus<std::string> PolicyService::GetProxyServer() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetProxyServer));
}

PolicyStatus<std::vector<std::string>> PolicyService::GetForceInstallApps()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetForceInstallApps));
}

PolicyStatus<int> PolicyService::DeprecatedGetLastCheckPeriodMinutes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      base::BindRepeating(&PolicyManagerInterface::GetLastCheckPeriod)
          .Then(base::BindRepeating([](absl::optional<base::TimeDelta> period) {
            return period ? absl::optional<int>(period->InMinutes())
                          : absl::nullopt;
          })));
}

template <typename T>
PolicyStatus<T> PolicyService::QueryPolicy(
    const base::RepeatingCallback<absl::optional<T>(
        const PolicyManagerInterface*)>& policy_query_callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::optional<T> query_result;
  PolicyStatus<T> status;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_.vector) {
    query_result = policy_query_callback.Run(policy_manager.get());
    if (!query_result)
      continue;
    status.AddPolicyIfNeeded(policy_manager->HasActiveDevicePolicies(),
                             policy_manager->source(), query_result.value());
  }

  return status;
}

template <typename T>
PolicyStatus<T> PolicyService::QueryAppPolicy(
    const base::RepeatingCallback<
        absl::optional<T>(const PolicyManagerInterface*, const std::string&)>&
        policy_query_callback,
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::optional<T> query_result;
  PolicyStatus<T> status;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_.vector) {
    query_result = policy_query_callback.Run(policy_manager.get(), app_id);
    if (!query_result)
      continue;
    status.AddPolicyIfNeeded(policy_manager->HasActiveDevicePolicies(),
                             policy_manager->source(), query_result.value());
  }

  return status;
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
  PolicyStatus<std::string> proxy_mode = policy_service->GetProxyMode();
  if (!proxy_mode || proxy_mode.policy().compare(kProxyModeSystem) == 0) {
    return absl::nullopt;
  }
  VLOG(2) << "Using policy proxy " << proxy_mode.policy();

  PolicyServiceProxyConfiguration policy_service_proxy_configuration;

  bool is_policy_config_valid = true;
  if (proxy_mode.policy().compare(kProxyModeFixedServers) == 0) {
    PolicyStatus<std::string> proxy_url = policy_service->GetProxyServer();
    if (!proxy_url) {
      VLOG(1) << "Fixed server mode proxy has no URL specified.";
      is_policy_config_valid = false;
    } else {
      policy_service_proxy_configuration.proxy_url = proxy_url.policy();
    }
  } else if (proxy_mode.policy().compare(kProxyModePacScript) == 0) {
    PolicyStatus<std::string> proxy_pac_url;
    if (!proxy_pac_url) {
      VLOG(1) << "PAC proxy policy has no PAC URL specified.";
      is_policy_config_valid = false;
    } else {
      policy_service_proxy_configuration.proxy_pac_url = proxy_pac_url.policy();
    }
  } else if (proxy_mode.policy().compare(kProxyModeAutoDetect)) {
    policy_service_proxy_configuration.proxy_auto_detect = true;
  }

  if (!is_policy_config_valid) {
    VLOG(1) << "Configuration set by policy was invalid.";
    return absl::nullopt;
  }

  return policy_service_proxy_configuration;
}

}  // namespace updater
