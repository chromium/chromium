// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/service.h"

#include <concepts>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/app/app_utils.h"
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
#include "components/crash/core/common/crash_key.h"

namespace updater {
namespace {

// Sorts the managed policy managers ahead of the non-managed ones in the
// vector, and creates a named map indexed by `source()`.
PolicyService::PolicyManagers SortManagers(
    std::vector<scoped_refptr<PolicyManagerInterface>> managers) {
  base::ranges::stable_sort(
      managers, [](const scoped_refptr<PolicyManagerInterface>& lhs,
                   const scoped_refptr<PolicyManagerInterface>& rhs) {
        return lhs->HasActiveDevicePolicies() &&
               !rhs->HasActiveDevicePolicies();
      });

  base::flat_map<std::string, scoped_refptr<PolicyManagerInterface>>
      manager_names;
  base::ranges::for_each(
      managers,
      [&manager_names](const scoped_refptr<PolicyManagerInterface>& manager) {
        manager_names[manager->source()] = manager;
      });

  return {managers, manager_names};
}

#if BUILDFLAG(IS_WIN)
bool CloudPolicyOverridesPlatformPolicy(
    std::vector<scoped_refptr<PolicyManagerInterface>> providers) {
  auto it = base::ranges::find_if(
      providers, [](scoped_refptr<PolicyManagerInterface> p) {
        return p && p->CloudPolicyOverridesPlatformPolicy();
      });

  return it == providers.end() ? false
                               : *(*it)->CloudPolicyOverridesPlatformPolicy();
}
#endif

}  // namespace

std::vector<scoped_refptr<PolicyManagerInterface>> CreateManagers(
    bool should_take_policy_critical_section,
    scoped_refptr<ExternalConstants> external_constants,
    scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
  // The order of the policy managers:
  //   1) External constants policy manager (if present).
  //   2) Group policy manager (Windows only). **
  //   3) DM policy manager (if present). **
  //   4) Managed preferences policy manager(macOS only).
  //   5) The default value policy manager.
  // ** If `CloudPolicyOverridesPlatformPolicy`, then the DM policy manager
  //    has a higher priority than the group policy manger.
  std::vector<scoped_refptr<PolicyManagerInterface>> managers;
  if (dm_policy_manager) {
    managers.push_back(dm_policy_manager);
  }
  scoped_refptr<PolicyManagerInterface> external_constants_policy_manager =
      external_constants ? base::MakeRefCounted<PolicyManager>(
                               external_constants->GroupPolicies())
                         : nullptr;
#if BUILDFLAG(IS_WIN)
  auto group_policy_manager = base::MakeRefCounted<GroupPolicyManager>(
      should_take_policy_critical_section,
      external_constants->IsMachineManaged());
  if (CloudPolicyOverridesPlatformPolicy({dm_policy_manager,
                                          group_policy_manager,
                                          external_constants_policy_manager})) {
    VLOG(1) << __func__ << ": CloudPolicyOverridesPlatformPolicy=1";
    managers.push_back(std::move(group_policy_manager));
  } else {
    managers.insert(managers.begin(), std::move(group_policy_manager));
  }
#endif
  if (external_constants_policy_manager) {
    managers.insert(managers.begin(), external_constants_policy_manager);
  }
#if BUILDFLAG(IS_MAC)
  managers.push_back(CreateManagedPreferencePolicyManager(
      external_constants->IsMachineManaged()));
#endif
  managers.push_back(GetDefaultValuesPolicyManager());
  return managers;
}

PolicyService::PolicyManagers::PolicyManagers(
    std::vector<scoped_refptr<PolicyManagerInterface>> managers,
    base::flat_map<std::string, scoped_refptr<PolicyManagerInterface>>
        manager_names)
    : managers(std::move(managers)), manager_names(std::move(manager_names)) {}
PolicyService::PolicyManagers::~PolicyManagers() = default;

PolicyService::PolicyService(
    std::vector<scoped_refptr<PolicyManagerInterface>> managers,
    bool usage_stats_enabled)
    : policy_managers_(SortManagers(std::move(managers))),
      usage_stats_enabled_(usage_stats_enabled) {}

// The policy managers are initialized without taking the Group Policy critical
// section here, by passing `false` for `should_take_policy_critical_section`,
// to avoid blocking the main sequence. Later in `FetchPoliciesDone`, the
// policies are reloaded with the critical section lock.
PolicyService::PolicyService(
    scoped_refptr<ExternalConstants> external_constants,
    bool usage_stats_enabled)
    : policy_managers_(SortManagers(CreateManagers(
          /*should_take_policy_critical_section=*/false,
          external_constants,
          CreateDMPolicyManager(external_constants->IsMachineManaged())))),
      external_constants_(external_constants),
      usage_stats_enabled_(usage_stats_enabled) {
  VLOG(1) << "Current effective policies:" << std::endl
          << GetAllPoliciesAsString();
}

PolicyService::~PolicyService() = default;

void PolicyService::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce([] {
        scoped_refptr<device_management_storage::DMStorage> dm_storage =
            device_management_storage::GetDefaultDMStorage();
        return dm_storage && (dm_storage->IsValidDMToken() ||
                              (!dm_storage->GetEnrollmentToken().empty() &&
                               !dm_storage->IsDeviceDeregistered()));
      }),
      base::BindOnce(&PolicyService::DoFetchPolicies,
                     base::WrapRefCounted(this), std::move(callback)));
}

