// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/policy_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/proxy_settings_constants.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/android/policy_service_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/policy/core/common/default_chrome_apps_migrator.h"
#endif

namespace policy {

namespace {

// Metrics should not be enforced so if this policy is set as mandatory
// downgrade it to a recommended level policy.
void DowngradeMetricsReportingToRecommendedPolicy(PolicyMap* policies) {
  // Capture both the Chrome-only and device-level policies on Chrome OS.
  const std::vector<const char*> metrics_keys = {
#if BUILDFLAG(IS_CHROMEOS)
    policy::key::kDeviceMetricsReportingEnabled,
#else
    policy::key::kMetricsReportingEnabled,
#endif
  };
  for (const char* policy_key : metrics_keys) {
    PolicyMap::Entry* policy = policies->GetMutable(policy_key);
    if (policy && policy->level != POLICY_LEVEL_RECOMMENDED &&
        policy->value(base::Value::Type::BOOLEAN) &&
        policy->value(base::Value::Type::BOOLEAN)->GetBool()) {
      policy->level = POLICY_LEVEL_RECOMMENDED;
      policy->AddMessage(PolicyMap::MessageType::kInfo,
                         IDS_POLICY_IGNORED_MANDATORY_REPORTING_POLICY);
    }
  }
}

// Returns the string values of |policy|. Returns an empty set if the values are
// not strings.
base::flat_set<std::string> GetStringListPolicyItems(
    const PolicyBundle& bundle,
    const PolicyNamespace& space,
    const std::string& policy) {
  return ValueToStringSet(
      bundle.Get(space).GetValue(policy, base::Value::Type::LIST));
}

bool IsUserCloudMergingAllowed(const PolicyMap& policies) {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  const base::Value* cloud_user_policy_merge_value =
      policies.GetValue(key::kCloudUserPolicyMerge, base::Value::Type::BOOLEAN);
  return cloud_user_policy_merge_value &&
         cloud_user_policy_merge_value->GetBool();
#endif
}

void AddPolicyMessages(PolicyMap& policies) {
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
  // Add warning to inform users that these policies are ignored when the user
  // is unaffiliated.
  if (policies.IsUserAffiliated())
    return;

  auto* cloud_user_precedence_entry =
      policies.GetMutable(key::kCloudUserPolicyOverridesCloudMachinePolicy);
  if (cloud_user_precedence_entry &&
      cloud_user_precedence_entry->value(base::Value::Type::BOOLEAN) &&
      cloud_user_precedence_entry->value(base::Value::Type::BOOLEAN)
          ->GetBool()) {
    cloud_user_precedence_entry->AddMessage(PolicyMap::MessageType::kError,
                                            IDS_POLICY_IGNORED_UNAFFILIATED);
  }

  if (IsUserCloudMergingAllowed(policies)) {
    policies.GetMutable(key::kCloudUserPolicyMerge)
        ->AddMessage(PolicyMap::MessageType::kError,
                     IDS_POLICY_IGNORED_UNAFFILIATED);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
}

// Returns the list of histogram names to record depending on scope and number
// of policies.
std::vector<std::string> InitTimeHistogramsToRecord(
    PolicyServiceImpl::ScopeForMetrics scope_for_metrics,
    size_t policy_count) {
  CHECK_NE(scope_for_metrics, PolicyServiceImpl::ScopeForMetrics::kUnspecified);

  std::string name_with_scope =
      scope_for_metrics == PolicyServiceImpl::ScopeForMetrics::kMachine
          ? base::StrCat({PolicyServiceImpl::kInitTimeHistogramPrefix,
                          PolicyServiceImpl::kMachineHistogramSuffix})
          : base::StrCat({PolicyServiceImpl::kInitTimeHistogramPrefix,
                          PolicyServiceImpl::kUserHistogramSuffix});

  std::vector<std::string> histogram_names{name_with_scope};

  if (policy_count == 0) {
    histogram_names.push_back(base::StrCat(
        {name_with_scope, PolicyServiceImpl::kWithoutPoliciesHistogramSuffix}));
  } else {
    histogram_names.push_back(base::StrCat(
        {name_with_scope, PolicyServiceImpl::kWithPoliciesHistogramSuffix}));
    if (policy_count <= 50u) {
      histogram_names.push_back(
          base::StrCat({name_with_scope,
                        PolicyServiceImpl::kWith1to50PoliciesHistogramSuffix}));
    } else if (policy_count <= 100u) {
      histogram_names.push_back(base::StrCat(
          {name_with_scope,
           PolicyServiceImpl::kWith51to100PoliciesHistogramSuffix}));
    } else {
      histogram_names.push_back(base::StrCat(
          {name_with_scope,
           PolicyServiceImpl::kWith101PlusPoliciesHistogramSuffix}));
    }
  }

  return histogram_names;
}

}  // namespace

PolicyServiceImpl::PolicyServiceImpl(Providers providers,
                                     ScopeForMetrics scope_for_metrics,
                                     Migrators migrators)
    : PolicyServiceImpl(std::move(providers),
                        scope_for_metrics,
                        std::move(migrators),
                        /*initialization_throttled=*/false) {}

PolicyServiceImpl::PolicyServiceImpl(Providers providers,
                                     ScopeForMetrics scope_for_metrics,
                                     Migrators migrators,
                                     bool initialization_throttled)
    : providers_(std::move(providers)),
      migrators_(std::move(migrators)),
      initialization_throttled_(initialization_throttled),
      scope_for_metrics_(scope_for_metrics) {
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    policy_domain_status_[domain] = PolicyDomainStatus::kUninitialized;

  for (policy::ConfigurationPolicyProvider* provider : providers_) {
    provider->AddObserver(this);
  }
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

// static
std::unique_ptr<PolicyServiceImpl>
PolicyServiceImpl::CreateWithThrottledInitialization(
    Providers providers,
    ScopeForMetrics scope_for_metrics,
    Migrators migrators) {
  return base::WrapUnique(new PolicyServiceImpl(
      std::move(providers), scope_for_metrics, std::move(migrators),
      /*initialization_throttled=*/true));
}

PolicyServiceImpl::~PolicyServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (policy::ConfigurationPolicyProvider* provider : providers_) {
    provider->RemoveObserver(this);
  }
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    PolicyService::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_[domain].AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       PolicyService::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = observers_.find(domain);
  if (it == observers_.end())
    return;
  it->second.RemoveObserver(observer);
  if (it->second.empty()) {
    observers_.erase(it);
  }
}

void PolicyServiceImpl::AddProviderUpdateObserver(
    ProviderUpdateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  provider_update_observers_.AddObserver(observer);
}

void PolicyServiceImpl::RemoveProviderUpdateObserver(
    ProviderUpdateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  provider_update_observers_.RemoveObserver(observer);
}

bool PolicyServiceImpl::HasProvider(
    ConfigurationPolicyProvider* provider) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(providers_, provider);
}

const PolicyMap& PolicyServiceImpl::GetPolicies(
    const PolicyNamespace& ns) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return policy_bundle_.Get(ns);
}

