// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

const char* kProxyPolicies[] = {
  key::kProxyMode,
  key::kProxyServerMode,
  key::kProxyServer,
  key::kProxyPacUrl,
  key::kProxyBypassList,
};

// Maps the separate policies for proxy settings into a single Dictionary
// policy. This allows to keep the logic of merging policies from different
// sources simple, as all separate proxy policies should be considered as a
// single whole during merging.
void RemapProxyPolicies(PolicyMap* policies) {
  // The highest (level, scope) pair for an existing proxy policy is determined
  // first, and then only policies with those exact attributes are merged.
  PolicyMap::Entry current_priority;  // Defaults to the lowest priority.
  PolicySource inherited_source = POLICY_SOURCE_ENTERPRISE_DEFAULT;
  std::unique_ptr<base::DictionaryValue> proxy_settings(
      new base::DictionaryValue);
  for (size_t i = 0; i < base::size(kProxyPolicies); ++i) {
    const PolicyMap::Entry* entry = policies->Get(kProxyPolicies[i]);
    if (entry) {
      if (entry->has_higher_priority_than(current_priority)) {
        proxy_settings->Clear();
        current_priority = entry->DeepCopy();
        if (entry->source > inherited_source)  // Higher priority?
          inherited_source = entry->source;
      }
      if (!entry->has_higher_priority_than(current_priority) &&
          !current_priority.has_higher_priority_than(*entry)) {
        proxy_settings->Set(kProxyPolicies[i], entry->value->CreateDeepCopy());
      }
      policies->Erase(kProxyPolicies[i]);
    }
  }
  // Sets the new |proxy_settings| if kProxySettings isn't set yet, or if the
  // new priority is higher.
  const PolicyMap::Entry* existing = policies->Get(key::kProxySettings);
  if (!proxy_settings->empty() &&
      (!existing || current_priority.has_higher_priority_than(*existing))) {
    policies->Set(key::kProxySettings, current_priority.level,
                  current_priority.scope, inherited_source,
                  std::move(proxy_settings), nullptr);
  }
}

// Returns the string values of |policy|. Returns an empty set if the values are
// not strings.
base::flat_set<std::string> GetStringListPolicyItems(
    const PolicyBundle& bundle,
    const PolicyNamespace& space,
    const std::string& policy) {
  const PolicyMap& chrome_policies = bundle.Get(space);
  const base::Value* items_ptr = chrome_policies.GetValue(policy);

  if (!items_ptr)
    return base::flat_set<std::string>();

  // Count the items to allocate the right-sized vector for them.
  const auto& item_list = items_ptr->GetList();
  const auto item_count =
      std::count_if(item_list.begin(), item_list.end(),
                    [](const auto& item) { return item.is_string(); });

  // Allocate the storage.
  std::vector<std::string> item_vector;
  item_vector.reserve(item_count);

  // Populate it.
  for (const auto& item : item_list) {
    if (item.is_string())
      item_vector.emplace_back(item.GetString());
  }

  return base::flat_set<std::string>(std::move(item_vector));
}

}  // namespace

PolicyServiceImpl::PolicyServiceImpl(Providers providers)
    : PolicyServiceImpl(std::move(providers),
                        /*initialization_throttled=*/false) {}

PolicyServiceImpl::PolicyServiceImpl(Providers providers,
                                     bool initialization_throttled)
    : providers_(std::move(providers)),
      initialization_throttled_(initialization_throttled) {
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    initialization_complete_[domain] = true;
  for (auto* provider : providers_) {
    provider->AddObserver(this);
    for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
      initialization_complete_[domain] &=
          provider->IsInitializationComplete(static_cast<PolicyDomain>(domain));
    }
  }
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

// static
std::unique_ptr<PolicyServiceImpl>
PolicyServiceImpl::CreateWithThrottledInitialization(Providers providers) {
  return base::WrapUnique(new PolicyServiceImpl(
      std::move(providers), /*initialization_throttled=*/true));
}

