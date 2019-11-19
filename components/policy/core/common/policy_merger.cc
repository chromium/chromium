// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <map>
#include <set>

#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

constexpr std::array<const char*, 7> kDictionaryPoliciesToMerge{
    key::kContentPackManualBehaviorURLs,
    key::kExtensionSettings,
    key::kDeviceLoginScreenPowerManagement,
    key::kKeyPermissions,
    key::kPowerManagementIdleSettings,
    key::kScreenBrightnessPercent,
    key::kScreenLockDelays,
};

}  // namespace

// static
bool PolicyMerger::ConflictCanBeMerged(const PolicyMap::Entry& conflict,
                                       const PolicyMap::Entry& policy) {
  // On desktop, the user cloud policy potentially comes from a different
  // domain than e.g. GPO policy or machine-level cloud policy, so prevent
  // merging user cloud policy with other policy sources.
  const bool is_conflict_user_cloud_policy =
      conflict.scope == POLICY_SCOPE_USER &&
      (conflict.source == POLICY_SOURCE_CLOUD ||
       conflict.source == POLICY_SOURCE_PRIORITY_CLOUD);
  return !is_conflict_user_cloud_policy && !conflict.IsBlockedOrIgnored() &&
         conflict.source != POLICY_SOURCE_ENTERPRISE_DEFAULT &&
         conflict.level == policy.level && conflict.scope == policy.scope;
}

PolicyMerger::PolicyMerger() = default;
PolicyMerger::~PolicyMerger() = default;

PolicyListMerger::PolicyListMerger(
    base::flat_set<std::string> policies_to_merge)
    : policies_to_merge_(std::move(policies_to_merge)) {}
PolicyListMerger::~PolicyListMerger() = default;

PolicyGroupMerger::PolicyGroupMerger() = default;
PolicyGroupMerger::~PolicyGroupMerger() = default;

void PolicyListMerger::Merge(PolicyMap::PolicyMapType* policies) const {
  DCHECK(policies);
  for (auto& it : *policies) {
    if (CanMerge(it.first, it.second))
      DoMerge(&it.second);
  }
}

bool PolicyListMerger::CanMerge(const std::string& policy_name,
                                PolicyMap::Entry& policy) const {
  if (policy.source == POLICY_SOURCE_MERGED)
    return false;

  if (policies_to_merge_.find("*") != policies_to_merge_.end())
    return policy.value->is_list();

  if (policies_to_merge_.find(policy_name) == policies_to_merge_.end())
    return false;

  if (!policy.value->is_list()) {
    policy.AddError(IDS_POLICY_LIST_MERGING_WRONG_POLICY_TYPE_SPECIFIED);
    return false;
  }

  return true;
}

void PolicyListMerger::DoMerge(PolicyMap::Entry* policy) const {
  std::vector<const base::Value*> merged_values;
  auto compare_value_ptr = [](const base::Value* a, const base::Value* b) {
    return *a < *b;
  };
  std::set<const base::Value*, decltype(compare_value_ptr)> duplicates(
      compare_value_ptr);
  bool value_changed = false;

  for (const base::Value& val : policy->value->GetList()) {
    if (duplicates.find(&val) != duplicates.end())
      continue;
    duplicates.insert(&val);
    merged_values.push_back(&val);
  }

  // Concatenates the values from accepted conflicting sources to the policy
  // value while avoiding duplicates.
  for (const auto& it : policy->conflicts) {
    if (!PolicyMerger::ConflictCanBeMerged(it, *policy)) {
      continue;
    }

    for (const base::Value& val : it.value->GetList()) {
      if (duplicates.find(&val) != duplicates.end())
        continue;
      duplicates.insert(&val);
      merged_values.push_back(&val);
    }

    value_changed = true;
  }

  auto new_conflict = policy->DeepCopy();
  if (value_changed) {
    base::ListValue* new_value = new base::ListValue();
    for (const base::Value* it : merged_values)
      new_value->Append(it->Clone());

    policy->value.reset(new_value);
  }
  policy->ClearConflicts();
  policy->AddConflictingPolicy(std::move(new_conflict));
  policy->source = POLICY_SOURCE_MERGED;
}

PolicyDictionaryMerger::PolicyDictionaryMerger(
    base::flat_set<std::string> policies_to_merge)
    : policies_to_merge_(std::move(policies_to_merge)),
      allowed_policies_(kDictionaryPoliciesToMerge.begin(),
                        kDictionaryPoliciesToMerge.end()) {}
PolicyDictionaryMerger::~PolicyDictionaryMerger() = default;

void PolicyDictionaryMerger::Merge(PolicyMap::PolicyMapType* policies) const {
  DCHECK(policies);
  for (auto& it : *policies) {
    if (CanMerge(it.first, it.second))
      DoMerge(&it.second);
  }
}

