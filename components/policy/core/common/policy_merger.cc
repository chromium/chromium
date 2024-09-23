// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/policy_merger.h"

#include <array>
#include <map>
#include <set>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
constexpr const char* kDictionaryPoliciesToMerge[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    key::kExtensionSettings,       key::kDeviceLoginScreenPowerManagement,
    key::kKeyPermissions,          key::kPowerManagementIdleSettings,
    key::kScreenBrightnessPercent, key::kScreenLockDelays,
#else
    key::kExtensionSettings,
#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)
};
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace

// static
bool PolicyMerger::EntriesCanBeMerged(
    const PolicyMap::Entry& entry_1,
    const PolicyMap::Entry& entry_2,
    const bool is_user_cloud_merging_enabled) {
  if (entry_1.value_unsafe()->type() != entry_2.value_unsafe()->type())
    return false;

  if (entry_1.ignored() || entry_2.ignored() ||
      entry_1.source == POLICY_SOURCE_ENTERPRISE_DEFAULT ||
      entry_2.source == POLICY_SOURCE_ENTERPRISE_DEFAULT ||
      entry_1.level != entry_2.level)
    return false;

  // If the policies have matching scope and are non-user, they can be merged.
  if (entry_1.scope == entry_2.scope && entry_1.scope != POLICY_SCOPE_USER)
    return true;

  // Merging of user-level GPO policies is not permitted to prevent unexpected
  // behavior. If such merging is desired, it will be implemented in a similar
  // way as user cloud merging.
  if ((entry_1.scope == POLICY_SCOPE_USER &&
       entry_1.source == POLICY_SOURCE_PLATFORM) ||
      (entry_2.scope == POLICY_SCOPE_USER &&
       entry_2.source == POLICY_SOURCE_PLATFORM))
    return false;

  // On desktop, the user cloud policy potentially comes from a different
  // domain than e.g. GPO policy or machine-level cloud policy. Merging a user
  // cloud policy with policies from other sources is only permitted if both of
  // the following conditions are met:
  //   1. The CloudUserPolicyMerge metapolicy is set to True.
  //   2. The user is affiliated with the machine-level cloud policy provider.
  const bool has_user_cloud_policy = (entry_1.scope == POLICY_SCOPE_USER &&
                                      entry_1.source == POLICY_SOURCE_CLOUD) ||
                                     (entry_2.scope == POLICY_SCOPE_USER &&
                                      entry_2.source == POLICY_SOURCE_CLOUD);
  const bool is_user_cloud_condition_satisfied =
      !has_user_cloud_policy || is_user_cloud_merging_enabled;

  // For the scope condition to be satisfied, either the scopes of the two
  // policies should match or the policy override should be enabled. The scope
  // check override is only enabled when a user cloud policy is present and user
  // cloud merging is enabled -- this allows user cloud policies to merge with
  // machine-level policies.
  const bool is_scope_overriden =
      has_user_cloud_policy && is_user_cloud_merging_enabled;
  const bool is_scope_condition_satisfied =
      entry_1.scope == entry_2.scope || is_scope_overriden;

  return is_user_cloud_condition_satisfied && is_scope_condition_satisfied;
}

PolicyMerger::PolicyMerger() = default;
PolicyMerger::~PolicyMerger() = default;

PolicyListMerger::PolicyListMerger(
    base::flat_set<std::string> policies_to_merge)
    : policies_to_merge_(std::move(policies_to_merge)) {}
PolicyListMerger::~PolicyListMerger() = default;

PolicyGroupMerger::PolicyGroupMerger() = default;
PolicyGroupMerger::~PolicyGroupMerger() = default;

void PolicyListMerger::Merge(PolicyMap* policies) const {
  DCHECK(policies);
  for (auto& it : *policies) {
    if (CanMerge(it.first, it.second))
      DoMerge(&it.second);
  }
}

void PolicyListMerger::SetAllowUserCloudPolicyMerging(bool allowed) {
  allow_user_cloud_policy_merging_ = allowed;
}