bool PolicyServiceImpl::IsInitializationComplete(PolicyDomain domain) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  return !initialization_throttled_ &&
         policy_domain_status_[domain] != PolicyDomainStatus::kUninitialized;
}

bool PolicyServiceImpl::IsFirstPolicyLoadComplete(PolicyDomain domain) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  return !initialization_throttled_ &&
         policy_domain_status_[domain] == PolicyDomainStatus::kPolicyReady;
}

void PolicyServiceImpl::RefreshPolicies(base::OnceClosure callback,
                                        PolicyFetchReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG_POLICY(2, POLICY_PROCESSING) << "Policy refresh starting";

  if (!callback.is_null())
    refresh_callbacks_.push_back(std::move(callback));

  if (providers_.empty()) {
    // Refresh is immediately complete if there are no providers. See the note
    // on OnUpdatePolicy() about why this is a posted task.
    update_task_ptr_factory_.InvalidateWeakPtrs();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PolicyServiceImpl::MergeAndTriggerUpdates,
                                  update_task_ptr_factory_.GetWeakPtr()));

    VLOG_POLICY(2, POLICY_PROCESSING) << "Policy refresh has no providers";
  } else {
    // Some providers might invoke OnUpdatePolicy synchronously while handling
    // RefreshPolicies. Mark all as pending before refreshing.
    for (policy::ConfigurationPolicyProvider* provider : providers_) {
      refresh_pending_.insert(provider);
    }
    for (policy::ConfigurationPolicyProvider* provider : providers_) {
      provider->RefreshPolicies(reason);
    }
  }
}

#if BUILDFLAG(IS_ANDROID)
android::PolicyServiceAndroid* PolicyServiceImpl::GetPolicyServiceAndroid() {
  if (!policy_service_android_)
    policy_service_android_ =
        std::make_unique<android::PolicyServiceAndroid>(this);
  return policy_service_android_.get();
}
#endif

