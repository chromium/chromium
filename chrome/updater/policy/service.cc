// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/service.h"

#include <algorithm>
#include <concepts>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/event_history.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/platform_policy_manager.h"
#include "chrome/updater/policy/policy_fetcher.h"
#include "chrome/updater/policy/policy_manager.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/common/policy_types.h"
#include "components/update_client/update_client_errors.h"

namespace updater {
namespace {

// Creates a base::Value::Dict representation of an individual policy adhering
// to the format defined by //docs/updater/history_log.md.
template <typename T>
base::Value::Dict PolicyStatusToDict(const PolicyStatus<T>& policy_status) {
  base::Value::Dict values_by_source;
  for (const typename PolicyStatus<T>::Entry entry :
       policy_status.all_policies()) {
    if constexpr (std::is_same_v<T, base::TimeDelta>) {
      values_by_source.Set(entry.source, base::TimeDeltaToValue(entry.policy));
    } else if constexpr (std::is_same_v<T, UpdatesSuppressedTimes>) {
      values_by_source.Set(entry.source,
                           base::Value::Dict()
                               .Set("StartHour", entry.policy.start_hour_)
                               .Set("StartMinute", entry.policy.start_minute_)
                               .Set("Duration", entry.policy.duration_minute_));
    } else {
      values_by_source.Set(entry.source, entry.policy);
    }
  }
  return base::Value::Dict()
      .Set("valuesBySource", std::move(values_by_source))
      .Set("prevailingSource", policy_status.effective_policy()->source);
}

}  // namespace

PolicyService::PolicyManagers::PolicyManagers(
    scoped_refptr<ExternalConstants> external_constants) {
  CreateManagers(external_constants);
  InitializeManagersVector();
}

PolicyService::PolicyManagers::~PolicyManagers() = default;

void PolicyService::PolicyManagers::CreateManagers(
    scoped_refptr<ExternalConstants> external_constants) {
  default_policy_manager_ = GetDefaultValuesPolicyManager();
  if (!external_constants) {
    return;
  }
  dm_policy_manager_ =
      CreateDMPolicyManager(external_constants->IsMachineManaged());
  external_constants_policy_manager_ =
      CreateDictPolicyManager(external_constants->DictPolicies());
  platform_policy_manager_ =
      CreatePlatformPolicyManager(external_constants->IsMachineManaged());
}

// The order of the policy managers:
//   1) External constants policy manager (if present).
//   2) Platform policy manager (Group policy on Windows, and Managed
//      Preferences on macOS). See NOTE below.
//   3) DM policy manager (if present). See NOTE below.
//   4) The default value policy manager.
// NOTE: If `CloudPolicyOverridesPlatformPolicy`, then the DM policy manager
//    has a higher priority than the platform policy manger.
void PolicyService::PolicyManagers::InitializeManagersVector() {
  managers_.clear();
  if (dm_policy_manager_) {
    managers_.push_back(dm_policy_manager_);
  }

  if (platform_policy_manager_) {
    if (CloudPolicyOverridesPlatformPolicy(
            {dm_policy_manager_, platform_policy_manager_,
             external_constants_policy_manager_})) {
      VLOG(1) << __func__ << ": CloudPolicyOverridesPlatformPolicy=1";
      managers_.push_back(platform_policy_manager_);
    } else {
      managers_.insert(managers_.begin(), platform_policy_manager_);
    }
  }

  if (external_constants_policy_manager_) {
    managers_.insert(managers_.begin(), external_constants_policy_manager_);
  }

  managers_.push_back(default_policy_manager_);

  SortManagersVector();
}

void PolicyService::PolicyManagers::SortManagersVector() {
  std::ranges::stable_sort(
      managers_, [](const scoped_refptr<PolicyManagerInterface>& lhs,
                    const scoped_refptr<PolicyManagerInterface>& rhs) {
        return lhs->HasActiveDevicePolicies() &&
               !rhs->HasActiveDevicePolicies();
      });
}

bool PolicyService::PolicyManagers::CloudPolicyOverridesPlatformPolicy(
    const std::vector<scoped_refptr<PolicyManagerInterface>>& providers) {
  auto it = std::ranges::find_if(
      providers, [](scoped_refptr<PolicyManagerInterface> p) {
        return p && (p->CloudPolicyOverridesPlatformPolicy()).has_value();
      });

  return it == providers.end()
             ? kCloudPolicyOverridesPlatformPolicyDefaultValue
             : ((*it)->CloudPolicyOverridesPlatformPolicy()).value();
}

void PolicyService::PolicyManagers::ResetDeviceManagementManager(
    scoped_refptr<PolicyManagerInterface> dm_manager) {
  dm_policy_manager_ = dm_manager;
  InitializeManagersVector();
}

void PolicyService::PolicyManagers::SetManagersForTesting(
    std::vector<scoped_refptr<PolicyManagerInterface>> managers) {
  managers_ = std::move(managers);
  SortManagersVector();
}

PolicyService::PolicyService(
    scoped_refptr<ExternalConstants> external_constants,
    scoped_refptr<PersistedData> persisted_data)
    : external_constants_(external_constants), persisted_data_(persisted_data) {
  LoadPolicyEndEvent event =
      LoadPolicyStartEvent().WriteAsyncAndReturnEndEvent();
  policy_managers_ = std::make_unique<PolicyManagers>(external_constants);
  event.SetPolicySet(GetAllPolicies()).WriteAsync();
  VLOG(1) << "Current effective policies:" << std::endl
          << GetAllPoliciesAsString();
}

PolicyService::~PolicyService() = default;

void PolicyService::FetchPolicies(policy::PolicyFetchReason reason,
                                  base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(&IsCloudManaged),
      base::BindOnce(&PolicyService::DoFetchPolicies,
                     base::WrapRefCounted(this), reason, std::move(callback)));
}