bool PolicyListMerger::CanMerge(const std::string& policy_name,
                                PolicyMap::Entry& policy) const {
  if (policy.source == POLICY_SOURCE_MERGED)
    return false;

  if (policies_to_merge_.find("*") != policies_to_merge_.end()) {
    return policy.HasConflicts() &&
           policy.value(base::Value::Type::LIST) != nullptr;
  }

  if (policies_to_merge_.find(policy_name) == policies_to_merge_.end())
    return false;

  if (!policy.value(base::Value::Type::LIST)) {
    policy.AddMessage(PolicyMap::MessageType::kError,
                      IDS_POLICY_LIST_MERGING_WRONG_POLICY_TYPE_SPECIFIED);
    return false;
  }

  return policy.HasConflicts();
}

bool PolicyListMerger::AllowUserCloudPolicyMerging() const {
  return allow_user_cloud_policy_merging_;
}

void PolicyListMerger::DoMerge(PolicyMap::Entry* policy) const {
  std::vector<const base::Value*> merged_values;
  auto compare_value_ptr = [](const base::Value* a, const base::Value* b) {
    return *a < *b;
  };
  std::set<const base::Value*, decltype(compare_value_ptr)> duplicates(
      compare_value_ptr);
  bool value_changed = false;

  for (const base::Value& val :
       policy->value(base::Value::Type::LIST)->GetList()) {
    if (duplicates.find(&val) != duplicates.end())
      continue;
    duplicates.insert(&val);
    merged_values.push_back(&val);
  }

  // Concatenates the values from accepted conflicting sources to the policy
  // value while avoiding duplicates.
  for (const auto& it : policy->conflicts) {
    if (!PolicyMerger::EntriesCanBeMerged(it.entry(), *policy,
                                          AllowUserCloudPolicyMerging())) {
      continue;
    }

    for (const base::Value& val :
         it.entry().value(base::Value::Type::LIST)->GetList()) {
      if (duplicates.find(&val) != duplicates.end())
        continue;
      duplicates.insert(&val);
      merged_values.push_back(&val);
    }

    value_changed = true;
  }

  auto new_conflict = policy->DeepCopy();
  if (value_changed) {
    base::Value::List new_value;
    for (const base::Value* it : merged_values)
      new_value.Append(it->Clone());

    policy->set_value(base::Value(std::move(new_value)));
  }
  policy->ClearConflicts();
  policy->AddConflictingPolicy(std::move(new_conflict));
  policy->source = POLICY_SOURCE_MERGED;
}

PolicyDictionaryMerger::PolicyDictionaryMerger(
    base::flat_set<std::string> policies_to_merge)
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    : policies_to_merge_(std::move(policies_to_merge)){}
#else
    : policies_to_merge_(std::move(policies_to_merge)),
      allowed_policies_(std::begin(kDictionaryPoliciesToMerge),
                        std::end(kDictionaryPoliciesToMerge)) {
}
#endif
      PolicyDictionaryMerger::~PolicyDictionaryMerger() = default;

void PolicyDictionaryMerger::Merge(PolicyMap* policies) const {
  DCHECK(policies);
  for (auto& it : *policies) {
    if (CanMerge(it.first, it.second))
      DoMerge(&it.second, *policies);
  }
}

void PolicyDictionaryMerger::SetAllowedPoliciesForTesting(
    base::flat_set<std::string> allowed_policies) {
  allowed_policies_ = std::move(allowed_policies);
}

void PolicyDictionaryMerger::SetAllowUserCloudPolicyMerging(bool allowed) {
  allow_user_cloud_policy_merging_ = allowed;
}