void PolicyServiceImpl::UnthrottleInitialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initialization_throttled_)
    return;

  initialization_throttled_ = false;
  std::vector<PolicyDomain> updated_domains;
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    updated_domains.push_back(static_cast<PolicyDomain>(domain));
  MaybeNotifyPolicyDomainStatusChange(updated_domains);
}

void PolicyServiceImpl::UseLocalTestPolicyProvider(
    ConfigurationPolicyProvider* provider) {
  local_test_policy_provider_ = provider;
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(1, base::ranges::count(providers_, provider));
  refresh_pending_.erase(provider);
  provider_update_pending_.insert(provider);

  // Note: a policy change may trigger further policy changes in some providers.
  // For example, disabling SigninAllowed would cause the CloudPolicyManager to
  // drop all its policies, which makes this method enter again for that
  // provider.
  //
  // Therefore this update is posted asynchronously, to prevent reentrancy in
  // MergeAndTriggerUpdates. Also, cancel a pending update if there is any,
  // since both will produce the same PolicyBundle.
  update_task_ptr_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PolicyServiceImpl::MergeAndTriggerUpdates,
                                update_task_ptr_factory_.GetWeakPtr()));
}

void PolicyServiceImpl::NotifyNamespaceUpdated(const PolicyNamespace& ns,
                                               const PolicyMap& previous,
                                               const PolicyMap& current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iterator = observers_.find(ns.domain);
  if (iterator != observers_.end()) {
    for (auto& observer : iterator->second)
      observer.OnPolicyUpdated(ns, previous, current);
  }
}

void PolicyServiceImpl::NotifyProviderUpdatesPropagated() {
  if (provider_update_pending_.empty())
    return;

  for (auto& provider_update_observer : provider_update_observers_) {
    for (ConfigurationPolicyProvider* provider : provider_update_pending_) {
      provider_update_observer.OnProviderUpdatePropagated(provider);
    }
  }
  provider_update_pending_.clear();
}

void PolicyServiceImpl::NotifyPoliciesUpdated(const PolicyBundle& old_bundle) {
  // Only notify observers of namespaces that have been modified.
  const PolicyMap kEmpty;
  PolicyBundle::const_iterator it_new = policy_bundle_.begin();
  PolicyBundle::const_iterator end_new = policy_bundle_.end();
  PolicyBundle::const_iterator it_old = old_bundle.begin();
  PolicyBundle::const_iterator end_old = old_bundle.end();
  while (it_new != end_new && it_old != end_old) {
    if (it_new->first < it_old->first) {
      // A new namespace is available.
      NotifyNamespaceUpdated(it_new->first, kEmpty, it_new->second);
      ++it_new;
    } else if (it_old->first < it_new->first) {
      // A previously available namespace is now gone.
      NotifyNamespaceUpdated(it_old->first, it_old->second, kEmpty);
      ++it_old;
    } else {
      if (!it_new->second.Equals(it_old->second)) {
        // An existing namespace's policies have changed.
        NotifyNamespaceUpdated(it_new->first, it_old->second, it_new->second);
      }
      ++it_new;
      ++it_old;
    }
  }

  // Send updates for the remaining new namespaces, if any.
  for (; it_new != end_new; ++it_new) {
    NotifyNamespaceUpdated(it_new->first, kEmpty, it_new->second);
  }

  // Sends updates for the remaining removed namespaces, if any.
  for (; it_old != end_old; ++it_old) {
    NotifyNamespaceUpdated(it_old->first, it_old->second, kEmpty);
  }

  const std::vector<PolicyDomain> updated_domains = UpdatePolicyDomainStatus();
  CheckRefreshComplete();
  NotifyProviderUpdatesPropagated();
  // This has to go last as one of the observers might actually destroy `this`.
  // See https://crbug.com/747817
  MaybeNotifyPolicyDomainStatusChange(updated_domains);
}

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  std::vector<const PolicyBundle*> policy_bundles;

  if (local_test_policy_provider_) {
    policy_bundles.push_back(&local_test_policy_provider_->policies());
  } else {
    for (policy::ConfigurationPolicyProvider* provider : providers_) {
      policy_bundles.push_back(&provider->policies());
    }
  }

  PolicyBundle bundle = MergePolicyBundles(policy_bundles, migrators_);
  auto& chrome_policies =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  // Add informational messages to specific policies.
  AddPolicyMessages(chrome_policies);

  // Swap first, so that observers that call GetPolicies() see the current
  // values.
  std::swap(policy_bundle_, bundle);
  RecordUserAffiliationStatus();
  NotifyPoliciesUpdated(bundle);
}

