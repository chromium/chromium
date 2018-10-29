// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_bundle.h"

#include "base/logging.h"

namespace policy {

PolicyBundle::PolicyBundle() {}

PolicyBundle::~PolicyBundle() {
  Clear();
}

PolicyMap& PolicyBundle::Get(const PolicyNamespace& ns) {
  DCHECK(ns.domain != POLICY_DOMAIN_CHROME || ns.component_id.empty());
  std::unique_ptr<PolicyMap>& policy = policy_bundle_[ns];
  if (!policy)
    policy = std::make_unique<PolicyMap>();
  return *policy;
}

const PolicyMap& PolicyBundle::Get(const PolicyNamespace& ns) const {
  DCHECK(ns.domain != POLICY_DOMAIN_CHROME || ns.component_id.empty());
  auto it = policy_bundle_.find(ns);
  return it == end() ? kEmpty_ : *it->second;
}

void PolicyBundle::Swap(PolicyBundle* other) {
  policy_bundle_.swap(other->policy_bundle_);
}

void PolicyBundle::CopyFrom(const PolicyBundle& other) {
  Clear();
  for (auto it = other.begin(); it != other.end(); ++it) {
    policy_bundle_[it->first] = it->second->DeepCopy();
  }
}

void PolicyBundle::MergeFrom(const PolicyBundle& other) {
  // Iterate over both |this| and |other| in order; skip what's extra in |this|,
  // add what's missing, and merge the namespaces in common.
  auto it_this = policy_bundle_.begin();
  auto end_this = policy_bundle_.end();
  auto it_other = other.begin();
  auto end_other = other.end();

  while (it_this != end_this && it_other != end_other) {
    if (it_this->first == it_other->first) {
      // Same namespace: merge existing PolicyMaps.
      it_this->second->MergeFrom(*it_other->second);
      ++it_this;
      ++it_other;
    } else if (it_this->first < it_other->first) {
      // |this| has a PolicyMap that |other| doesn't; skip it.
      ++it_this;
    } else if (it_other->first < it_this->first) {
      // |other| has a PolicyMap that |this| doesn't; copy it.
      policy_bundle_[it_other->first] = it_other->second->DeepCopy();
      ++it_other;
    } else {
      NOTREACHED();
    }
  }

  // Add extra PolicyMaps at the end.
  while (it_other != end_other) {
    policy_bundle_[it_other->first] = it_other->second->DeepCopy();
    ++it_other;
  }
}

bool PolicyBundle::Equals(const PolicyBundle& other) const {
  // Equals() has the peculiarity that an entry with an empty PolicyMap equals
  // an non-existent entry. This handles usage of non-const Get() that doesn't
  // insert any policies.
  auto it_this = begin();
  auto it_other = other.begin();

  while (true) {
    // Skip empty PolicyMaps.
    while (it_this != end() && it_this->second->empty())
      ++it_this;
    while (it_other != other.end() && it_other->second->empty())
      ++it_other;
    if (it_this == end() || it_other == other.end())
      break;
    if (it_this->first != it_other->first ||
        !it_this->second->Equals(*it_other->second)) {
      return false;
    }
    ++it_this;
    ++it_other;
  }
  return it_this == end() && it_other == other.end();
}

void PolicyBundle::Clear() {
  policy_bundle_.clear();
}

}  // namespace policy