void PolicyService::DoFetchPolicies(base::OnceCallback<void(int)> callback,
                                    bool is_cbcm_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static crash_reporter::CrashKeyString<6> crash_key_cbcm("cbcm");
  crash_key_cbcm.Set(is_cbcm_managed ? "true" : "false");
  if (!is_cbcm_managed) {
    VLOG(2) << "Device is not CBCM managed, skipped policy fetch.";
    std::move(callback).Run(0);
    return;
  }

  if (fetch_policies_callback_) {
    // Combine with existing call.
    fetch_policies_callback_ = base::BindOnce(
        [](base::OnceCallback<void(int)> a, base::OnceCallback<void(int)> b,
           int v) {
          std::move(a).Run(v);
          std::move(b).Run(v);
        },
        std::move(fetch_policies_callback_), std::move(callback));
    return;
  }

  fetch_policies_callback_ = std::move(callback);
  auto fetcher = base::MakeRefCounted<FallbackPolicyFetcher>(
      CreateOutOfProcessPolicyFetcher(
          usage_stats_enabled_, external_constants_->IsMachineManaged(),
          external_constants_->CecaConnectionTimeout()),
      CreateInProcessPolicyFetcher(external_constants_->DeviceManagementURL(),
                                   PolicyServiceProxyConfiguration::Get(this),
                                   external_constants_->IsMachineManaged()));
  fetcher->FetchPolicies(
      base::BindOnce(&PolicyService::FetchPoliciesDone, this, fetcher));
}

void PolicyService::FetchPoliciesDone(
    scoped_refptr<PolicyFetcher> fetcher,
    int result,
    scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](scoped_refptr<ExternalConstants> external_constants,
             scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
            return CreateManagers(
                /*should_take_policy_critical_section=*/true,
                external_constants, dm_policy_manager);
          },
          external_constants_,
          dm_policy_manager ? dm_policy_manager
          : policy_managers_.manager_names.contains(kSourceDMPolicyManager)
              ? policy_managers_.manager_names[kSourceDMPolicyManager]
              : nullptr),
      base::BindOnce(&PolicyService::PolicyManagerLoaded,
                     base::WrapRefCounted(this), result));
}

void PolicyService::PolicyManagerLoaded(
    int result,
    std::vector<scoped_refptr<PolicyManagerInterface>> managers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy_managers_ = SortManagers(std::move(managers));
  VLOG(1) << "Policies after refresh:" << std::endl << GetAllPoliciesAsString();
  std::move(fetch_policies_callback_).Run(result);
}