PolicyServiceImpl::~PolicyServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto* provider : providers_)
    provider->RemoveObserver(this);
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    PolicyService::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<Observers>& list = observers_[domain];
  if (!list)
    list = std::make_unique<Observers>();
  list->AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       PolicyService::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = observers_.find(domain);
  if (it == observers_.end())
    return;
  it->second->RemoveObserver(observer);
  if (!it->second->might_have_observers()) {
    observers_.erase(it);
  }
}

void PolicyServiceImpl::AddProviderUpdateObserver(
    ProviderUpdateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  provider_update_observers_.AddObserver(observer);
}

void PolicyServiceImpl::RemoveProviderUpdateObserver(
    ProviderUpdateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  provider_update_observers_.RemoveObserver(observer);
}

bool PolicyServiceImpl::HasProvider(
    ConfigurationPolicyProvider* provider) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Contains(providers_, provider);
}

const PolicyMap& PolicyServiceImpl::GetPolicies(
    const PolicyNamespace& ns) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return policy_bundle_.Get(ns);
}

bool PolicyServiceImpl::IsInitializationComplete(PolicyDomain domain) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  if (initialization_throttled_)
    return false;
  return initialization_complete_[domain];
}

void PolicyServiceImpl::RefreshPolicies(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!callback.is_null())
    refresh_callbacks_.push_back(callback);

  if (providers_.empty()) {
    // Refresh is immediately complete if there are no providers. See the note
    // on OnUpdatePolicy() about why this is a posted task.
    update_task_ptr_factory_.InvalidateWeakPtrs();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PolicyServiceImpl::MergeAndTriggerUpdates,
                                  update_task_ptr_factory_.GetWeakPtr()));
  } else {
    // Some providers might invoke OnUpdatePolicy synchronously while handling
    // RefreshPolicies. Mark all as pending before refreshing.
    for (auto* provider : providers_)
      refresh_pending_.insert(provider);
    for (auto* provider : providers_)
      provider->RefreshPolicies();
  }
}

void PolicyServiceImpl::UnthrottleInitialization() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialization_throttled_)
    return;

  initialization_throttled_ = false;
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    MaybeNotifyInitializationComplete(static_cast<PolicyDomain>(domain));
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(1, std::count(providers_.begin(), providers_.end(), provider));
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PolicyServiceImpl::MergeAndTriggerUpdates,
                                update_task_ptr_factory_.GetWeakPtr()));
}