// static
PolicyBundle PolicyServiceImpl::MergePolicyBundles(
    std::vector<const policy::PolicyBundle*>& bundles,
    Migrators& migrators) {
  // Merge from each provider in their order of priority.
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());
  PolicyBundle bundle;
#if BUILDFLAG(IS_CHROMEOS)
  DefaultChromeAppsMigrator chrome_apps_migrator;
#endif  // BUILDFLAG(IS_CHROMEOS)
  for (const PolicyBundle* policy_bundle : bundles) {
    PolicyBundle provided_bundle = policy_bundle->Clone();
    IgnoreUserCloudPrecedencePolicies(&provided_bundle.Get(chrome_namespace));
    DowngradeMetricsReportingToRecommendedPolicy(
        &provided_bundle.Get(chrome_namespace));
#if BUILDFLAG(IS_CHROMEOS)
    chrome_apps_migrator.Migrate(&provided_bundle.Get(chrome_namespace));
#endif  // BUILDFLAG(IS_CHROMEOS)
    bundle.MergeFrom(provided_bundle);
  }

  auto& chrome_policies = bundle.Get(chrome_namespace);

  // Merges all the mergeable policies
  base::flat_set<std::string> policy_lists_to_merge = GetStringListPolicyItems(
      bundle, chrome_namespace, key::kPolicyListMultipleSourceMergeList);
  base::flat_set<std::string> policy_dictionaries_to_merge =
      GetStringListPolicyItems(bundle, chrome_namespace,
                               key::kPolicyDictionaryMultipleSourceMergeList);

  // This has to be done after setting enterprise default values since it is
  // enabled by default for enterprise users.
  auto* atomic_policy_group_enabled_entry =
      chrome_policies.Get(key::kPolicyAtomicGroupsEnabled);

  // This policy has to be ignored if it comes from a user signed-in profile.
  bool atomic_policy_group_enabled =
      atomic_policy_group_enabled_entry &&
      atomic_policy_group_enabled_entry->value(base::Value::Type::BOOLEAN) &&
      atomic_policy_group_enabled_entry->value(base::Value::Type::BOOLEAN)
          ->GetBool() &&
      !(atomic_policy_group_enabled_entry->source == POLICY_SOURCE_CLOUD &&
        atomic_policy_group_enabled_entry->scope == POLICY_SCOPE_USER);

  PolicyListMerger policy_list_merger(std::move(policy_lists_to_merge));
  PolicyDictionaryMerger policy_dictionary_merger(
      std::move(policy_dictionaries_to_merge));

  // Pass affiliation and CloudUserPolicyMerge values to both mergers.
  const bool is_user_affiliated = chrome_policies.IsUserAffiliated();
  const bool is_user_cloud_merging_enabled =
      IsUserCloudMergingAllowed(chrome_policies);
  policy_list_merger.SetAllowUserCloudPolicyMerging(
      is_user_affiliated && is_user_cloud_merging_enabled);
  policy_dictionary_merger.SetAllowUserCloudPolicyMerging(
      is_user_affiliated && is_user_cloud_merging_enabled);

  std::vector<PolicyMerger*> mergers{&policy_list_merger,
                                     &policy_dictionary_merger};

  PolicyGroupMerger policy_group_merger;
  if (atomic_policy_group_enabled)
    mergers.push_back(&policy_group_merger);

  for (auto& entry : bundle)
    entry.second.MergeValues(mergers);

  for (auto& migrator : migrators) {
    migrator->Migrate(&bundle);
  }

  return bundle;
}

std::vector<PolicyDomain> PolicyServiceImpl::UpdatePolicyDomainStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PolicyDomain> updated_domains;

  // Check if all the providers just became initialized for each domain; if so,
  // notify that domain's observers. If they were initialized, check if they had
  // their first policies loaded.
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
    PolicyDomain policy_domain = static_cast<PolicyDomain>(domain);
    if (policy_domain_status_[domain] == PolicyDomainStatus::kPolicyReady)
      continue;

    PolicyDomainStatus new_status = PolicyDomainStatus::kPolicyReady;

    for (policy::ConfigurationPolicyProvider* provider : providers_) {
      if (!provider->IsInitializationComplete(policy_domain)) {
        new_status = PolicyDomainStatus::kUninitialized;
        break;
      } else if (!provider->IsFirstPolicyLoadComplete(policy_domain)) {
        new_status = PolicyDomainStatus::kInitialized;
      }
    }

    if (new_status == policy_domain_status_[domain])
      continue;

    policy_domain_status_[domain] = new_status;
    updated_domains.push_back(static_cast<PolicyDomain>(domain));
  }
  return updated_domains;
}