std::string PolicyService::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Returns the non-empty source combination of all active policy providers,
  // separated by ';'. For example: "group_policy;device_management".
  std::vector<std::string> sources;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_.managers) {
    if (policy_manager->HasActiveDevicePolicies() &&
        !policy_manager->source().empty()) {
      sources.push_back(policy_manager->source());
    }
  }
  return base::JoinString(sources, ";");
}

PolicyStatus<bool> PolicyService::CloudPolicyOverridesPlatformPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      &PolicyManagerInterface::CloudPolicyOverridesPlatformPolicy);
}

PolicyStatus<base::TimeDelta> PolicyService::GetLastCheckPeriod() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(&PolicyManagerInterface::GetLastCheckPeriod);
}

PolicyStatus<UpdatesSuppressedTimes> PolicyService::GetUpdatesSuppressedTimes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(&PolicyManagerInterface::GetUpdatesSuppressedTimes);
}

PolicyStatus<std::string> PolicyService::GetDownloadPreference() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      &PolicyManagerInterface::GetDownloadPreference,
      base::BindRepeating([](std::optional<std::string> download_preference) {
        return (download_preference.has_value() &&
                base::EqualsCaseInsensitiveASCII(download_preference.value(),
                                                 kDownloadPreferenceCacheable))
                   ? download_preference
                   : std::nullopt;
      }));
}

PolicyStatus<int> PolicyService::GetPackageCacheSizeLimitMBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(&PolicyManagerInterface::GetPackageCacheSizeLimitMBytes);
}

PolicyStatus<int> PolicyService::GetPackageCacheExpirationTimeDays() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      &PolicyManagerInterface::GetPackageCacheExpirationTimeDays);
}

PolicyStatus<int> PolicyService::GetPolicyForAppInstalls(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      &PolicyManagerInterface::GetEffectivePolicyForAppInstalls, app_id);
}

PolicyStatus<int> PolicyService::GetPolicyForAppUpdates(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      &PolicyManagerInterface::GetEffectivePolicyForAppUpdates, app_id);
}

PolicyStatus<std::string> PolicyService::GetTargetChannel(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(&PolicyManagerInterface::GetTargetChannel, app_id);
}

PolicyStatus<std::string> PolicyService::GetTargetVersionPrefix(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(&PolicyManagerInterface::GetTargetVersionPrefix,
                        app_id);
}

PolicyStatus<bool> PolicyService::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(
      &PolicyManagerInterface::IsRollbackToTargetVersionAllowed, app_id);
}

PolicyStatus<std::string> PolicyService::GetProxyMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      &PolicyManagerInterface::GetProxyMode,
      base::BindRepeating([](std::optional<std::string> proxy_mode) {
        return (proxy_mode.has_value() &&
                base::Contains(std::vector<std::string>(
                                   {kProxyModeDirect, kProxyModeSystem,
                                    kProxyModeFixedServers, kProxyModePacScript,
                                    kProxyModeAutoDetect}),
                               base::ToLowerASCII(proxy_mode.value())))
                   ? proxy_mode
                   : std::nullopt;
      }));
}

PolicyStatus<std::string> PolicyService::GetProxyPacUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(&PolicyManagerInterface::GetProxyPacUrl);
}

PolicyStatus<std::string> PolicyService::GetProxyServer() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(&PolicyManagerInterface::GetProxyServer);
}

PolicyStatus<std::vector<std::string>> PolicyService::GetForceInstallApps()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(&PolicyManagerInterface::GetForceInstallApps);
}

PolicyStatus<int> PolicyService::DeprecatedGetLastCheckPeriodMinutes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryPolicy(
      &PolicyManagerInterface::GetLastCheckPeriod,
      base::BindRepeating([](std::optional<base::TimeDelta> period) {
        return period ? std::make_optional(period->InMinutes()) : std::nullopt;
      }));
}

