// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

namespace policy {

// Abstract class that provides an interface to apply custom merging logic on a
// set of policies.
class POLICY_EXPORT PolicyMerger {
 public:
  PolicyMerger();
  static bool ConflictCanBeMerged(const PolicyMap::Entry& conflict,
                                  const PolicyMap::Entry& policy);
  virtual ~PolicyMerger();
  virtual void Merge(PolicyMap::PolicyMapType* policies) const = 0;
};

// PolicyListMerger allows the merging of policy lists that have multiple
// sources. Each policy that has to be merged will have the values from its
// multiple sources concatenated without duplicates.
class POLICY_EXPORT PolicyListMerger : public PolicyMerger {
 public:
  explicit PolicyListMerger(base::flat_set<std::string> policies_to_merge);
  ~PolicyListMerger() override;

  // Merges the list policies from |policies| that have multiple sources.
  void Merge(PolicyMap::PolicyMapType* policies) const override;

 private:
  // Returns True if |policy_name| is in the list of policies to merge and if
  // |policy| has values from different sources that share the same level,
  // target and scope.
  bool CanMerge(const std::string& policy_name, PolicyMap::Entry& policy) const;

  // Merges the values of |policy| if they come from multiple sources. Keeps
  // track of the original values by leaving them as conflicts. |policy| must
  // remain unchanged if there is nothing to merge.
  void DoMerge(PolicyMap::Entry* policy) const;

  const base::flat_set<std::string> policies_to_merge_;

  DISALLOW_COPY_AND_ASSIGN(PolicyListMerger);
};

// PolicyDictionaryMerger allows the merging of policy dictionaries that have
// multiple sources. Each policy that has to be merged will have its first level
// keys merged into one dictionary, each conflict will be resolved by
// using the key coming from the highest priority source.
class POLICY_EXPORT PolicyDictionaryMerger : public PolicyMerger {
 public:
  explicit PolicyDictionaryMerger(
      base::flat_set<std::string> policies_to_merge);
  ~PolicyDictionaryMerger() override;

  // Merges the dictionary policies from |policies| that have multiple sources.
  void Merge(PolicyMap::PolicyMapType* policies) const override;
  void SetAllowedPoliciesForTesting(
      base::flat_set<std::string> allowed_policies);

 private:
  // Returns True if |policy_name| is in the list of policies to merge and if
  // |policy| has values from different sources that share the same level,
  // target and scope.
  bool CanMerge(const std::string& policy_name, PolicyMap::Entry& policy) const;

  // Merges the values of |policy| if they come from multiple sources. Keeps
  // track of the original values by leaving them as conflicts. |policy| stays
  // intact if there is nothing to merge.
  void DoMerge(PolicyMap::Entry* policy) const;

  const base::flat_set<std::string> policies_to_merge_;
  base::flat_set<std::string> allowed_policies_;

  DISALLOW_COPY_AND_ASSIGN(PolicyDictionaryMerger);
};

// PolicyGroupMerger enforces atomic policy groups. It disables the policies
// from a group that do not share the highest priority from that group.
class POLICY_EXPORT PolicyGroupMerger : public PolicyMerger {
 public:
  PolicyGroupMerger();
  ~PolicyGroupMerger() override;

  // Disables policies from atomic groups that do not share the highest priority
  // from that group.
  void Merge(PolicyMap::PolicyMapType* result) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyGroupMerger);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_
