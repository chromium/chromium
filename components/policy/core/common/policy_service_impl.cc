// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
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

}  // namespace

PolicyServiceImpl::PolicyServiceImpl(Providers providers, Migrators migrators)
    : PolicyServiceImpl(std::move(providers),
                        std::move(migrators),
                        /*initialization_throttled=*/false) {}

PolicyServiceImpl::PolicyServiceImpl(Providers providers,
                                     Migrators migrators,
                                     bool initialization_throttled)
    : providers_(std::move(providers)),
      migrators_(std::move(migrators)),
      initialization_throttled_(initialization_throttled) {
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    policy_domain_status_[domain] = PolicyDomainStatus::kUninitialized;

  for (auto* provider : providers_)
    provider->AddObserver(this);
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

// static
std::unique_ptr<PolicyServiceImpl>
PolicyServiceImpl::CreateWithThrottledInitialization(Providers providers,
                                                     Migrators migrators) {
  return base::WrapUnique(
      new PolicyServiceImpl(std::move(providers), std::move(migrators),
                            /*initialization_throttled=*/true));
}

PolicyServiceImpl::~PolicyServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* provider : providers_)
    provider->RemoveObserver(this);
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
    for (auto* provider : providers_)
      refresh_pending_.insert(provider);
    for (auto* provider : providers_)
      provider->RefreshPolicies(reason);
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

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  // Merge from each provider in their order of priority.
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());
  PolicyBundle bundle;
#if BUILDFLAG(IS_CHROMEOS)
  DefaultChromeAppsMigrator chrome_apps_migrator;
#endif  // BUILDFLAG(IS_CHROMEOS)
  for (auto* provider : providers_) {
    if (!provider->is_active()) {
      continue;
    }

    PolicyBundle provided_bundle = provider->policies().Clone();
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

  for (auto& migrator : migrators_)
    migrator->Migrate(&bundle);

  // Add informational messages to specific policies.
  AddPolicyMessages(chrome_policies);

  // Swap first, so that observers that call GetPolicies() see the current
  // values.
  std::swap(policy_bundle_, bundle);

  // Only notify observers of namespaces that have been modified.
  const PolicyMap kEmpty;
  PolicyBundle::const_iterator it_new = policy_bundle_.begin();
  PolicyBundle::const_iterator end_new = policy_bundle_.end();
  PolicyBundle::const_iterator it_old = bundle.begin();
  PolicyBundle::const_iterator end_old = bundle.end();
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
  for (; it_new != end_new; ++it_new)
    NotifyNamespaceUpdated(it_new->first, kEmpty, it_new->second);

  // Sends updates for the remaining removed namespaces, if any.
  for (; it_old != end_old; ++it_old)
    NotifyNamespaceUpdated(it_old->first, it_old->second, kEmpty);

  const std::vector<PolicyDomain> updated_domains = UpdatePolicyDomainStatus();
  CheckRefreshComplete();
  NotifyProviderUpdatesPropagated();
  // This has to go last as one of the observers might actually destroy `this`.
  // See https://crbug.com/747817
  MaybeNotifyPolicyDomainStatusChange(updated_domains);
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

    for (auto* provider : providers_) {
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

}  // namespace policy