std::set<std::string> PolicyService::GetAppsWithPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<std::string> apps_with_policy;

  base::ranges::for_each(
      policy_managers_.managers,
      [&apps_with_policy](
          const scoped_refptr<PolicyManagerInterface>& manager) {
        auto apps = manager->GetAppsWithPolicy();
        if (apps) {
          apps_with_policy.insert(apps->begin(), apps->end());
        }
      });

  return apps_with_policy;
}

base::Value PolicyService::GetAllPolicies() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict policies;

  const PolicyStatus<bool> cloud_policy_override_platform_policy =
      CloudPolicyOverridesPlatformPolicy();
  if (cloud_policy_override_platform_policy) {
    policies.Set(
        "CloudPolicyOverridesPlatformPolicy",
        base::Value::Dict()
            .Set("value", cloud_policy_override_platform_policy.policy())
            .Set("source",
                 cloud_policy_override_platform_policy.effective_policy()
                     ->source));
  }

  const PolicyStatus<base::TimeDelta> last_check_period = GetLastCheckPeriod();
  if (last_check_period) {
    policies.Set(
        "LastCheckPeriod",
        base::Value::Dict()
            .Set("value", last_check_period.policy().InMinutes())
            .Set("source", last_check_period.effective_policy()->source));
  }

  const PolicyStatus<UpdatesSuppressedTimes> update_supressed_times =
      GetUpdatesSuppressedTimes();
  if (update_supressed_times) {
    policies.Set(
        "UpdatesSuppressed",
        base::Value::Dict()
            .Set("StartHour", update_supressed_times.policy().start_hour_)
            .Set("StartMinute", update_supressed_times.policy().start_minute_)
            .Set("Duration", update_supressed_times.policy().duration_minute_)
            .Set("source", update_supressed_times.effective_policy()->source));
  }

  const PolicyStatus<std::string> download_preference = GetDownloadPreference();
  if (download_preference) {
    policies.Set(
        "DownloadPreference",
        base::Value::Dict()
            .Set("value", download_preference.policy())
            .Set("source", download_preference.effective_policy()->source));
  }

  const PolicyStatus<int> cache_size_limit = GetPackageCacheSizeLimitMBytes();
  if (cache_size_limit) {
    policies.Set(
        "PackageCacheSizeLimit",
        base::Value::Dict()
            .Set("value", cache_size_limit.policy())
            .Set("source", cache_size_limit.effective_policy()->source));
  }

  const PolicyStatus<int> cache_expiration_time =
      GetPackageCacheExpirationTimeDays();
  if (cache_expiration_time) {
    policies.Set(
        "PackageCacheExpires",
        base::Value::Dict()
            .Set("value", cache_expiration_time.policy())
            .Set("source", cache_expiration_time.effective_policy()->source));
  }

  const PolicyStatus<std::string> proxy_mode = GetProxyMode();
  if (proxy_mode) {
    policies.Set("ProxyMode",
                 base::Value::Dict()
                     .Set("value", proxy_mode.policy())
                     .Set("source", proxy_mode.effective_policy()->source));
  }
  const PolicyStatus<std::string> proxy_pac_url = GetProxyPacUrl();
  if (proxy_pac_url) {
    policies.Set("ProxyPacURL",
                 base::Value::Dict()
                     .Set("value", proxy_pac_url.policy())
                     .Set("source", proxy_pac_url.effective_policy()->source));
  }
  const PolicyStatus<std::string> proxy_server = GetProxyServer();
  if (proxy_server) {
    policies.Set("ProxyServer",
                 base::Value::Dict()
                     .Set("value", proxy_server.policy())
                     .Set("source", proxy_server.effective_policy()->source));
  }

  for (const std::string& app_id : GetAppsWithPolicy()) {
    base::Value::Dict app_policies;
    const PolicyStatus<int> app_install = GetPolicyForAppInstalls(app_id);
    if (app_install) {
      app_policies.Set(
          "Install",
          base::Value::Dict()
              .Set("value", app_install.policy())
              .Set("source", app_install.effective_policy()->source));
    }
    const PolicyStatus<int> app_update = GetPolicyForAppUpdates(app_id);
    if (app_update) {
      app_policies.Set(
          "Update", base::Value::Dict()
                        .Set("value", app_update.policy())
                        .Set("source", app_update.effective_policy()->source));
    }
    const PolicyStatus<std::string> target_channel = GetTargetChannel(app_id);
    if (target_channel) {
      app_policies.Set(
          "TargetChannel",
          base::Value::Dict()
              .Set("value", target_channel.policy())
              .Set("source", target_channel.effective_policy()->source));
    }
    const PolicyStatus<std::string> target_version_prefix =
        GetTargetVersionPrefix(app_id);
    if (target_version_prefix) {
      app_policies.Set(
          "TargetVersionPrefix",
          base::Value::Dict()
              .Set("value", target_version_prefix.policy())
              .Set("source", target_version_prefix.effective_policy()->source));
    }
    const PolicyStatus<bool> rollback_allowed =
        IsRollbackToTargetVersionAllowed(app_id);
    if (rollback_allowed) {
      app_policies.Set(
          "RollbackToTargetVersionAllowed",
          base::Value::Dict()
              .Set("value", rollback_allowed.policy())
              .Set("source", rollback_allowed.effective_policy()->source));
    }

    policies.Set(app_id, std::move(app_policies));
  }
  return base::Value(std::move(policies));
}