void PolicyService::DoFetchPolicies(policy::PolicyFetchReason reason,
                                    base::OnceCallback<void(int)> callback,
                                    bool is_cbcm_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static crash_reporter::CrashKeyString<6> crash_key_cbcm("cbcm");
  crash_key_cbcm.Set(base::ToString(is_cbcm_managed));

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

  if (!is_cbcm_managed) {
    VLOG(2) << "Device is not CBCM managed, skipped policy fetch.";
    std::move(fetch_policies_callback_).Run(kErrorOk);
    return;
  }

  scoped_refptr<PolicyFetcher> fetcher = CreateOutOfProcessPolicyFetcher(
      persisted_data_, external_constants_->IsMachineManaged(),
      external_constants_->CecaConnectionTimeout());
  fetcher->FetchPolicies(
      reason,
      base::BindOnce(&PolicyService::FetchPoliciesDone, this, fetcher,
                     LoadPolicyStartEvent().WriteAsyncAndReturnEndEvent()));
}

void PolicyService::FetchPoliciesDone(
    scoped_refptr<PolicyFetcher> fetcher,
    LoadPolicyEndEvent event,
    int result,
    scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << ": " << result;

  if (result == kErrorOk) {
    policy_managers_->ResetDeviceManagementManager(dm_policy_manager);
    VLOG(1) << "Policies after refresh:" << std::endl
            << GetAllPoliciesAsString();
  } else {
    event.AddError(
        {.category = static_cast<int>(update_client::ErrorCategory::kService),
         .code = result});
    VLOG(1) << "Failed to refresh policies: " << result;
  }

  event.SetPolicySet(GetAllPolicies()).WriteAsync();
  std::move(fetch_policies_callback_).Run(result);
}