bool PolicyDictionaryMerger::CanMerge(const std::string& policy_name,
                                      PolicyMap::Entry& policy) const {
  if (policy.source == POLICY_SOURCE_MERGED)
    return false;

  const bool allowed_to_merge =
      allowed_policies_.find(policy_name) != allowed_policies_.end();

  if (policies_to_merge_.find("*") != policies_to_merge_.end()) {
    return allowed_to_merge && policy.HasConflicts() &&
           policy.value(base::Value::Type::DICT);
  }

  if (policies_to_merge_.find(policy_name) == policies_to_merge_.end())
    return false;

  if (!allowed_to_merge) {
    policy.AddMessage(PolicyMap::MessageType::kError,
                      IDS_POLICY_DICTIONARY_MERGING_POLICY_NOT_ALLOWED);
    return false;
  }

  if (!policy.value(base::Value::Type::DICT)) {
    policy.AddMessage(
        PolicyMap::MessageType::kError,
        IDS_POLICY_DICTIONARY_MERGING_WRONG_POLICY_TYPE_SPECIFIED);
    return false;
  }

  return policy.HasConflicts();
}

bool PolicyDictionaryMerger::AllowUserCloudPolicyMerging() const {
  return allow_user_cloud_policy_merging_;
}

void PolicyDictionaryMerger::DoMerge(PolicyMap::Entry* policy,
                                     const PolicyMap& policy_map) const {
  // Keep priority sorted list of potential merge targets.
  std::vector<const PolicyMap::Entry*> policies;
  policies.push_back(policy);
  for (const auto& it : policy->conflicts)
    policies.push_back(&it.entry());
  std::sort(
      policies.begin(), policies.end(),
      [&policy_map](const PolicyMap::Entry* a, const PolicyMap::Entry* b) {
        return policy_map.EntryHasHigherPriority(*b, *a);
      });

  base::Value::Dict merged_dictionary;
  bool value_changed = false;

  // Merges all the keys from the policies from different sources.
  for (const auto* it : policies) {
    if (it != policy && !PolicyMerger::EntriesCanBeMerged(
                            *it, *policy, AllowUserCloudPolicyMerging()))
      continue;

    const base::Value::Dict* dict =
        it->value(base::Value::Type::DICT)->GetIfDict();
    DCHECK(dict);

    for (auto pair : *dict) {
      const auto& key = pair.first;
      const auto& val = pair.second;
      merged_dictionary.Set(key, val.Clone());
    }

    value_changed |= it != policy;
  }

  auto new_conflict = policy->DeepCopy();
  if (value_changed)
    policy->set_value(base::Value(std::move(merged_dictionary)));

  policy->ClearConflicts();
  policy->AddConflictingPolicy(std::move(new_conflict));
  policy->source = POLICY_SOURCE_MERGED;
}

void PolicyGroupMerger::Merge(PolicyMap* policies) const {
  for (size_t i = 0; i < kPolicyAtomicGroupMappingsLength; ++i) {
    const AtomicGroup& group = kPolicyAtomicGroupMappings[i];
    bool use_highest_set_priority = false;

    // Defaults to the lowest priority.
    PolicyMap::Entry highest_set_priority;

    // Find the policy with the highest priority that is both in |policies| and
    // |group.policies|, an array ending with a nullptr.
    for (const char* const* policy_name = group.policies; *policy_name;
         ++policy_name) {
      const auto* policy = policies->Get(*policy_name);
      if (!policy)
        continue;

      use_highest_set_priority = true;

      if (!policies->EntryHasHigherPriority(*policy, highest_set_priority))
        continue;

      // Do not set POLICY_SOURCE_MERGED as the highest acceptable source
      // because it is a computed source. In case of an already merged policy,
      // the highest acceptable source must be the highest of the ones used to
      // compute the merged value.
      if (policy->source != POLICY_SOURCE_MERGED) {
        highest_set_priority = policy->DeepCopy();
      } else {
        for (const auto& conflict : policy->conflicts) {
          if (policies->EntryHasHigherPriority(conflict.entry(),
                                               highest_set_priority) &&
              conflict.entry().source > highest_set_priority.source) {
            highest_set_priority = conflict.entry().DeepCopy();
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
      auto* policy = policies->GetMutable(*policy_name);
      if (!policy)
        continue;

      if (policy->source < highest_set_priority.source)
        policy->SetIgnoredByPolicyAtomicGroup();
    }
  }
}

}  // namespace policy