std::string PolicyService::GetAllPoliciesAsString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> policies;

  const PolicyStatus<bool> cloud_policy_override_platform_policy =
      CloudPolicyOverridesPlatformPolicy();
  if (cloud_policy_override_platform_policy) {
    policies.push_back(base::StringPrintf(
        "CloudPolicyOverridesPlatformPolicy = %d (%s)",
        cloud_policy_override_platform_policy.policy(),
        cloud_policy_override_platform_policy.effective_policy()
            ->source.c_str()));
  }

  const PolicyStatus<base::TimeDelta> last_check_period = GetLastCheckPeriod();
  if (last_check_period) {
    policies.push_back(base::StringPrintf(
        "LastCheckPeriod = %d (%s)", last_check_period.policy().InMinutes(),
        last_check_period.effective_policy()->source.c_str()));
  }

  const PolicyStatus<UpdatesSuppressedTimes> update_supressed_times =
      GetUpdatesSuppressedTimes();
  if (update_supressed_times) {
    policies.push_back(base::StringPrintf(
        "UpdatesSuppressed = {StartHour: %d, StartMinute: "
        "%d, Duration: %d} (%s)",
        update_supressed_times.policy().start_hour_,
        update_supressed_times.policy().start_minute_,
        update_supressed_times.policy().duration_minute_,
        update_supressed_times.effective_policy()->source.c_str()));
  }

  const PolicyStatus<std::string> download_preference = GetDownloadPreference();
  if (download_preference) {
    policies.push_back(base::StringPrintf(
        "DownloadPreference = %s (%s)", download_preference.policy().c_str(),
        download_preference.effective_policy()->source.c_str()));
  }

  const PolicyStatus<int> cache_size_limit = GetPackageCacheSizeLimitMBytes();
  if (cache_size_limit) {
    policies.push_back(base::StringPrintf(
        "PackageCacheSizeLimit = %d MB (%s)", cache_size_limit.policy(),
        cache_size_limit.effective_policy()->source.c_str()));
  }

  const PolicyStatus<int> cache_expiration_time =
      GetPackageCacheExpirationTimeDays();
  if (cache_expiration_time) {
    policies.push_back(base::StringPrintf(
        "PackageCacheExpires = %d days (%s)", cache_expiration_time.policy(),
        cache_expiration_time.effective_policy()->source.c_str()));
  }

  const PolicyStatus<std::string> proxy_mode = GetProxyMode();
  if (proxy_mode) {
    policies.push_back(
        base::StringPrintf("ProxyMode = %s (%s)", proxy_mode.policy().c_str(),
                           proxy_mode.effective_policy()->source.c_str()));
  }

  const PolicyStatus<std::string> proxy_pac_url = GetProxyPacUrl();
  if (proxy_pac_url) {
    policies.push_back(base::StringPrintf(
        "ProxyPacURL = %s (%s)", proxy_pac_url.policy().c_str(),
        proxy_pac_url.effective_policy()->source.c_str()));
  }
  const PolicyStatus<std::string> proxy_server = GetProxyServer();
  if (proxy_server) {
    policies.push_back(base::StringPrintf(
        "ProxyServer = %s (%s)", proxy_server.policy().c_str(),
        proxy_server.effective_policy()->source.c_str()));
  }

  for (const std::string& app_id : GetAppsWithPolicy()) {
    std::vector<std::string> app_policies;
    const PolicyStatus<int> app_install = GetPolicyForAppInstalls(app_id);
    if (app_install) {
      app_policies.push_back(
          base::StringPrintf("Install = %d (%s)", app_install.policy(),
                             app_install.effective_policy()->source.c_str()));
    }

    const PolicyStatus<int> app_update = GetPolicyForAppUpdates(app_id);
    if (app_update) {
      app_policies.push_back(
          base::StringPrintf("Update = %d (%s)", app_update.policy(),
                             app_update.effective_policy()->source.c_str()));
    }
    const PolicyStatus<std::string> target_channel = GetTargetChannel(app_id);
    if (target_channel) {
      app_policies.push_back(base::StringPrintf(
          "TargetChannel = %s (%s)", target_channel.policy().c_str(),
          target_channel.effective_policy()->source.c_str()));
    }
    const PolicyStatus<std::string> target_version_prefix =
        GetTargetVersionPrefix(app_id);
    if (target_version_prefix) {
      app_policies.push_back(base::StringPrintf(
          "TargetVersionPrefix = %s (%s)",
          target_version_prefix.policy().c_str(),
          target_version_prefix.effective_policy()->source.c_str()));
    }
    const PolicyStatus<bool> rollback_allowed =
        IsRollbackToTargetVersionAllowed(app_id);
    if (rollback_allowed) {
      app_policies.push_back(base::StringPrintf(
          "RollbackToTargetVersionAllowed = %d (%s)", rollback_allowed.policy(),
          rollback_allowed.effective_policy()->source.c_str()));
    }

    policies.push_back(
        base::StringPrintf("\"%s\": {\n    %s\n  }", app_id.c_str(),
                           base::JoinString(app_policies, "\n    ").c_str()));
  }
  return base::StringPrintf("{\n  %s\n}\n",
                            base::JoinString(policies, "\n  ").c_str());
}