std::string PolicyService::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Returns the non-empty source combination of all active policy providers,
  // separated by ';'. For example: "group_policy;device_management".
  std::vector<std::string> sources;
  for (const scoped_refptr<PolicyManagerInterface>& policy_manager :
       policy_managers_->managers()) {
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

PolicyStatus<int> PolicyService::GetMajorVersionRolloutPolicy(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(&PolicyManagerInterface::GetMajorVersionRolloutPolicy,
                        app_id);
}

PolicyStatus<int> PolicyService::GetMinorVersionRolloutPolicy(
    const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryAppPolicy(&PolicyManagerInterface::GetMajorVersionRolloutPolicy,
                        app_id);
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

  std::ranges::for_each(
      policy_managers_->managers(),
      [&apps_with_policy](
          const scoped_refptr<PolicyManagerInterface>& manager) {
        auto apps = manager->GetAppsWithPolicy();
        if (apps) {
          apps_with_policy.insert(apps->begin(), apps->end());
        }
      });

  return apps_with_policy;
}

base::Value::Dict PolicyService::GetAllPolicies() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict policies;

  const PolicyStatus<bool> cloud_policy_override_platform_policy =
      CloudPolicyOverridesPlatformPolicy();
  if (cloud_policy_override_platform_policy) {
    policies.Set("CloudPolicyOverridesPlatformPolicy",
                 PolicyStatusToDict(cloud_policy_override_platform_policy));
  }

  const PolicyStatus<base::TimeDelta> last_check_period = GetLastCheckPeriod();
  if (last_check_period) {
    policies.Set("LastCheckPeriod", PolicyStatusToDict(last_check_period));
  }

  const PolicyStatus<UpdatesSuppressedTimes> update_suppressed_times =
      GetUpdatesSuppressedTimes();
  if (update_suppressed_times) {
    policies.Set("UpdatesSuppressed",
                 PolicyStatusToDict(update_suppressed_times));
  }

  const PolicyStatus<std::string> download_preference = GetDownloadPreference();
  if (download_preference) {
    policies.Set("DownloadPreference", PolicyStatusToDict(download_preference));
  }

  const PolicyStatus<int> cache_size_limit = GetPackageCacheSizeLimitMBytes();
  if (cache_size_limit) {
    policies.Set("PackageCacheSizeLimit", PolicyStatusToDict(cache_size_limit));
  }

  const PolicyStatus<int> cache_expiration_time =
      GetPackageCacheExpirationTimeDays();
  if (cache_expiration_time) {
    policies.Set("PackageCacheExpires",
                 PolicyStatusToDict(cache_expiration_time));
  }

  const PolicyStatus<std::string> proxy_mode = GetProxyMode();
  if (proxy_mode) {
    policies.Set("ProxyMode", PolicyStatusToDict(proxy_mode));
  }
  const PolicyStatus<std::string> proxy_pac_url = GetProxyPacUrl();
  if (proxy_pac_url) {
    policies.Set("ProxyPacURL", PolicyStatusToDict(proxy_pac_url));
  }
  const PolicyStatus<std::string> proxy_server = GetProxyServer();
  if (proxy_server) {
    policies.Set("ProxyServer", PolicyStatusToDict(proxy_server));
  }

  base::Value::Dict policies_by_app_id;
  for (const std::string& app_id : GetAppsWithPolicy()) {
    base::Value::Dict policies_by_name;
    const PolicyStatus<int> app_install = GetPolicyForAppInstalls(app_id);
    if (app_install) {
      policies_by_name.Set("Install", PolicyStatusToDict(app_install));
    }
    const PolicyStatus<int> app_update = GetPolicyForAppUpdates(app_id);
    if (app_update) {
      policies_by_name.Set("Update", PolicyStatusToDict(app_update));
    }
    const PolicyStatus<std::string> target_channel = GetTargetChannel(app_id);
    if (target_channel) {
      policies_by_name.Set("TargetChannel", PolicyStatusToDict(target_channel));
    }
    const PolicyStatus<std::string> target_version_prefix =
        GetTargetVersionPrefix(app_id);
    if (target_version_prefix) {
      policies_by_name.Set("TargetVersionPrefix",
                           PolicyStatusToDict(target_version_prefix));
    }
    const PolicyStatus<bool> rollback_allowed =
        IsRollbackToTargetVersionAllowed(app_id);
    if (rollback_allowed) {
      policies_by_name.Set("RollbackToTargetVersionAllowed",
                           PolicyStatusToDict(rollback_allowed));
    }

    if (!policies_by_name.empty()) {
      policies_by_app_id.Set(app_id, std::move(policies_by_name));
    }
  }

  return base::Value::Dict()
      .Set("policiesByName", std::move(policies))
      .Set("policiesByAppId", std::move(policies_by_app_id));
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

bool PolicyService::AreUpdatesSuppressedNow(base::Time now) const {
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
       policy_managers_->managers()) {
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
      status.AddPolicy(policy_manager->HasActiveDevicePolicies(),
                       policy_manager->source(), transformed_result.value());
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
       policy_managers_->managers()) {
    std::optional<T> query_result =
        std::invoke(policy_query_function, policy_manager, app_id);
    if (query_result.has_value()) {
      status.AddPolicy(policy_manager->HasActiveDevicePolicies(),
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
  } else if (proxy_mode.policy().compare(kProxyModeDirect) != 0) {
    is_policy_config_valid = false;
  }

  if (!is_policy_config_valid) {
    VLOG(1) << "Configuration set by policy was invalid.";
    return std::nullopt;
  }

  return policy_service_proxy_configuration;
}

bool IsCloudManaged() {
  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  return dm_storage && (dm_storage->IsValidDMToken() ||
                        (!dm_storage->GetEnrollmentToken().empty() &&
                         !dm_storage->IsDeviceDeregistered()));
}

}  // namespace updater