void PolicyServiceImpl::NotifyNamespaceUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto iterator = observers_.find(ns.domain);
  if (iterator != observers_.end()) {
    for (auto& observer : *iterator->second)
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
  for (auto* provider : providers_) {
    PolicyBundle provided_bundle;
    provided_bundle.CopyFrom(provider->policies());
    RemapProxyPolicies(&provided_bundle.Get(chrome_namespace));
    bundle.MergeFrom(provided_bundle);
  }

  // Merges all the mergeable policies
  base::flat_set<std::string> policy_lists_to_merge = GetStringListPolicyItems(
      bundle, chrome_namespace, key::kPolicyListMultipleSourceMergeList);
  base::flat_set<std::string> policy_dictionaries_to_merge =
      GetStringListPolicyItems(bundle, chrome_namespace,
                               key::kPolicyDictionaryMultipleSourceMergeList);

  auto& chrome_policies = bundle.Get(chrome_namespace);

  // This has to be done after setting enterprise default values since it is
  // enabled by default for enterprise users.
  auto* atomic_policy_group_enabled_policy_value =
      chrome_policies.Get(key::kPolicyAtomicGroupsEnabled);

  // This policy has to be ignored if it comes from a user signed-in profile.
  bool atomic_policy_group_enabled =
      atomic_policy_group_enabled_policy_value &&
      atomic_policy_group_enabled_policy_value->value->GetBool() &&
      !((atomic_policy_group_enabled_policy_value->source ==
             POLICY_SOURCE_CLOUD ||
         atomic_policy_group_enabled_policy_value->source ==
             POLICY_SOURCE_PRIORITY_CLOUD) &&
        atomic_policy_group_enabled_policy_value->scope == POLICY_SCOPE_USER);
  auto* value =
      chrome_policies.GetValue(key::kExtensionInstallListsMergeEnabled);
  if (value && value->GetBool()) {
    policy_lists_to_merge.insert(key::kExtensionInstallForcelist);
    policy_lists_to_merge.insert(key::kExtensionInstallBlacklist);
    policy_lists_to_merge.insert(key::kExtensionInstallWhitelist);
  }

  PolicyListMerger policy_list_merger(std::move(policy_lists_to_merge));
  PolicyDictionaryMerger policy_dictionary_merger(
      std::move(policy_dictionaries_to_merge));

  std::vector<PolicyMerger*> mergers{&policy_list_merger,
                                     &policy_dictionary_merger};

  PolicyGroupMerger policy_group_merger;
  if (atomic_policy_group_enabled)
    mergers.push_back(&policy_group_merger);

  for (auto it = bundle.begin(); it != bundle.end(); ++it)
    it->second->MergeValues(mergers);

  // Swap first, so that observers that call GetPolicies() see the current
  // values.
  policy_bundle_.Swap(&bundle);

  // Only notify observers of namespaces that have been modified.
  const PolicyMap kEmpty;
  PolicyBundle::const_iterator it_new = policy_bundle_.begin();
  PolicyBundle::const_iterator end_new = policy_bundle_.end();
  PolicyBundle::const_iterator it_old = bundle.begin();
  PolicyBundle::const_iterator end_old = bundle.end();
  while (it_new != end_new && it_old != end_old) {
    if (it_new->first < it_old->first) {
      // A new namespace is available.
      NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);
      ++it_new;
    } else if (it_old->first < it_new->first) {
      // A previously available namespace is now gone.
      NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);
      ++it_old;
    } else {
      if (!it_new->second->Equals(*it_old->second)) {
        // An existing namespace's policies have changed.
        NotifyNamespaceUpdated(it_new->first, *it_old->second, *it_new->second);
      }
      ++it_new;
      ++it_old;
    }
  }

  // Send updates for the remaining new namespaces, if any.
  for (; it_new != end_new; ++it_new)
    NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);

  // Sends updates for the remaining removed namespaces, if any.
  for (; it_old != end_old; ++it_old)
    NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);

  CheckInitializationComplete();
  CheckRefreshComplete();
  NotifyProviderUpdatesPropagated();
}

void PolicyServiceImpl::CheckInitializationComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check if all the providers just became initialized for each domain; if so,
  // notify that domain's observers.
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
    if (initialization_complete_[domain])
      continue;

    PolicyDomain policy_domain = static_cast<PolicyDomain>(domain);

    bool all_complete = true;
    for (auto* provider : providers_) {
      if (!provider->IsInitializationComplete(policy_domain)) {
        all_complete = false;
        break;
      }
    }
    if (all_complete) {
      initialization_complete_[domain] = true;
      MaybeNotifyInitializationComplete(policy_domain);
    }
  }
}

void PolicyServiceImpl::MaybeNotifyInitializationComplete(
    PolicyDomain policy_domain) {
  if (initialization_throttled_)
    return;
  if (!initialization_complete_[policy_domain])
    return;
  auto iter = observers_.find(policy_domain);
  if (iter != observers_.end()) {
    for (auto& observer : *iter->second)
      observer.OnPolicyServiceInitialized(policy_domain);
  }
}

void PolicyServiceImpl::CheckRefreshComplete() {
  // Invoke all the callbacks if a refresh has just fully completed.
  if (refresh_pending_.empty() && !refresh_callbacks_.empty()) {
    std::vector<base::Closure> callbacks;
    callbacks.swap(refresh_callbacks_);
    std::vector<base::Closure>::iterator it;
    for (it = callbacks.begin(); it != callbacks.end(); ++it)
      it->Run();
  }
}

}  // namespace policy