void PolicyDictionaryMerger::SetAllowedPoliciesForTesting(
    base::flat_set<std::string> allowed_policies) {
  allowed_policies_ = std::move(allowed_policies);
}

bool PolicyDictionaryMerger::CanMerge(const std::string& policy_name,
                                      PolicyMap::Entry& policy) const {
  if (policy.source == POLICY_SOURCE_MERGED)
    return false;

  const bool allowed_to_merge =
      allowed_policies_.find(policy_name) != allowed_policies_.end();

  if (policies_to_merge_.find("*") != policies_to_merge_.end())
    return allowed_to_merge && policy.value->is_dict();

  if (policies_to_merge_.find(policy_name) == policies_to_merge_.end())
    return false;

  if (!allowed_to_merge) {
    policy.AddError(IDS_POLICY_DICTIONARY_MERGING_POLICY_NOT_ALLOWED);
    return false;
  }

  if (!policy.value->is_dict()) {
    policy.AddError(IDS_POLICY_DICTIONARY_MERGING_WRONG_POLICY_TYPE_SPECIFIED);
    return false;
  }

  return true;
}

void PolicyDictionaryMerger::DoMerge(PolicyMap::Entry* policy) const {
  // Keep priority sorted list of potential merge targets.
  std::vector<const PolicyMap::Entry*> policies;
  policies.push_back(policy);
  for (const auto& it : policy->conflicts)
    policies.push_back(&it);

  std::sort(policies.begin(), policies.end(),
            [](const PolicyMap::Entry* a, const PolicyMap::Entry* b) {
              return b->has_higher_priority_than(*a);
            });

  base::DictionaryValue merged_dictionary;
  bool value_changed = false;

  // Merges all the keys from the policies from different sources.
  for (const auto* it : policies) {
    if (it != policy && !PolicyMerger::ConflictCanBeMerged(*it, *policy))
      continue;

    base::DictionaryValue* dict = nullptr;

    it->value->GetAsDictionary(&dict);
    DCHECK(dict);

    for (const auto& pair : *dict) {
      const auto& key = pair.first;
      const auto& val = pair.second;
      merged_dictionary.SetKey(key, val->Clone());
    }

    value_changed |= it != policy;
  }

  auto new_conflict = policy->DeepCopy();
  if (value_changed)
    policy->value = base::Value::ToUniquePtrValue(std::move(merged_dictionary));

  policy->ClearConflicts();
  policy->AddConflictingPolicy(std::move(new_conflict));
  policy->source = POLICY_SOURCE_MERGED;
}

void PolicyGroupMerger::Merge(PolicyMap::PolicyMapType* policies) const {
  for (size_t i = 0; i < kPolicyAtomicGroupMappingsLength; ++i) {
    const AtomicGroup& group = kPolicyAtomicGroupMappings[i];
    bool use_highest_set_priority = false;

    // Defaults to the lowest priority.
    PolicyMap::Entry highest_set_priority;

    // Find the policy with the highest priority that is both in |policies| and
    // |group.policies|, an array ending with a nullptr.
    for (const char* const* policy_name = group.policies; *policy_name;
         ++policy_name) {
      auto policy_it = policies->find(*policy_name);

      if (policy_it == policies->end())
        continue;

      use_highest_set_priority = true;

      PolicyMap::Entry& policy = policy_it->second;

      if (!policy.has_higher_priority_than(highest_set_priority))
        continue;

      // Do not set POLICY_SOURCE_MERGED as the highest acceptable source
      // because it is a computed source. In case of an already merged policy,
      // the highest acceptable source must be the highest of the ones used to
      // compute the merged value.
      if (policy.source != POLICY_SOURCE_MERGED) {
        highest_set_priority = policy.DeepCopy();
      } else {
        for (const auto& conflict : policy.conflicts) {
          if (conflict.has_higher_priority_than(highest_set_priority) &&
              conflict.source > highest_set_priority.source) {
            highest_set_priority = conflict.DeepCopy();
          }
        }
      }
    }

    if (!use_highest_set_priority)
      continue;

    // Ignore the policies from |group.policies|, an array ending with a
    // nullptr, that do not share the same source as the one with the highest
    // priority.
    for (const char* const* policy_name = group.policies; *policy_name;
         ++policy_name) {
      auto policy_it = policies->find(*policy_name);
      if (policy_it == policies->end())
        continue;

      PolicyMap::Entry& policy = policy_it->second;

      if (policy.source < highest_set_priority.source)
        policy.SetIgnoredByPolicyAtomicGroup();
    }
  }
}

}  // namespace policy
