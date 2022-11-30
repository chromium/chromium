// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_

#include <stddef.h>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

namespace policy {

// Abstract class that provides an interface to apply custom merging logic on a
// set of policies.
class POLICY_EXPORT PolicyMerger {
 public:
  PolicyMerger();

  // Determines if two policy entries are eligible for merging with each other
  // depending on several factors including its scope, source, and level.
  static bool EntriesCanBeMerged(const PolicyMap::Entry& entry_1,
                                 const PolicyMap::Entry& entry_2,
                                 const bool is_user_cloud_merging_enabled);

  virtual ~PolicyMerger();
  virtual void Merge(PolicyMap* policies) const = 0;
};

// PolicyListMerger allows the merging of policy lists that have multiple
// sources. Each policy that has to be merged will have the values from its
// multiple sources concatenated without duplicates.
class POLICY_EXPORT PolicyListMerger : public PolicyMerger {
 public:
  explicit PolicyListMerger(base::flat_set<std::string> policies_to_merge);
  PolicyListMerger(const PolicyListMerger&) = delete;
  PolicyListMerger& operator=(const PolicyListMerger&) = delete;
  ~PolicyListMerger() override;

  // Merges the list policies from |policies| that have multiple sources.
  void Merge(PolicyMap* policies) const override;

  // Sets the variable used for determining if user cloud merging is enabled.
  void SetAllowUserCloudPolicyMerging(bool allowed);

 private:
  // Returns True if |policy_name| is in the list of policies to merge and if
  // |policy| has values from different sources that share the same level,
  // target and scope.
  bool CanMerge(const std::string& policy_name, PolicyMap::Entry& policy) const;

  // Returns True if user cloud policy merging is enabled through the
  // CloudUserPolicyMerge policy and the current user is affiliated.
  bool AllowUserCloudPolicyMerging() const;

  // Merges the values of |policy| if they come from multiple sources. Keeps
  // track of the original values by leaving them as conflicts. |policy| must
  // remain unchanged if there is nothing to merge.
  void DoMerge(PolicyMap::Entry* policy) const;

  bool allow_user_cloud_policy_merging_ = false;
  const base::flat_set<std::string> policies_to_merge_;
};

// PolicyDictionaryMerger allows the merging of policy dictionaries that have
// multiple sources. Each policy that has to be merged will have its first level
// keys merged into one dictionary, each conflict will be resolved by
// using the key coming from the highest priority source.
class POLICY_EXPORT PolicyDictionaryMerger : public PolicyMerger {
 public:
  explicit PolicyDictionaryMerger(
      base::flat_set<std::string> policies_to_merge);
  PolicyDictionaryMerger(const PolicyDictionaryMerger&) = delete;
  PolicyDictionaryMerger& operator=(const PolicyDictionaryMerger&) = delete;
  ~PolicyDictionaryMerger() override;

  // Merges the dictionary policies from |policies| that have multiple sources.
  void Merge(PolicyMap* policies) const override;
  void SetAllowedPoliciesForTesting(
      base::flat_set<std::string> allowed_policies);

  // Sets the variable used for determining if user cloud merging is enabled.
  void SetAllowUserCloudPolicyMerging(bool allowed);

 private:
  // Returns True if |policy_name| is in the list of policies to merge and if
  // |policy| has values from different sources that share the same level,
  // target and scope.
  bool CanMerge(const std::string& policy_name, PolicyMap::Entry& policy) const;

  // Returns True if user cloud policy merging is enabled through the
  // CloudUserPolicyMerge policy and the current user is affiliated.
  bool AllowUserCloudPolicyMerging() const;

  // Merges the values of |policy| if they come from multiple sources. Keeps
  // track of the original values by leaving them as conflicts. |policy| stays
  // intact if there is nothing to merge.
  void DoMerge(PolicyMap::Entry* policy, const PolicyMap& policy_map) const;

  bool allow_user_cloud_policy_merging_ = false;
  const base::flat_set<std::string> policies_to_merge_;
  base::flat_set<std::string> allowed_policies_;
};

// PolicyGroupMerger enforces atomic policy groups. It disables the policies
// from a group that do not share the highest priority from that group.
class POLICY_EXPORT PolicyGroupMerger : public PolicyMerger {
 public:
  PolicyGroupMerger();
  PolicyGroupMerger(const PolicyGroupMerger&) = delete;
  PolicyGroupMerger& operator=(const PolicyGroupMerger&) = delete;
  ~PolicyGroupMerger() override;

  // Disables policies from atomic groups that do not share the highest priority
  // from that group.
  void Merge(PolicyMap* result) const override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_
