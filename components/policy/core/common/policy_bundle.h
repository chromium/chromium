// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_BUNDLE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_BUNDLE_H_

#include <map>

#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace policy {

// Maps policy namespaces to PolicyMaps.
class POLICY_EXPORT PolicyBundle {
 public:
  using MapType = std::map<PolicyNamespace, PolicyMap>;
  using iterator = MapType::iterator;
  using const_iterator = MapType::const_iterator;

  PolicyBundle();
  PolicyBundle(const PolicyBundle&) = delete;
  PolicyBundle(PolicyBundle&&);
  PolicyBundle& operator=(const PolicyBundle&) = delete;
  PolicyBundle& operator=(PolicyBundle&&);
  virtual ~PolicyBundle();

  // Returns the PolicyMap for namespace `ns`. Creates a new map if no entry
  // for `ns` is present yet.
  PolicyMap& Get(const PolicyNamespace& ns);

  // Returns the PolicyMap for namespace `ns`. Returns a reference to a static
  // empty map if no entry for `ns` is present.
  const PolicyMap& Get(const PolicyNamespace& ns) const;

  // Create a clone of `this`.
  PolicyBundle Clone() const;

  // Merges the PolicyMaps of |this| with those of |other| for each namespace
  // in common. Also adds copies of the (namespace, PolicyMap) pairs in |other|
  // that don't have an entry in |this|.
  // Each policy in each PolicyMap is replaced only if the policy from |other|
  // has a higher priority.
  // See PolicyMap::MergeFrom for details on merging individual PolicyMaps.
  void MergeFrom(const PolicyBundle& other);

  // Returns true if |other| has the same keys and value as |this|.
  bool Equals(const PolicyBundle& other) const;

  // Returns iterators to the beginning and end of the underlying container.
  iterator begin() { return policy_bundle_.begin(); }
  iterator end() { return policy_bundle_.end(); }

  // These can be used to iterate over and read the PolicyMaps, but not to
  // modify them.
  const_iterator begin() const { return policy_bundle_.begin(); }
  const_iterator end() const { return policy_bundle_.end(); }

  // Erases all the existing pairs.
  void Clear();

 private:
  MapType policy_bundle_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_BUNDLE_H_