bool PolicyService::AreUpdatesSuppressedNow(const base::Time& now) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const PolicyStatus<UpdatesSuppressedTimes> suppression =
      GetUpdatesSuppressedTimes();
  if (!suppression || !suppression.policy().valid()) {
    return false;
  }
  base::Time::Exploded now_local;
  now.LocalExplode(&now_local);
  const bool are_updates_suppressed =
      suppression.policy().contains(now_local.hour, now_local.minute);
  VLOG(0) << __func__ << ": Updates are "
          << (are_updates_suppressed ? "" : "not ") << "suppressed: now=" << now
          << ": UpdatesSuppressedTimes: start_hour_:"
          << suppression.policy().start_hour_
          << ": start_minute_:" << suppression.policy().start_minute_
          << ": duration_minute_:" << suppression.policy().duration_minute_;
  return are_updates_suppressed;
}

template <typename T, typename U>
PolicyStatus<U> PolicyService::QueryPolicy(
    PolicyQueryFunction<T> policy_query_function,
    const base::RepeatingCallback<std::optional<U>(std::optional<T>)>&
        transform) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PolicyStatus<U> status;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_.managers) {
    const std::optional<U> transformed_result =
        [&transform](std::optional<T> query_result) {
          if constexpr (std::same_as<T, U>) {
            return transform.is_null() ? std::move(query_result)
                                       : transform.Run(std::move(query_result));
          } else {
            CHECK(!transform.is_null());
            return transform.Run(std::move(query_result));
          }
        }(std::invoke(policy_query_function, policy_manager));
    if (transformed_result.has_value()) {
      status.AddPolicyIfNeeded(policy_manager->HasActiveDevicePolicies(),
                               policy_manager->source(),
                               transformed_result.value());
    }
  }
  return status;
}