void PolicyServiceImpl::MaybeNotifyPolicyDomainStatusChange(
    const std::vector<PolicyDomain>& updated_domains) {
  if (initialization_throttled_)
    return;

  for (const auto policy_domain : updated_domains) {
    if (policy_domain_status_[policy_domain] ==
        PolicyDomainStatus::kUninitialized) {
      continue;
    }

    auto iter = observers_.find(policy_domain);
    if (iter == observers_.end())
      continue;

    // If and when crbug.com/1221454 gets fixed, we should drop the WeakPtr
    // construction and checks here.
    const auto weak_this = weak_ptr_factory_.GetWeakPtr();
    VLOG_POLICY(2, POLICY_PROCESSING)
        << "PolicyService is initialized for domain: " << policy_domain;
    for (auto& observer : iter->second) {
      observer.OnPolicyServiceInitialized(policy_domain);
      if (!weak_this) {
        VLOG_POLICY(1, POLICY_PROCESSING)
            << "PolicyService destroyed while notifying observers.";
        return;
      }
      if (policy_domain_status_[policy_domain] ==
          PolicyDomainStatus::kPolicyReady) {
        observer.OnFirstPoliciesLoaded(policy_domain);
        // If this gets hit, it implies that some OnFirstPoliciesLoaded()
        // observer was changed to trigger the deletion of |this|. See
        // crbug.com/1221454 for a similar problem with
        // OnPolicyServiceInitialized().
        CHECK(weak_this);
      }
    }
  }

  // Record the initialization time for the policy service once the Chrome
  // domain status changes to ready. Ignore if scope is unspecified.
  if (policy_domain_status_[POLICY_DOMAIN_CHROME] ==
          PolicyDomainStatus::kPolicyReady &&
      base::ranges::find(updated_domains, POLICY_DOMAIN_CHROME) !=
          updated_domains.end()) {
    RecordInitializationTime(
        scope_for_metrics_,
        GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
            .size(),
        base::Time::Now() - creation_time_);
  }
}

void PolicyServiceImpl::CheckRefreshComplete() {
  if (refresh_pending_.empty()) {
    VLOG(2) << "Policy refresh complete";
  }

  // Invoke all the callbacks if a refresh has just fully completed.
  if (refresh_pending_.empty() && !refresh_callbacks_.empty()) {
    std::vector<base::OnceClosure> callbacks;
    callbacks.swap(refresh_callbacks_);
    for (auto& callback : callbacks)
      std::move(callback).Run();
  }
}

void PolicyServiceImpl::RecordUserAffiliationStatus() {
  auto& policies =
      policy_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  // All policy fetches seem to include a policy service with an unmanaged user,
  // even if the only open profile is managed. As a result, only log policy
  // fetches where a managed user is involved.
  if (policies.GetUserAffiliationIds().empty()) {
    return;
  }

  CloudUserAffiliationStatus status = CloudUserAffiliationStatus::kUserOnly;
  if (policies.IsUserAffiliated()) {
    status = CloudUserAffiliationStatus::kDeviceAndUserAffiliated;
  } else if (!policies.GetDeviceAffiliationIds().empty()) {
    status = CloudUserAffiliationStatus::kDeviceAndUserUnaffiliated;
  }

  base::UmaHistogramEnumeration("Enterprise.CloudUserAffiliationStatus",
                                status);
}

// static
void PolicyServiceImpl::IgnoreUserCloudPrecedencePolicies(PolicyMap* policies) {
  for (auto* policy_name : metapolicy::kPrecedence) {
    const PolicyMap::Entry* policy_entry = policies->Get(policy_name);
    if (policy_entry && policy_entry->scope == POLICY_SCOPE_USER &&
        policy_entry->source == POLICY_SOURCE_CLOUD) {
      PolicyMap::Entry* policy_entry_mutable =
          policies->GetMutable(policy_name);
      policy_entry_mutable->SetIgnored();
      policy_entry_mutable->AddMessage(PolicyMap::MessageType::kError,
                                       IDS_POLICY_IGNORED_CHROME_PROFILE);
    }
  }
}

// static
void PolicyServiceImpl::RecordInitializationTime(
    PolicyServiceImpl::ScopeForMetrics scope_for_metrics,
    size_t policy_count,
    base::TimeDelta initialization_time) {
  if (scope_for_metrics == ScopeForMetrics::kUnspecified) {
    return;
  }

  for (const std::string& histogram_name :
       InitTimeHistogramsToRecord(scope_for_metrics, policy_count)) {
    base::UmaHistogramLongTimes(histogram_name, initialization_time);
  }
}

}  // namespace policy