template <typename T>
PolicyStatus<T> PolicyService::QueryAppPolicy(
    AppPolicyQueryFunction<T> policy_query_function,
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsUpdaterOrCompanionApp(app_id) || app_id == kQualificationAppId) {
    // Updater itself, qualification and the companion app are excluded from
    // policy settings.
    return {};
  }
  PolicyStatus<T> status;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_.managers) {
    std::optional<T> query_result =
        std::invoke(policy_query_function, policy_manager, app_id);
    if (query_result.has_value()) {
      status.AddPolicyIfNeeded(policy_manager->HasActiveDevicePolicies(),
                               policy_manager->source(), query_result.value());
    }
  }

  return status;
}

PolicyServiceProxyConfiguration::PolicyServiceProxyConfiguration() = default;
PolicyServiceProxyConfiguration::~PolicyServiceProxyConfiguration() = default;
PolicyServiceProxyConfiguration::PolicyServiceProxyConfiguration(
    const PolicyServiceProxyConfiguration&) = default;
PolicyServiceProxyConfiguration::PolicyServiceProxyConfiguration(
    PolicyServiceProxyConfiguration&&) = default;
PolicyServiceProxyConfiguration& PolicyServiceProxyConfiguration::operator=(
    const PolicyServiceProxyConfiguration&) = default;
PolicyServiceProxyConfiguration& PolicyServiceProxyConfiguration::operator=(
    PolicyServiceProxyConfiguration&&) = default;

std::optional<PolicyServiceProxyConfiguration>
PolicyServiceProxyConfiguration::Get(
    scoped_refptr<PolicyService> policy_service) {
  const PolicyStatus<std::string> proxy_mode = policy_service->GetProxyMode();
  if (!proxy_mode || proxy_mode.policy().compare(kProxyModeSystem) == 0) {
    return std::nullopt;
  }
  VLOG(2) << "Using policy proxy " << proxy_mode.policy();

  PolicyServiceProxyConfiguration policy_service_proxy_configuration;

  bool is_policy_config_valid = true;
  if (proxy_mode.policy().compare(kProxyModeFixedServers) == 0) {
    const PolicyStatus<std::string> proxy_url =
        policy_service->GetProxyServer();
    if (!proxy_url) {
      VLOG(1) << "Fixed server mode proxy has no URL specified.";
      is_policy_config_valid = false;
    } else {
      policy_service_proxy_configuration.proxy_url = proxy_url.policy();
    }
  } else if (proxy_mode.policy().compare(kProxyModePacScript) == 0) {
    const PolicyStatus<std::string> proxy_pac_url =
        policy_service->GetProxyPacUrl();
    if (!proxy_pac_url) {
      VLOG(1) << "PAC proxy policy has no PAC URL specified.";
      is_policy_config_valid = false;
    } else {
      policy_service_proxy_configuration.proxy_pac_url = proxy_pac_url.policy();
    }
  } else if (proxy_mode.policy().compare(kProxyModeAutoDetect) == 0) {
    policy_service_proxy_configuration.proxy_auto_detect = true;
  } else {
    is_policy_config_valid = false;
  }

  if (!is_policy_config_valid) {
    VLOG(1) << "Configuration set by policy was invalid.";
    return std::nullopt;
  }

  return policy_service_proxy_configuration;
}

}  // namespace updater
